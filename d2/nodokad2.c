// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

/*++

Module Name:

    nodokad2.c

Abstract:

    nodokad2 メイン。DriverEntry / EvtDeviceAdd / CONNECT フック / ServiceCallback。

    kbfiltr 型: IOCTL_INTERNAL_KEYBOARD_CONNECT を横取りして kbdclass の
    ClassService コールバックをラップする。IRP には一切触らない。

Environment:

    Kernel mode only.

--*/

#include "nodokad2.h"

#ifndef DevicePropertyHardwareId
#define DevicePropertyHardwareId 1  // DEVICE_REGISTRY_PROPERTY::DevicePropertyHardwareID
#endif

//
// グローバル状態
//
WDFDRIVER  g_Driver          = NULL;
WDFDEVICE  g_ControlDevice   = NULL;
WDFQUEUE   g_PendingQueue    = NULL;

LIST_ENTRY g_InstanceList;
KSPIN_LOCK g_InstanceLock;
volatile LONG g_LastActiveDeviceId = 0;

volatile LONG g_ClientConnected = 0;
volatile LONG g_InterceptMode   = NODOKA2_MODE_PASSTHROUGH;

#ifdef NODOKAD2_SUBSCRIPTION
volatile LONG g_LicenseValid = 0;
#endif

static LONG g_UniqCounter = 0;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, Nodoka2EvtDeviceAdd)
#pragma alloc_text(PAGE, Nodoka2EvtFilterCleanup)
#endif

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG   config;
    NTSTATUS            status;
    WDF_OBJECT_ATTRIBUTES attributes;

    N2_TRACE("DriverEntry\n");

    InitializeListHead(&g_InstanceList);
    KeInitializeSpinLock(&g_InstanceLock);
    Nodoka2QueueInit();

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    WDF_DRIVER_CONFIG_INIT(&config, Nodoka2EvtDeviceAdd);

    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             &attributes,
                             &config,
                             &g_Driver);
    if (!NT_SUCCESS(status)) {
        N2_TRACE("WdfDriverCreate failed 0x%08X\n", status);
        return status;
    }

    //
    // コントロールデバイス (detour 後継。全体で 1 個) を作成。
    // 失敗してもフィルタ自体はロードさせ、素通しとして機能させる
    // (キーボードを殺さないための保険)。
    //
    status = Nodoka2CreateControlDevice(g_Driver);
    if (!NT_SUCCESS(status)) {
        N2_TRACE("CreateControlDevice failed 0x%08X (continue as passthrough)\n", status);
        // 致命的ではない。素通しフィルタとしてロード継続。
    }

    return STATUS_SUCCESS;
}

//
// g_InstanceList の登録/解除。DISPATCH_LEVEL (スピンロック保持中) で
// 実行されるため、あえて PAGE セクションに置かない (非ページ常駐)。
// EvtDeviceAdd / EvtFilterCleanup 自体は PAGE 指定のため、ロック区間を
// ここへ切り出さないとロック保持中にページアウトされたコードを実行し
// IRQL_NOT_LESS_OR_EQUAL を起こす。
//
static VOID
Nodoka2LinkInstance(
    _In_ PFILTER_CONTEXT ctx
    )
{
    KIRQL irql;
    KeAcquireSpinLock(&g_InstanceLock, &irql);
    InsertTailList(&g_InstanceList, &ctx->ListEntry);
    ctx->Linked = TRUE;
    KeReleaseSpinLock(&g_InstanceLock, irql);
}

static VOID
Nodoka2UnlinkInstance(
    _In_ PFILTER_CONTEXT ctx
    )
{
    KIRQL irql;
    KeAcquireSpinLock(&g_InstanceLock, &irql);
    if (ctx->Linked) {
        RemoveEntryList(&ctx->ListEntry);
        ctx->Linked = FALSE;
    }
    // g_LastActiveDeviceId は値なのでクリア不要 (注入時に生存解決・フォールバックする)。
    KeReleaseSpinLock(&g_InstanceLock, irql);
}

//
// キーボードスタックごとに呼ばれる。フィルタ FDO を生成し、
// 内部 IOCTL キュー (CONNECT 横取り用) を設定する。
//
NTSTATUS
Nodoka2EvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS              status;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFDEVICE             device;
    PFILTER_CONTEXT       ctx;
    WDF_IO_QUEUE_CONFIG   ioQueueConfig;

    UNREFERENCED_PARAMETER(Driver);
    PAGED_CODE();

    //
    // フィルタとして構成 (下位スタックへ自動転送)。
    //
    WdfFdoInitSetFilter(DeviceInit);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, FILTER_CONTEXT);
    attributes.EvtCleanupCallback = Nodoka2EvtFilterCleanup;

    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        N2_TRACE("WdfDeviceCreate failed 0x%08X\n", status);
        return status;
    }

    ctx = FilterGetContext(device);
    RtlZeroMemory(ctx, sizeof(*ctx));
    ctx->WdfDevice = device;

    //
    // DeviceId を決定 (common/ の Try 連鎖。PASSIVE_LEVEL)。
    // 刻印の有無に関わらず ENUM_DEVICES / イベント付与に使う基盤情報。
    //
    {
        PDEVICE_OBJECT pdo = WdfDeviceWdmGetPhysicalDevice(device);
        LONG uniq = InterlockedIncrement(&g_UniqCounter);
        if (pdo != NULL) {
            ctx->DeviceId = Nodoka2ComputeDeviceId(pdo, (ULONG)uniq);

            // ENUM 用に HardwareId 先頭要素を保存 (失敗は無視)。
            {
                WCHAR hwid[NODOKA2_HWID_MAX] = {0};
                ULONG hlen = sizeof(hwid);
                if (NT_SUCCESS(IoGetDeviceProperty(pdo, DevicePropertyHardwareId,
                                                   hlen, hwid, &hlen)) && hwid[0]) {
                    RtlStringCchCopyW(ctx->HardwareId, NODOKA2_HWID_MAX, hwid);
                }
            }
        }
        N2_TRACE("EvtDeviceAdd DeviceId=0x%08X hwid=%ws\n", ctx->DeviceId, ctx->HardwareId);
    }

    //
    // 内部デバイス制御用キュー: CONNECT を横取りする。
    // 既定は Parallel で下位へ転送、CONNECT のみ書き換える。
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchParallel);
    ioQueueConfig.EvtIoInternalDeviceControl = Nodoka2EvtInternalIoctl;

    status = WdfIoQueueCreate(device, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        N2_TRACE("WdfIoQueueCreate failed 0x%08X\n", status);
        return status;
    }

    //
    // グローバルインスタンスリストへ登録 (注入先選択・ENUM 用)。
    //
    Nodoka2LinkInstance(ctx);

    return STATUS_SUCCESS;
}

//
// デバイス破棄時: インスタンスリストから外す。
//
VOID
Nodoka2EvtFilterCleanup(
    _In_ WDFOBJECT Object
    )
{
    PFILTER_CONTEXT ctx = FilterGetContext((WDFDEVICE)Object);

    PAGED_CODE();

    Nodoka2UnlinkInstance(ctx);

    N2_TRACE("FilterCleanup DeviceId=0x%08X\n", ctx->DeviceId);
}

//
// 内部 IOCTL ハンドラ。IOCTL_INTERNAL_KEYBOARD_CONNECT を横取りして
// ClassService を Nodoka2ServiceCallback に差し替える。それ以外は素通し。
//
VOID
Nodoka2EvtInternalIoctl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
    )
{
    WDFDEVICE        device;
    PFILTER_CONTEXT  ctx;
    PCONNECT_DATA    connectData = NULL;
    size_t           length;
    NTSTATUS         status;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    device = WdfIoQueueGetDevice(Queue);
    ctx = FilterGetContext(device);

    if (IoControlCode == IOCTL_INTERNAL_KEYBOARD_CONNECT) {
        //
        // 既に接続済みなら二重接続を拒否 (kbdclass は 1 回のみ CONNECT する)。
        //
        if (ctx->Connected) {
            WdfRequestComplete(Request, STATUS_SHARING_VIOLATION);
            return;
        }

        status = WdfRequestRetrieveInputBuffer(Request,
                                               sizeof(CONNECT_DATA),
                                               (PVOID*)&connectData,
                                               &length);
        if (!NT_SUCCESS(status) || length < sizeof(CONNECT_DATA)) {
            N2_TRACE("CONNECT retrieve failed 0x%08X\n", status);
            WdfRequestComplete(Request, status);
            return;
        }

        //
        // 上位 (kbdclass) のコールバックを保存し、自前に差し替える。
        // ClassDeviceObject は自フィルタの WDM デバイスオブジェクトにする
        // (ServiceCallback の第1引数として戻ってくる)。
        //
        ctx->UpperConnectData = *connectData;
        connectData->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(device);
#pragma warning(suppress: 4152) // 関数ポインタ <-> PVOID
        connectData->ClassService = Nodoka2ServiceCallback;

        ctx->Connected = TRUE;
        N2_TRACE("CONNECT hooked DeviceId=0x%08X\n", ctx->DeviceId);
    }

    //
    // CONNECT を含め下位へ転送 (CONNECT は書き換え済みバッファが流れる)。
    //
    Nodoka2ForwardRequest(Request, device);
}

//
// フィルタの標準パススルー転送 (send-and-forget)。
//
VOID
Nodoka2ForwardRequest(
    _In_ WDFREQUEST Request,
    _In_ WDFDEVICE  Device
    )
{
    WDF_REQUEST_SEND_OPTIONS options;
    BOOLEAN                  sent;
    NTSTATUS                 status;

    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    sent = WdfRequestSend(Request,
                          WdfDeviceGetIoTarget(Device),
                          &options);
    if (!sent) {
        status = WdfRequestGetStatus(Request);
        N2_TRACE("WdfRequestSend failed 0x%08X\n", status);
        WdfRequestComplete(Request, status);
    }
}

//
// kbdclass が ClassService を呼ぶ位置に割り込むコールバック。
// DISPATCH_LEVEL 以下で走る。IRP には触らない。
//
//   - 素通し (既定): 上位コールバックへそのまま転送。
//   - intercept 中 : イベントをリングへ enqueue し飲み込む (上位へ転送しない)。
//   - 刻印 (サブスク有効時のみ): 転送前に ExtraInformation へ DeviceId を書く。
//
// フェイルセーフ: enqueue 失敗 (リング満杯) やクライアント未接続では必ず素通し。
//
VOID
Nodoka2ServiceCallback(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PKEYBOARD_INPUT_DATA InputDataStart,
    _In_ PKEYBOARD_INPUT_DATA InputDataEnd,
    _Inout_ PULONG InputDataConsumed
    )
{
    WDFDEVICE       device;
    PFILTER_CONTEXT ctx;
    ULONG           count;

    device = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);
    if (device == NULL) {
        // 想定外。少なくともキーを落とさないよう何もしない。
        return;
    }
    ctx = FilterGetContext(device);

    count = (ULONG)(InputDataEnd - InputDataStart);

#if DBG || defined(NODOKAD2_TRACE)
    // 診断: 各デバイスから「最初の」イベントだけトレースする (毎キーではない)。
    // RDP キーボード等の入力が実際に ServiceCallback を通っているかの確認用。
    if (!ctx->FirstEventTraced) {
        ctx->FirstEventTraced = TRUE;
        N2_TRACE("first event from DeviceId=0x%08X (connected=%d, clientConnected=%ld, mode=%ld)\n",
                 ctx->DeviceId, ctx->Connected,
                 InterlockedCompareExchange(&g_ClientConnected, 0, 0),
                 InterlockedCompareExchange(&g_InterceptMode, 0, 0));
    }
#endif

#ifdef NODOKAD2_SUBSCRIPTION
    //
    // 刻印: ライセンス有効かつ DeviceId 決定済みのときのみ。
    // 下位16bit は他ソフト用に温存し、上位16bit へ DeviceId を OR。
    //
    if (g_LicenseValid && ctx->DeviceId != 0) {
        PKEYBOARD_INPUT_DATA p;
        for (p = InputDataStart; p < InputDataEnd; p++) {
            p->ExtraInformation =
                (p->ExtraInformation & 0x0000FFFFUL) | (ctx->DeviceId & 0xFFFF0000UL);
        }
    }
#endif

    //
    // intercept 判定 (フェイルセーフ集約点)。
    //
    if (InterlockedCompareExchange(&g_ClientConnected, 0, 0) != 0 &&
        InterlockedCompareExchange(&g_InterceptMode, 0, 0) == NODOKA2_MODE_INTERCEPT) {

        if (Nodoka2Enqueue(ctx->DeviceId, InputDataStart, count)) {
            // 飲み込み成功: 上位へは転送しない。全件消費を報告。
            // 「打っているキーボード」の DeviceId を記録 → TargetDeviceId=0 の注入先にする
            // (RDP/マルチセッションで出力を入力と同じセッションへ戻すため)。値の記録なので安全。
            InterlockedExchange(&g_LastActiveDeviceId, (LONG)ctx->DeviceId);
            *InputDataConsumed = count;
            Nodoka2TryCompletePending();
            return;
        }
        // enqueue 失敗 (満杯) → 素通しへフォールバック (キーボード生存優先)。
    }

    //
    // 素通し: 保存した上位コールバックをそのまま呼ぶ。
    //
    (*(PSERVICE_CALLBACK_ROUTINE)(ULONG_PTR)ctx->UpperConnectData.ClassService)(
        ctx->UpperConnectData.ClassDeviceObject,
        InputDataStart,
        InputDataEnd,
        InputDataConsumed);
}
