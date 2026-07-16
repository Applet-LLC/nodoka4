// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

/*++

Module Name:

    evque.c

Abstract:

    グローバルイベントリングバッファ + inverted call の pending 完了処理。

    ServiceCallback (DISPATCH_LEVEL) が enqueue し、GET_EVENTS の pending IOCTL を
    完了させる。全キーボードインスタンス共通の 1 本のリングで順序を直列化する。

    ロックは KeAcquireSpinLock を一貫して使用 (呼び出しは <= DISPATCH_LEVEL)。

Environment:

    Kernel mode only.

--*/

#include "nodokad2.h"

//
// 単純なリングバッファ。head==tail は空。満杯は (head+1)%N == tail を避けるため
// 実効容量は NODOKA2_RING_SIZE-1。
//
static NODOKA2_EVENT g_Ring[NODOKA2_RING_SIZE];
static ULONG         g_Head = 0;   // 次に書く位置
static ULONG         g_Tail = 0;   // 次に読む位置
static KSPIN_LOCK    g_RingLock;

VOID
Nodoka2QueueInit(VOID)
{
    KeInitializeSpinLock(&g_RingLock);
    g_Head = 0;
    g_Tail = 0;
}

static ULONG
RingCountLocked(VOID)
{
    if (g_Head >= g_Tail) {
        return g_Head - g_Tail;
    }
    return NODOKA2_RING_SIZE - g_Tail + g_Head;
}

//
// count 件を丸ごと入れられなければ 1 件も入れず FALSE を返す
// (途中まで入れて順序が崩れるのを防ぐ)。
//
BOOLEAN
Nodoka2Enqueue(
    _In_ ULONG DeviceId,
    _In_ PKEYBOARD_INPUT_DATA Start,
    _In_ ULONG Count
    )
{
    KIRQL   irql;
    ULONG   i;
    BOOLEAN ok = FALSE;

    if (Count == 0) {
        return TRUE;
    }

    KeAcquireSpinLock(&g_RingLock, &irql);

    if (RingCountLocked() + Count <= NODOKA2_RING_SIZE - 1) {
        for (i = 0; i < Count; i++) {
            PNODOKA2_EVENT dst = &g_Ring[g_Head];
            dst->DeviceId         = DeviceId;
            dst->UnitId           = Start[i].UnitId;
            dst->MakeCode         = Start[i].MakeCode;
            dst->Flags            = Start[i].Flags;
            dst->Reserved         = Start[i].Reserved;
            dst->ExtraInformation = Start[i].ExtraInformation;
            g_Head = (g_Head + 1) % NODOKA2_RING_SIZE;
        }
        ok = TRUE;
    }

    KeReleaseSpinLock(&g_RingLock, irql);
    return ok;
}

//
// 最大 MaxEvents 件を Out へ取り出し、取り出した件数を返す。
//
ULONG
Nodoka2Dequeue(
    _Out_writes_(MaxEvents) PNODOKA2_EVENT Out,
    _In_ ULONG MaxEvents
    )
{
    KIRQL irql;
    ULONG n = 0;

    KeAcquireSpinLock(&g_RingLock, &irql);

    while (n < MaxEvents && g_Tail != g_Head) {
        Out[n++] = g_Ring[g_Tail];
        g_Tail = (g_Tail + 1) % NODOKA2_RING_SIZE;
    }

    KeReleaseSpinLock(&g_RingLock, irql);
    return n;
}

ULONG
Nodoka2RingCount(VOID)
{
    KIRQL irql;
    ULONG n;

    KeAcquireSpinLock(&g_RingLock, &irql);
    n = RingCountLocked();
    KeReleaseSpinLock(&g_RingLock, irql);
    return n;
}

VOID
Nodoka2RingReset(VOID)
{
    KIRQL irql;

    KeAcquireSpinLock(&g_RingLock, &irql);
    g_Head = 0;
    g_Tail = 0;
    KeReleaseSpinLock(&g_RingLock, irql);
}

//
// pending 中の GET_EVENTS を 1 件取り出し、リングの内容を詰めて完了させる。
// イベント側 (ServiceCallback) とリクエスト側 (GET_EVENTS 受領時) の両方から
// 呼ばれる。WDFQUEUE のretrieve はフレームワークが直列化する。
//
VOID
Nodoka2TryCompletePending(VOID)
{
    NTSTATUS             status;
    WDFREQUEST           request;
    PNODOKA2_EVENTS_OUTPUT out = NULL;
    size_t               outLen = 0;
    ULONG                maxEvents;
    ULONG                n;

    if (g_PendingQueue == NULL) {
        return;
    }

    // リングが空なら完了させるものがない。
    if (Nodoka2RingCount() == 0) {
        return;
    }

    status = WdfIoQueueRetrieveNextRequest(g_PendingQueue, &request);
    if (!NT_SUCCESS(status)) {
        return; // pending リクエスト無し
    }

    status = WdfRequestRetrieveOutputBuffer(request,
                                            FIELD_OFFSET(NODOKA2_EVENTS_OUTPUT, Events) + sizeof(NODOKA2_EVENT),
                                            (PVOID*)&out,
                                            &outLen);
    if (!NT_SUCCESS(status) || out == NULL) {
        WdfRequestComplete(request, NT_SUCCESS(status) ? STATUS_BUFFER_TOO_SMALL : status);
        return;
    }

    maxEvents = (ULONG)((outLen - FIELD_OFFSET(NODOKA2_EVENTS_OUTPUT, Events)) / sizeof(NODOKA2_EVENT));
    if (maxEvents == 0) {
        WdfRequestComplete(request, STATUS_BUFFER_TOO_SMALL);
        return;
    }

    n = Nodoka2Dequeue(out->Events, maxEvents);
    out->Count = n;

    WdfRequestCompleteWithInformation(
        request,
        STATUS_SUCCESS,
        FIELD_OFFSET(NODOKA2_EVENTS_OUTPUT, Events) + (size_t)n * sizeof(NODOKA2_EVENT));
}
