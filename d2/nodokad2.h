// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

/*++

Module Name:

    nodokad2.h

Abstract:

    nodokad2 (kbfiltr 型 KMDF キーボードフィルタ + inverted call) の
    内部共有宣言。デバイスコンテキスト、グローバル状態、関数プロトタイプ。

    設計方針 (docs/新ドライバ開発プラン_kbfiltr型_20260709.md):
      - kbdclass の READ IRP には一切触らない。
      - IOCTL_INTERNAL_KEYBOARD_CONNECT を横取りして ClassService コールバックを
        ラップし、キーイベントをコールバック引数で受ける。
      - 素通しがデフォルト。intercept はアプリ接続時の opt-in。
      - キー欠落よりキーボード生存を常に優先 (fail-safe by construction)。

Environment:

    Kernel mode only. KMDF 1.x.

--*/

#ifndef _NODOKAD2_H
#define _NODOKAD2_H

#include <ntddk.h>
#include <wdf.h>
#include <kbdmou.h>
#include <ntddkbd.h>
#include <ntstrsafe.h>

#include "public2.h"

//
// プールタグ 'kdN2' (逆順表示 '2Ndk')
//
#define NODOKA2_POOL_TAG 'kdN2'

//
// グローバルイベントリングバッファのエントリ数。
// 打鍵レートでは 256 で十分。intercept 中にアプリが数百イベント分停止した
// 異常時のみ満杯になり、その場合は素通しへフォールバックする。
//
#define NODOKA2_RING_SIZE 512

//
// フィルタインスタンス (キーボードスタックごと) のデバイスコンテキスト。
//
// 重要: DeviceId は先頭ポインタ 1 個分のオフセットに置く。
// common/fallback.c の TryQueryIdAsync は使用しないが、レイアウト規約
// (LowerDevice 相当の後に DeviceId) を踏襲しておく。KMDF では下位ターゲットは
// フレームワークが保持するため、ここでは WdfDevice のみ持つ。
//
typedef struct _FILTER_CONTEXT {
    WDFDEVICE    WdfDevice;

    // IOCTL_INTERNAL_KEYBOARD_CONNECT で保存した上位 (kbdclass) の
    // コールバックとデバイスオブジェクト。ServiceCallback / 注入で使う。
    CONNECT_DATA UpperConnectData;

    // このキーボードの DeviceId (common/ の Try 連鎖で決定。上位16bit有効)。
    ULONG        DeviceId;

    // ENUM_DEVICES 用に保持する HardwareId 先頭要素 (正規化前の生値)。
    WCHAR        HardwareId[NODOKA2_HWID_MAX];

    // グローバルインスタンスリストへのリンク。
    LIST_ENTRY   ListEntry;

    BOOLEAN      Connected;   // CONNECT フック済みか
    BOOLEAN      Linked;      // g_InstanceList に接続済みか
    BOOLEAN      FirstEventTraced; // 診断: 最初の intercept イベントを 1 回だけトレース済みか
} FILTER_CONTEXT, *PFILTER_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FILTER_CONTEXT, FilterGetContext)

//
// グローバル状態 (nodokad2.c で定義)
//
extern WDFDRIVER  g_Driver;
extern WDFDEVICE  g_ControlDevice;
extern WDFQUEUE   g_PendingQueue;      // GET_EVENTS inverted call 用 manual queue

extern LIST_ENTRY g_InstanceList;      // FILTER_CONTEXT.ListEntry の連結
extern KSPIN_LOCK g_InstanceLock;

// 最後に intercept イベントを出したキーボードの DeviceId (ポインタでなく値で保持)。
// TargetDeviceId=0 の注入は「打っているキーボード」= この DeviceId のインスタンスへ
// 向けることで、RDP/マルチセッション環境でも出力が入力と同じセッションに戻る。
// 値で持つため削除競合による UAF は原理的に起きない (注入時にロック下で生存解決し、
// 見つからなければ先頭へフォールバック)。0 は「未確定」。
extern volatile LONG g_LastActiveDeviceId;

extern volatile LONG g_ClientConnected; // コントロールデバイス open 中か
extern volatile LONG g_InterceptMode;   // NODOKA2_MODE_*

#ifdef NODOKAD2_SUBSCRIPTION
extern volatile LONG g_LicenseValid;    // 刻印を行ってよいか (トークン検証結果)
#endif

//
// nodokad2.c
//
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD Nodoka2EvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL Nodoka2EvtInternalIoctl;
EVT_WDF_OBJECT_CONTEXT_CLEANUP Nodoka2EvtFilterCleanup;

VOID
Nodoka2ServiceCallback(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PKEYBOARD_INPUT_DATA InputDataStart,
    _In_ PKEYBOARD_INPUT_DATA InputDataEnd,
    _Inout_ PULONG InputDataConsumed
    );

VOID Nodoka2ForwardRequest(_In_ WDFREQUEST Request, _In_ WDFDEVICE Device);

//
// control.c
//
NTSTATUS Nodoka2CreateControlDevice(_In_ WDFDRIVER Driver);

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL Nodoka2EvtIoDeviceControl;
EVT_WDF_DEVICE_FILE_CREATE Nodoka2EvtFileCreate;
EVT_WDF_FILE_CLEANUP Nodoka2EvtFileCleanup;

//
// evque.c  — グローバルイベントリング
//
VOID    Nodoka2QueueInit(VOID);
BOOLEAN Nodoka2Enqueue(_In_ ULONG DeviceId, _In_ PKEYBOARD_INPUT_DATA Start, _In_ ULONG Count);
ULONG   Nodoka2Dequeue(_Out_writes_(MaxEvents) PNODOKA2_EVENT Out, _In_ ULONG MaxEvents);
ULONG   Nodoka2RingCount(VOID);
VOID    Nodoka2RingReset(VOID);
VOID    Nodoka2TryCompletePending(VOID);

//
// devid.c  — DeviceId 決定 (common/ の Try 連鎖ラッパ、常時コンパイル)
//
_IRQL_requires_max_(PASSIVE_LEVEL)
ULONG Nodoka2ComputeDeviceId(_In_ PDEVICE_OBJECT Pdo, _In_ ULONG UniqNum);

//
// license/  — サブスク刻印ゲート (NODOKAD2_SUBSCRIPTION 定義時のみ)
//
#ifdef NODOKAD2_SUBSCRIPTION
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN Nodoka2ValidateLicense(VOID);
#endif

//
// デバッグトレース。
// DBG (Debug ビルド) または NODOKAD2_TRACE 定義時に有効。
// dogfooding 期は Release でも NODOKAD2_TRACE を定義して DebugView で追えるようにする。
// トレースは per-key のホットパス (ServiceCallback/enqueue) には入れない方針
// (状態変化・接続・注入エラー等のみ) なので打鍵オーバーヘッドは無い。
//
#if DBG || defined(NODOKAD2_TRACE)
#define N2_TRACE(fmt, ...) \
    DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL, "nodokad2: " fmt, ##__VA_ARGS__)
#else
#define N2_TRACE(fmt, ...) ((void)0)
#endif

#endif // _NODOKAD2_H
