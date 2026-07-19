// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

/*++

Module Name:

    sigcheck.h

Abstract:

    呼び出し元プロセスの実行イメージ署名 (CI キャッシュ済みサインレベル /
    リーフ証明書サムプリント) を調べる共通ヘルパー。d / d2 両ドライバから
    ../common/sigcheck.h として参照される。

    2026-07-19 実機検証で、SeGetCachedSigningLevel は WDAC/Smart App Control/
    PPL 等の強制ポリシーが無い通常の Windows では STATUS_NOT_FOUND となり
    単独ではアクセス制御のゲートにできないと判明した (docs/driver-access-
    control-plan.md §0 参照)。よって本ヘルパーは「取れれば使う」ベスト
    エフォートの早期パス (NodokaSigCheckFastPath) と位置づけ、TRUE を返した
    場合のみ即認証扱いにしてよい。FALSE (キャッシュ無し/不一致いずれも) は
    "未認証" であって "拒否" ではない -- 呼び出し側は必ず L3
    (../common/authchallenge.h の challenge-response) にフォールバックする
    こと。単独で STATUS_ACCESS_DENIED を出すために使ってはならない。

    NodokaSigCheckTrace は診断専用 (旧 API、ACCESS_DENIED を返さず結果を
    デバッグ出力へ書き出すだけ) として残す。

Environment:

    Kernel mode only. Call at PASSIVE_LEVEL, caller's thread context
    (IRP_MJ_CREATE / WDF EvtFileCreate; NOT after KeAcquireSpinLock).

--*/

#ifndef _NODOKA_SIGCHECK_H
#define _NODOKA_SIGCHECK_H

#include <ntddk.h>

// PsReferenceProcessFilePointer / SeGetCachedSigningLevel は ntoskrnl の
// エクスポート関数だが、このリポジトリが使う WDK
// (K:\Program Files\Windows Kits\10, 10.0.26100.0) の公開ヘッダ
// (km\ntddk.h, km\ntifs.h) には宣言が無い (確認済み: SE_SIGNING_LEVEL 等の
// 定数は ntddk.h にあるが関数プロトタイプは無い)。準ドキュメント API のため
// 自前で extern 宣言する。シグネチャは MS 公開ドキュメント準拠。
#ifndef _NODOKA_SIGCHECK_EXTERNS_DECLARED
#define _NODOKA_SIGCHECK_EXTERNS_DECLARED

NTKERNELAPI
NTSTATUS
PsReferenceProcessFilePointer(
    _In_ PEPROCESS Process,
    _Outptr_ PFILE_OBJECT *FileObject
    );

NTKERNELAPI
NTSTATUS
SeGetCachedSigningLevel(
    _In_ PFILE_OBJECT File,
    _Out_ PULONG Flags,
    _Out_ PSE_SIGNING_LEVEL SigningLevel,
    _Out_writes_bytes_all_opt_(*ThumbprintSize) PUCHAR Thumbprint,
    _Inout_opt_ PULONG ThumbprintSize,
    _Out_opt_ PULONG ThumbprintAlgorithm
    );

#endif // _NODOKA_SIGCHECK_EXTERNS_DECLARED

// 呼び出し元プロセスの署名キャッシュ状態を調べ、デバッグ出力へダンプする。
// 診断専用: 戻り値もアクセス制御的な意味は持たない (TRUE = 情報を取得できた)。
// PASSIVE_LEVEL・呼び出し元スレッド文脈専用。
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
NodokaSigCheckTrace(
    _In_opt_ PCSTR Tag
    );

// L2 ベストエフォート早期パス。呼び出し元プロセスの実行イメージについて
// CI のキャッシュ済みサインレベルが Authenticode 以上、かつリーフ証明書の
// サムプリントが埋め込みベンダー証明書と一致する場合のみ TRUE。
// それ以外 (キャッシュ無し・レベル不足・不一致) は FALSE ("未認証"、
// "拒否" ではない -- 呼び出し側は L3 へフォールバックすること)。
// PASSIVE_LEVEL・呼び出し元スレッド文脈専用。
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
NodokaSigCheckFastPath(VOID);

#endif // _NODOKA_SIGCHECK_H
