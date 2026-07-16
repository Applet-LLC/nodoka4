// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

/*++

Module Name:

    control.c

Abstract:

    コントロールデバイス (detour 後継。全体で 1 個) と、のどか本体アプリとの
    IOCTL プロトコル。inverted call (GET_EVENTS) / 注入 (INJECT) / モード / 列挙。

    排他 open。open 中のみ intercept 可能。ファイルクローズで即座に素通しへ戻す。

Environment:

    Kernel mode only.

--*/

#include "nodokad2.h"

#include <wdmsec.h> // SDDL, WdfControlDeviceInitAllocate 用

//
// コントロールデバイスの SDDL: SYSTEM/Administrators/Everyone/AuthenticatedUsers に
// フルアクセス。現行 detour デバイスの Security 設定を踏襲。
//
DECLARE_CONST_UNICODE_STRING(g_Nodoka2Sddl,
    L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;WD)(A;;GA;;;AU)");

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Nodoka2CreateControlDevice)
#endif

NTSTATUS
Nodoka2CreateControlDevice(
    _In_ WDFDRIVER Driver
    )
{
    PWDFDEVICE_INIT       init = NULL;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_IO_QUEUE_CONFIG   ioQueueConfig;
    WDF_FILEOBJECT_CONFIG fileConfig;
    WDFDEVICE             controlDevice = NULL;
    WDFQUEUE              defaultQueue;
    NTSTATUS              status;

    DECLARE_CONST_UNICODE_STRING(ntName, NODOKA2_DEVICE_NAME_W);
    DECLARE_CONST_UNICODE_STRING(symLink, NODOKA2_SYMLINK_NAME_W);

    PAGED_CODE();

    init = WdfControlDeviceInitAllocate(Driver, &g_Nodoka2Sddl);
    if (init == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    WdfDeviceInitSetDeviceType(init, FILE_DEVICE_KEYBOARD);
    WdfDeviceInitSetCharacteristics(init, FILE_DEVICE_SECURE_OPEN, FALSE);
    WdfDeviceInitSetExclusive(init, TRUE); // 排他 open (アプリ 1 個)

    status = WdfDeviceInitAssignName(init, &ntName);
    if (!NT_SUCCESS(status)) {
        goto Fail;
    }

    //
    // ファイルオブジェクトコールバック (open/close でクライアント接続状態を管理)。
    //
    WDF_FILEOBJECT_CONFIG_INIT(&fileConfig,
                               Nodoka2EvtFileCreate,
                               NULL, // EvtFileClose
                               Nodoka2EvtFileCleanup);
    WdfDeviceInitSetFileObjectConfig(init, &fileConfig, WDF_NO_OBJECT_ATTRIBUTES);

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    status = WdfDeviceCreate(&init, &attributes, &controlDevice);
    if (!NT_SUCCESS(status)) {
        // WdfDeviceCreate 失敗時は init はフレームワークが解放済み。
        init = NULL;
        goto Fail;
    }
    init = NULL; // 以降 WdfDeviceCreate が所有

    status = WdfDeviceCreateSymbolicLink(controlDevice, &symLink);
    if (!NT_SUCCESS(status)) {
        goto Fail;
    }

    //
    // 既定キュー: アプリからの IOCTL を処理。
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchParallel);
    ioQueueConfig.EvtIoDeviceControl = Nodoka2EvtIoDeviceControl;

    status = WdfIoQueueCreate(controlDevice, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, &defaultQueue);
    if (!NT_SUCCESS(status)) {
        goto Fail;
    }

    //
    // GET_EVENTS 用 manual キュー (inverted call の pending 置き場)。
    // ファイルオブジェクトが閉じられると WDF が該当リクエストを自動キャンセルする。
    //
    WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig, WdfIoQueueDispatchManual);
    status = WdfIoQueueCreate(controlDevice, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, &g_PendingQueue);
    if (!NT_SUCCESS(status)) {
        goto Fail;
    }

    WdfControlFinishInitializing(controlDevice);

    g_ControlDevice = controlDevice;
    N2_TRACE("ControlDevice created\n");
    return STATUS_SUCCESS;

Fail:
    if (init != NULL) {
        WdfDeviceInitFree(init);
    }
    N2_TRACE("CreateControlDevice failed 0x%08X\n", status);
    return status;
}

//
// アプリが CreateFile したとき。排他デバイスなので同時 open は 1 個。
//
VOID
Nodoka2EvtFileCreate(
    _In_ WDFDEVICE     Device,
    _In_ WDFREQUEST    Request,
    _In_ WDFFILEOBJECT FileObject
    )
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(FileObject);

    InterlockedExchange(&g_ClientConnected, 1);
    // 既定は素通し。アプリが SET_MODE で明示的に intercept を有効化する。
    InterlockedExchange(&g_InterceptMode, NODOKA2_MODE_PASSTHROUGH);
    Nodoka2RingReset();

    N2_TRACE("Client connected\n");
    WdfRequestComplete(Request, STATUS_SUCCESS);
}

//
// アプリのハンドルが閉じた (正常/異常問わず)。即座に素通しへ戻す。
// pending GET_EVENTS は WDF が自動キャンセルする。
//
VOID
Nodoka2EvtFileCleanup(
    _In_ WDFFILEOBJECT FileObject
    )
{
    UNREFERENCED_PARAMETER(FileObject);

    InterlockedExchange(&g_InterceptMode, NODOKA2_MODE_PASSTHROUGH);
    InterlockedExchange(&g_ClientConnected, 0);
    Nodoka2RingReset();

    N2_TRACE("Client disconnected -> passthrough\n");
}

//
// 注入: TargetDeviceId のインスタンスの上位コールバックを合成データで呼ぶ。
//
static NTSTATUS
Nodoka2DoInject(
    _In_reads_bytes_(InLen) PNODOKA2_INJECT_INPUT In,
    _In_ size_t InLen
    )
{
    CONNECT_DATA target;
    BOOLEAN      found = FALSE;
    KIRQL        irql;
    PLIST_ENTRY  entry;
    ULONG        i;
    ULONG        count;
    PKEYBOARD_INPUT_DATA kdata;
    ULONG        consumed = 0;

    if (InLen < FIELD_OFFSET(NODOKA2_INJECT_INPUT, Events)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    count = In->Count;
    if (count == 0) {
        return STATUS_SUCCESS;
    }
    // 入力バッファに Count 件が収まっているか検証。
    if (InLen < FIELD_OFFSET(NODOKA2_INJECT_INPUT, Events) + (size_t)count * sizeof(NODOKA2_EVENT)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // 宛先インスタンスの CONNECT_DATA をロック下でコピー。
    // TargetDeviceId==0 は最初の生存インスタンス。
    //
    RtlZeroMemory(&target, sizeof(target));
    KeAcquireSpinLock(&g_InstanceLock, &irql);
    for (entry = g_InstanceList.Flink; entry != &g_InstanceList; entry = entry->Flink) {
        PFILTER_CONTEXT c = CONTAINING_RECORD(entry, FILTER_CONTEXT, ListEntry);
        if (!c->Connected) {
            continue;
        }
        if (In->TargetDeviceId == 0 || c->DeviceId == In->TargetDeviceId) {
            target = c->UpperConnectData;
            found = TRUE;
            break;
        }
    }
    KeReleaseSpinLock(&g_InstanceLock, irql);

    if (!found || target.ClassService == NULL) {
        N2_TRACE("INJECT: no target instance (TargetDeviceId=0x%08X)\n", In->TargetDeviceId);
        return STATUS_DEVICE_NOT_CONNECTED;
    }

    //
    // NODOKA2_EVENT -> KEYBOARD_INPUT_DATA へ変換 (非ページプール)。
    //
    kdata = (PKEYBOARD_INPUT_DATA)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, (size_t)count * sizeof(KEYBOARD_INPUT_DATA), NODOKA2_POOL_TAG);
    if (kdata == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    for (i = 0; i < count; i++) {
        kdata[i].UnitId           = In->Events[i].UnitId;
        kdata[i].MakeCode         = In->Events[i].MakeCode;
        kdata[i].Flags            = In->Events[i].Flags;
        kdata[i].Reserved         = In->Events[i].Reserved;
        kdata[i].ExtraInformation = In->Events[i].ExtraInformation;
#ifdef NODOKAD2_SUBSCRIPTION
        // 注入キーにも由来キーボードの DeviceId を刻印 (ライセンス有効時)。
        if (g_LicenseValid && In->TargetDeviceId != 0) {
            kdata[i].ExtraInformation =
                (kdata[i].ExtraInformation & 0x0000FFFFUL) | (In->TargetDeviceId & 0xFFFF0000UL);
        }
#endif
    }

    //
    // 上位 (kbdclass) コールバックを DISPATCH_LEVEL で呼ぶ。
    //
    {
        KIRQL old;
        KeRaiseIrql(DISPATCH_LEVEL, &old);
        (*(PSERVICE_CALLBACK_ROUTINE)(ULONG_PTR)target.ClassService)(
            target.ClassDeviceObject,
            kdata,
            kdata + count,
            &consumed);
        KeLowerIrql(old);
    }

    ExFreePoolWithTag(kdata, NODOKA2_POOL_TAG);
    return STATUS_SUCCESS;
}

//
// ENUM_DEVICES: 生存インスタンス一覧を出力。
//
static NTSTATUS
Nodoka2DoEnum(
    _Out_writes_bytes_(OutLen) PNODOKA2_ENUM_OUTPUT Out,
    _In_ size_t OutLen,
    _Out_ size_t* Written
    )
{
    KIRQL       irql;
    PLIST_ENTRY entry;
    ULONG       maxDev;
    ULONG       n = 0;

    *Written = 0;

    if (OutLen < FIELD_OFFSET(NODOKA2_ENUM_OUTPUT, Devices) + sizeof(NODOKA2_DEVICE_INFO)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    maxDev = (ULONG)((OutLen - FIELD_OFFSET(NODOKA2_ENUM_OUTPUT, Devices)) / sizeof(NODOKA2_DEVICE_INFO));

    KeAcquireSpinLock(&g_InstanceLock, &irql);
    for (entry = g_InstanceList.Flink; entry != &g_InstanceList && n < maxDev; entry = entry->Flink) {
        PFILTER_CONTEXT c = CONTAINING_RECORD(entry, FILTER_CONTEXT, ListEntry);
        Out->Devices[n].DeviceId = c->DeviceId;
        RtlStringCchCopyW(Out->Devices[n].HardwareId, NODOKA2_HWID_MAX, c->HardwareId);
        n++;
    }
    KeReleaseSpinLock(&g_InstanceLock, irql);

    Out->Count = n;
    *Written = FIELD_OFFSET(NODOKA2_ENUM_OUTPUT, Devices) + (size_t)n * sizeof(NODOKA2_DEVICE_INFO);
    return STATUS_SUCCESS;
}

//
// アプリからの IOCTL ディスパッチ。
//
VOID
Nodoka2EvtIoDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
    )
{
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    size_t   info = 0;
    PVOID    inBuf = NULL;
    PVOID    outBuf = NULL;
    size_t   len = 0;

    UNREFERENCED_PARAMETER(Queue);

    switch (IoControlCode) {

    case IOCTL_NODOKA2_GET_EVENTS:
        //
        // inverted call: 一旦 pending キューへ入れ、直後にリング内容を
        // 排出試行する。イベントが既にあれば即完了、無ければ待機。
        // enqueue 側 (ServiceCallback) も同じ排出関数を呼ぶためレース安全。
        //
        status = WdfRequestForwardToIoQueue(Request, g_PendingQueue);
        if (!NT_SUCCESS(status)) {
            break; // 下で complete
        }
        Nodoka2TryCompletePending();
        return; // pending / 完了済みのため戻る

    case IOCTL_NODOKA2_INJECT:
        status = WdfRequestRetrieveInputBuffer(Request, FIELD_OFFSET(NODOKA2_INJECT_INPUT, Events), &inBuf, &len);
        if (NT_SUCCESS(status)) {
            status = Nodoka2DoInject((PNODOKA2_INJECT_INPUT)inBuf, len);
        }
        break;

    case IOCTL_NODOKA2_SET_MODE:
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(ULONG), &inBuf, &len);
        if (NT_SUCCESS(status) && len >= sizeof(ULONG)) {
            ULONG mode = *(ULONG*)inBuf;
            InterlockedExchange(&g_InterceptMode,
                                (mode == NODOKA2_MODE_INTERCEPT) ? NODOKA2_MODE_INTERCEPT
                                                                 : NODOKA2_MODE_PASSTHROUGH);
            if (mode != NODOKA2_MODE_INTERCEPT) {
                Nodoka2RingReset();
            }
            N2_TRACE("SET_MODE mode=%lu (clientConnected=%ld)\n",
                     mode, InterlockedCompareExchange(&g_ClientConnected, 0, 0));
            status = STATUS_SUCCESS;
        } else if (NT_SUCCESS(status)) {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_NODOKA2_ENUM_DEVICES:
        status = WdfRequestRetrieveOutputBuffer(Request,
                                                FIELD_OFFSET(NODOKA2_ENUM_OUTPUT, Devices) + sizeof(NODOKA2_DEVICE_INFO),
                                                &outBuf, &len);
        if (NT_SUCCESS(status)) {
            status = Nodoka2DoEnum((PNODOKA2_ENUM_OUTPUT)outBuf, len, &info);
        }
        break;

    case IOCTL_NODOKA2_GET_VERSION:
        status = WdfRequestRetrieveOutputBuffer(Request, sizeof(ULONG), &outBuf, &len);
        if (NT_SUCCESS(status) && len >= sizeof(ULONG)) {
            *(ULONG*)outBuf = NODOKA2_PROTOCOL_VERSION;
            info = sizeof(ULONG);
            status = STATUS_SUCCESS;
        } else if (NT_SUCCESS(status)) {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_NODOKA2_REFRESH_LICENSE:
#ifdef NODOKAD2_SUBSCRIPTION
        InterlockedExchange(&g_LicenseValid, Nodoka2ValidateLicense() ? 1 : 0);
        status = STATUS_SUCCESS;
#else
        status = STATUS_NOT_SUPPORTED;
#endif
        break;

    default:
        break;
    }

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    WdfRequestCompleteWithInformation(Request, status, info);
}
