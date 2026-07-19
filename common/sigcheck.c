// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#include "sigcheck.h"

#define NODOKA_SIG_THUMBPRINT_MAX 64

// ビルド構成に関わらず常に出力する (DBG/NODOKAD*_TRACE に依存しない)。
// ホットパス外 (open 時 1 回) なので常時ログでもコストは無視できる。
#define NODOKA_SIG_LOG(fmt, ...) \
    DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL, "nodoka-sig: " fmt, ##__VA_ARGS__)

//
// ベンダー EV 証明書のリーフ・サムプリント (SHA-1, 20 バイト)。
// nodoka\x64\Release\nodoka64.exe (2026-07-16 ビルド, CN=Applet LLC) を
// Get-AuthenticodeSignature で確認した値: 0AF36F94140C861D17147C17334866BD3B581A87
//
// 注意: SeGetCachedSigningLevel が実際に成功したケースをまだ一度も観測できて
// いない (2026-07-19 時点、通常環境では STATUS_NOT_FOUND)。よってこの定数が
// SeGetCachedSigningLevel の返す thumbprint と同じバイト列になるかは未検証。
// 不一致でも害はない (NodokaSigCheckFastPath は FALSE を返すだけで、呼び出し
// 側は必ず L3 へフォールバックする)。WDAC 等が有効な実機で一致を確認できた
// 時点でこのコメントを更新すること。EV 証明書更新時は本値も合わせて更新し、
// アプリと同時に再署名・再ビルドすること (docs/driver-access-control-plan.md
// §4 の運用ルール参照)。
//
static const UCHAR g_VendorThumbprint[20] = {
    0x0A, 0xF3, 0x6F, 0x94, 0x14, 0x0C, 0x86, 0x1D, 0x17, 0x14,
    0x7C, 0x17, 0x33, 0x48, 0x66, 0xBD, 0x3B, 0x58, 0x1A, 0x87
};

// PsReferenceProcessFilePointer + SeGetCachedSigningLevel を呼び、成功時は
// flags/signingLevel/thumbprint を出力する共通ヘルパー。
// 戻り値: SeGetCachedSigningLevel まで成功したら TRUE。
static BOOLEAN
NodokaSigCheckQuery(
    _Out_ PULONG Flags,
    _Out_ PSE_SIGNING_LEVEL SigningLevel,
    _Out_writes_bytes_all_(NODOKA_SIG_THUMBPRINT_MAX) PUCHAR Thumbprint,
    _Out_ PULONG ThumbprintSize,
    _Out_ PULONG ThumbprintAlgorithm,
    _In_opt_ PCSTR Tag
    )
{
    NTSTATUS     status;
    PFILE_OBJECT fileObject = NULL;
    BOOLEAN      ok = FALSE;
    PCSTR        tag = (Tag != NULL) ? Tag : "?";

    *Flags = 0;
    *SigningLevel = SE_SIGNING_LEVEL_UNCHECKED;
    RtlZeroMemory(Thumbprint, NODOKA_SIG_THUMBPRINT_MAX);
    *ThumbprintSize = NODOKA_SIG_THUMBPRINT_MAX;
    *ThumbprintAlgorithm = 0;

    status = PsReferenceProcessFilePointer(PsGetCurrentProcess(), &fileObject);
    if (!NT_SUCCESS(status)) {
        NODOKA_SIG_LOG("[%s] PsReferenceProcessFilePointer failed 0x%08X (pid=%p)\n",
            tag, status, PsGetCurrentProcessId());
        return FALSE;
    }

    status = SeGetCachedSigningLevel(fileObject, Flags, SigningLevel,
        Thumbprint, ThumbprintSize, ThumbprintAlgorithm);
    if (!NT_SUCCESS(status)) {
        NODOKA_SIG_LOG("[%s] SeGetCachedSigningLevel failed 0x%08X (pid=%p) -- "
            "CI has no cached signature for this image (expected on a plain "
            "launch without WDAC/Smart App Control enforcement)\n",
            tag, status, PsGetCurrentProcessId());
        goto Cleanup;
    }

    ok = TRUE;

Cleanup:
    ObDereferenceObject(fileObject);
    return ok;
}

static VOID
NodokaSigCheckHexDump(
    _In_reads_bytes_(Size) const UCHAR *Bytes,
    _In_ ULONG Size,
    _Out_writes_bytes_(NODOKA_SIG_THUMBPRINT_MAX * 2 + 1) PSTR Hex
    )
{
    ULONG n = (Size < NODOKA_SIG_THUMBPRINT_MAX) ? Size : NODOKA_SIG_THUMBPRINT_MAX;
    ULONG i;
    static const CHAR hexDigits[] = "0123456789ABCDEF";

    for (i = 0; i < n; i++) {
        Hex[i * 2]     = hexDigits[Bytes[i] >> 4];
        Hex[i * 2 + 1] = hexDigits[Bytes[i] & 0xF];
    }
    Hex[n * 2] = '\0';
}

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
NodokaSigCheckTrace(
    _In_opt_ PCSTR Tag
    )
{
    ULONG             flags;
    SE_SIGNING_LEVEL  signingLevel;
    UCHAR             thumbprint[NODOKA_SIG_THUMBPRINT_MAX];
    ULONG             thumbprintSize;
    ULONG             thumbprintAlgorithm;
    PCSTR             tag = (Tag != NULL) ? Tag : "?";

    if (!NodokaSigCheckQuery(&flags, &signingLevel, thumbprint, &thumbprintSize,
            &thumbprintAlgorithm, tag)) {
        return FALSE;
    }

    {
    CHAR hex[NODOKA_SIG_THUMBPRINT_MAX * 2 + 1];
    NodokaSigCheckHexDump(thumbprint, thumbprintSize, hex);
    NODOKA_SIG_LOG("[%s] pid=%p flags=0x%08X signingLevel=%u thumbprintAlgo=%u thumbprintSize=%lu thumbprint=%s\n",
        tag, PsGetCurrentProcessId(), flags, signingLevel, thumbprintAlgorithm, thumbprintSize, hex);
    }

    return TRUE;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
NodokaSigCheckFastPath(VOID)
{
    ULONG             flags;
    SE_SIGNING_LEVEL  signingLevel;
    UCHAR             thumbprint[NODOKA_SIG_THUMBPRINT_MAX];
    ULONG             thumbprintSize;
    ULONG             thumbprintAlgorithm;

    if (!NodokaSigCheckQuery(&flags, &signingLevel, thumbprint, &thumbprintSize,
            &thumbprintAlgorithm, "fastpath")) {
        return FALSE; // キャッシュ無し -- 未認証 (拒否ではない)。L3 へフォールバック。
    }

    if (signingLevel < SE_SIGNING_LEVEL_AUTHENTICODE) {
        NODOKA_SIG_LOG("fastpath: signingLevel=%u too low (pid=%p)\n",
            signingLevel, PsGetCurrentProcessId());
        return FALSE;
    }

    if (thumbprintSize != sizeof(g_VendorThumbprint) ||
        !RtlEqualMemory(thumbprint, g_VendorThumbprint, sizeof(g_VendorThumbprint))) {
        CHAR hex[NODOKA_SIG_THUMBPRINT_MAX * 2 + 1];
        NodokaSigCheckHexDump(thumbprint, thumbprintSize, hex);
        NODOKA_SIG_LOG("fastpath: thumbprint mismatch (pid=%p) got=%s\n",
            PsGetCurrentProcessId(), hex);
        return FALSE;
    }

    NODOKA_SIG_LOG("fastpath: OK (pid=%p)\n", PsGetCurrentProcessId());
    return TRUE;
}
