// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

/*++

Module Name:

    control.c

Abstract:

    コントロールデバイス (detour 後継。全体で 1 個) と、のどか本体アプリとの
    IOCTL プロトコル。inverted call (GET_EVENTS) / 注入 (INJECT) / モード / 列挙。

    排他 open。open 中のみ intercept 可能。ファイルクローズで即座に素通しへ戻す。

    呼び出し元認証 (docs/driver-access-control-plan.md): デバイス自体の DACL は
    一般ユーザーに開放したまま (クライアント非特権を維持)、GET_EVENTS/INJECT/
    SET_MODE は「認証済み」ハンドルのみ許可する。認証は 2 段構え:
      - L2 (ベストエフォート早期パス): open 直後に NodokaSigCheckFastPath() で
        CI キャッシュ済み署名を確認できれば即認証。WDAC 等が無い通常環境では
        まず成立しない (2026-07-19 実機確認済み)。
      - L3 (必須フォールバック): IOCTL_NODOKA2_AUTH_BEGIN で nonce を発行し、
        クライアントがアプリ埋め込みの ECDSA 秘密鍵で署名、
        IOCTL_NODOKA2_AUTH_RESPONSE で提出・検証。成功して初めて認証済みになる。
    未認証のまま GET_EVENTS/INJECT/SET_MODE を呼ぶと STATUS_ACCESS_DENIED。

Environment:

    Kernel mode only.

--*/

#include "nodokad2.h"

#include <wdmsec.h> // SDDL, WdfControlDeviceInitAllocate 用
#include "..\common\sigcheck.h"       // L2 署名検証ベストエフォート早期パス
#include "..\common\authchallenge.h"  // L3 challenge-response (必須フォールバック)

//
// コントロールデバイスの SDDL: SYSTEM/Administrators/Everyone/AuthenticatedUsers に
// フルアクセス。現行 detour デバイスの Security 設定を踏襲。
// (呼び出し元の識別は DACL ではなく L2/L3 で行うため、ここは緩いままでよい。
// docs/driver-access-control-plan.md §4 「(参考) レイヤー0/1」参照。)
//
DECLARE_CONST_UNICODE_STRING(g_Nodoka2Sddl,
    L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;WD)(A;;GA;;;AU)");

//
// L3 challenge-response の状態。コントロールデバイスは排他 open (同時に
// 1 ハンドルのみ) なので、ハンドル単位ではなくドライバ全体でグローバルに
// 持ってよい (g_ClientConnected 等と同じ設計)。既定キューは Parallel
// dispatch なので AUTH_BEGIN/AUTH_RESPONSE 間の状態更新は g_AuthLock で守る。
//
#define NODOKA2_AUTH_NONCE_TIMEOUT_100NS (5 * 10000000ULL) // 5秒: nonce の有効期限

static volatile LONG g_ClientAuthenticated; // L2 fast-path または L3 で認証済みか
static volatile LONG g_AuthChallengeIssued; // AUTH_BEGIN 発行済み・未消費(1回限り)
static UCHAR         g_AuthNonce[NODOKA2_AUTH_NONCE_SIZE];
static ULONGLONG     g_AuthNonceTick; // KeQueryInterruptTime() 発行時刻
static KSPIN_LOCK    g_AuthLock;

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

    KeInitializeSpinLock(&g_AuthLock);

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
    KIRQL irql;
    BOOLEAN fastPathOk;

    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(FileObject);

    // 未認証状態にリセット (L3 必須)。
    KeAcquireSpinLock(&g_AuthLock, &irql);
    g_AuthChallengeIssued = FALSE;
    RtlZeroMemory(g_AuthNonce, sizeof(g_AuthNonce));
    KeReleaseSpinLock(&g_AuthLock, irql);
    InterlockedExchange(&g_ClientAuthenticated, FALSE);

    // L2 ベストエフォート早期パス。成功すれば即認証、失敗しても未認証のまま
    // (拒否ではない) -- クライアントは続けて AUTH_BEGIN/AUTH_RESPONSE (L3) へ
    // フォールバックする。
    fastPathOk = NodokaSigCheckFastPath();
    if (fastPathOk) {
        InterlockedExchange(&g_ClientAuthenticated, TRUE);
        N2_TRACE("Client connected (L2 fast-path authenticated)\n");
    } else {
        N2_TRACE("Client connected (unauthenticated -- L3 challenge-response required)\n");
    }

    InterlockedExchange(&g_ClientConnected, 1);
    // 既定は素通し。アプリが SET_MODE で明示的に intercept を有効化する。
    InterlockedExchange(&g_InterceptMode, NODOKA2_MODE_PASSTHROUGH);
    Nodoka2RingReset();

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
    KIRQL irql;

    UNREFERENCED_PARAMETER(FileObject);

    InterlockedExchange(&g_InterceptMode, NODOKA2_MODE_PASSTHROUGH);
    InterlockedExchange(&g_ClientConnected, 0);
    InterlockedExchange(&g_ClientAuthenticated, FALSE);
    KeAcquireSpinLock(&g_AuthLock, &irql);
    g_AuthChallengeIssued = FALSE;
    RtlZeroMemory(g_AuthNonce, sizeof(g_AuthNonce));
    KeReleaseSpinLock(&g_AuthLock, irql);
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
    ULONG        targetDeviceId = 0;  // 実際に注入した宛先の DeviceId (刻印・トレース用)

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
    //   - TargetDeviceId != 0: その DeviceId のインスタンス (明示ルーティング)。
    //   - TargetDeviceId == 0: 「打っているキーボード」= g_LastActiveDeviceId のインスタンスを
    //     優先。生存していなければ最初の生存インスタンスにフォールバック。
    //     これで RDP/マルチセッション環境でも出力が入力と同じセッションへ戻る。
    // すべてロック下で g_InstanceList を走査して解決するため UAF は起きない。
    //
    {
        LONG lastActive = InterlockedCompareExchange(&g_LastActiveDeviceId, 0, 0);
        ULONG wantId = (In->TargetDeviceId != 0) ? In->TargetDeviceId
                     : (lastActive != 0) ? (ULONG)lastActive : 0;

        RtlZeroMemory(&target, sizeof(target));
        KeAcquireSpinLock(&g_InstanceLock, &irql);

        // まず wantId (明示 or last-active) に一致する生存インスタンスを探す。
        if (wantId != 0) {
            for (entry = g_InstanceList.Flink; entry != &g_InstanceList; entry = entry->Flink) {
                PFILTER_CONTEXT c = CONTAINING_RECORD(entry, FILTER_CONTEXT, ListEntry);
                if (c->Connected && c->DeviceId == wantId) {
                    target = c->UpperConnectData;
                    targetDeviceId = c->DeviceId;
                    found = TRUE;
                    break;
                }
            }
        }

        // 見つからない & TargetDeviceId==0 なら最初の生存インスタンスへフォールバック。
        if (!found && In->TargetDeviceId == 0) {
            for (entry = g_InstanceList.Flink; entry != &g_InstanceList; entry = entry->Flink) {
                PFILTER_CONTEXT c = CONTAINING_RECORD(entry, FILTER_CONTEXT, ListEntry);
                if (c->Connected) {
                    target = c->UpperConnectData;
                    targetDeviceId = c->DeviceId;
                    found = TRUE;
                    break;
                }
            }
        }

        KeReleaseSpinLock(&g_InstanceLock, irql);
    }

    if (!found || target.ClassService == NULL) {
        N2_TRACE("INJECT: no target instance (TargetDeviceId=0x%08X)\n", In->TargetDeviceId);
        return STATUS_DEVICE_NOT_CONNECTED;
    }
    N2_TRACE("INJECT: %lu key(s) -> DeviceId=0x%08X (requested 0x%08X)\n",
             count, targetDeviceId, In->TargetDeviceId);

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
        // 注入キーにも実際の宛先キーボードの DeviceId を刻印 (ライセンス有効時)。
        if (g_LicenseValid && targetDeviceId != 0) {
            kdata[i].ExtraInformation =
                (kdata[i].ExtraInformation & 0x0000FFFFUL) | (targetDeviceId & 0xFFFF0000UL);
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
// AUTH_BEGIN: L3 challenge を新規発行する。単発消費 (前回分は上書き/破棄)。
//
static NTSTATUS
Nodoka2DoAuthBegin(
    _Out_writes_bytes_(NODOKA2_AUTH_NONCE_SIZE) PUCHAR Out
    )
{
    UCHAR nonce[NODOKA2_AUTH_NONCE_SIZE];
    KIRQL irql;

    if (!NodokaAuthGenerateNonce(nonce)) {
        return STATUS_UNSUCCESSFUL;
    }

    KeAcquireSpinLock(&g_AuthLock, &irql);
    RtlCopyMemory(g_AuthNonce, nonce, sizeof(g_AuthNonce));
    g_AuthNonceTick = KeQueryInterruptTime();
    g_AuthChallengeIssued = TRUE;
    KeReleaseSpinLock(&g_AuthLock, irql);

    RtlCopyMemory(Out, nonce, NODOKA2_AUTH_NONCE_SIZE);
    N2_TRACE("AUTH_BEGIN: nonce issued\n");
    return STATUS_SUCCESS;
}

//
// AUTH_RESPONSE: 直前の AUTH_BEGIN で発行した nonce への署名を検証する。
// 成功/失敗を問わず challenge は消費する (同じ nonce への再挑戦不可 ==
// 総当たり対策。失敗時は AUTH_BEGIN からやり直す)。
//
static NTSTATUS
Nodoka2DoAuthResponse(
    _In_reads_bytes_(NODOKA2_AUTH_SIG_SIZE) const UCHAR *Signature
    )
{
    UCHAR     nonce[NODOKA2_AUTH_NONCE_SIZE];
    KIRQL     irql;
    BOOLEAN   hadChallenge;
    ULONGLONG issuedTick;
    ULONGLONG now;

    KeAcquireSpinLock(&g_AuthLock, &irql);
    hadChallenge = (g_AuthChallengeIssued != 0);
    issuedTick = g_AuthNonceTick;
    RtlCopyMemory(nonce, g_AuthNonce, sizeof(nonce));
    g_AuthChallengeIssued = FALSE;
    RtlZeroMemory(g_AuthNonce, sizeof(g_AuthNonce));
    KeReleaseSpinLock(&g_AuthLock, irql);

    if (!hadChallenge) {
        N2_TRACE("AUTH_RESPONSE: no outstanding challenge\n");
        return STATUS_ACCESS_DENIED;
    }

    now = KeQueryInterruptTime();
    if (now - issuedTick > NODOKA2_AUTH_NONCE_TIMEOUT_100NS) {
        N2_TRACE("AUTH_RESPONSE: challenge expired\n");
        return STATUS_ACCESS_DENIED;
    }

    if (!NodokaAuthVerifyResponse(nonce, Signature)) {
        N2_TRACE("AUTH_RESPONSE: signature verify failed\n");
        return STATUS_ACCESS_DENIED;
    }

    InterlockedExchange(&g_ClientAuthenticated, TRUE);
    N2_TRACE("AUTH_RESPONSE: OK -- client authenticated\n");
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

    case IOCTL_NODOKA2_AUTH_BEGIN:
        status = WdfRequestRetrieveOutputBuffer(Request, NODOKA2_AUTH_NONCE_SIZE, &outBuf, &len);
        if (NT_SUCCESS(status)) {
            status = Nodoka2DoAuthBegin((PUCHAR)outBuf);
            if (NT_SUCCESS(status)) {
                info = NODOKA2_AUTH_NONCE_SIZE;
            }
        }
        break;

    case IOCTL_NODOKA2_AUTH_RESPONSE:
        status = WdfRequestRetrieveInputBuffer(Request, NODOKA2_AUTH_SIG_SIZE, &inBuf, &len);
        if (NT_SUCCESS(status)) {
            status = Nodoka2DoAuthResponse((const UCHAR*)inBuf);
        }
        break;

    case IOCTL_NODOKA2_GET_EVENTS:
        // L3: 未認証ハンドルからの sniff は拒否 (docs/driver-access-control-plan.md)。
        if (!InterlockedCompareExchange(&g_ClientAuthenticated, 0, 0)) {
            status = STATUS_ACCESS_DENIED;
            break;
        }
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
        // L3: 未認証ハンドルからの inject は拒否。
        if (!InterlockedCompareExchange(&g_ClientAuthenticated, 0, 0)) {
            status = STATUS_ACCESS_DENIED;
            break;
        }
        status = WdfRequestRetrieveInputBuffer(Request, FIELD_OFFSET(NODOKA2_INJECT_INPUT, Events), &inBuf, &len);
        if (NT_SUCCESS(status)) {
            status = Nodoka2DoInject((PNODOKA2_INJECT_INPUT)inBuf, len);
        }
        break;

    case IOCTL_NODOKA2_SET_MODE:
        // L3: 未認証ハンドルは INTERCEPT へ切り替えられない (PASSTHROUGH は
        // 既定・無害だが、単純化のため認証必須で統一する)。
        if (!InterlockedCompareExchange(&g_ClientAuthenticated, 0, 0)) {
            status = STATUS_ACCESS_DENIED;
            break;
        }
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
