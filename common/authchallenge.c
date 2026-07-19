// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#include "authchallenge.h"
#include <bcrypt.h>

//
// ECDSA P-256 public key (BCRYPT_ECCPUBLIC_BLOB, 72 bytes). Pairs with the
// private key embedded in the client app (nodoka\nodoka\authkey.h).
// Generated 2026-07-19 specifically for this challenge-response protocol
// (distinct from the license-token key pair used by token.c).
//
static const UCHAR g_AuthPubKeyBlob[72] = {
    /* magic (BCRYPT_ECDSA_PUBLIC_P256_MAGIC) + cbKey */
    0x45, 0x43, 0x53, 0x31,  0x20, 0x00, 0x00, 0x00,
    /* X */
    0x58, 0x9F, 0xBF, 0x20,  0x9A, 0x20, 0x10, 0x73,
    0xD9, 0x4F, 0x06, 0x84,  0xC6, 0x3D, 0x0A, 0x6E,
    0x08, 0x66, 0xBF, 0x8B,  0x77, 0x8D, 0xE2, 0x5C,
    0xC8, 0x1A, 0x28, 0xEE,  0x7A, 0x2E, 0x94, 0xC5,
    /* Y */
    0xF2, 0x49, 0x77, 0xC1,  0xEF, 0x9D, 0xAF, 0x62,
    0xBE, 0x88, 0x7A, 0xBA,  0xCE, 0xDC, 0x3C, 0xCD,
    0xE7, 0x5E, 0x58, 0x03,  0x30, 0x17, 0x6F, 0xE4,
    0x45, 0x88, 0x6A, 0x6E,  0xF8, 0x01, 0x4F, 0xED
};

// ドメイン分離タグ。署名対象は SHA-256(g_AuthDomainTag || nonce)。
static const CHAR g_AuthDomainTag[] = "NODOKA2-AUTH-CHALLENGE-V1";

#define NODOKA_AUTH_LOG(fmt, ...) \
    DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL, "nodoka-auth: " fmt, ##__VA_ARGS__)

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
NodokaAuthGenerateNonce(
    _Out_writes_bytes_(NODOKA_AUTH_NONCE_SIZE) PUCHAR Nonce
    )
{
    NTSTATUS status = BCryptGenRandom(NULL, Nonce, NODOKA_AUTH_NONCE_SIZE,
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!NT_SUCCESS(status)) {
        NODOKA_AUTH_LOG("BCryptGenRandom failed 0x%08X\n", status);
        return FALSE;
    }
    return TRUE;
}

static BOOLEAN
NodokaAuthDomainHash(
    _In_reads_bytes_(NODOKA_AUTH_NONCE_SIZE) const UCHAR Nonce[NODOKA_AUTH_NONCE_SIZE],
    _Out_writes_bytes_(32) PUCHAR Hash
    )
{
    BCRYPT_ALG_HANDLE  hAlg  = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    NTSTATUS status;
    BOOLEAN  ok = FALSE;

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (!NT_SUCCESS(status)) {
        NODOKA_AUTH_LOG("BCryptOpenAlgorithmProvider(SHA256) 0x%08X\n", status);
        return FALSE;
    }

    status = BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0);
    if (!NT_SUCCESS(status)) {
        NODOKA_AUTH_LOG("BCryptCreateHash 0x%08X\n", status);
        goto Cleanup;
    }

    status = BCryptHashData(hHash, (PUCHAR)g_AuthDomainTag, sizeof(g_AuthDomainTag) - 1, 0);
    if (!NT_SUCCESS(status)) {
        NODOKA_AUTH_LOG("BCryptHashData(tag) 0x%08X\n", status);
        goto Cleanup;
    }

    status = BCryptHashData(hHash, (PUCHAR)Nonce, NODOKA_AUTH_NONCE_SIZE, 0);
    if (!NT_SUCCESS(status)) {
        NODOKA_AUTH_LOG("BCryptHashData(nonce) 0x%08X\n", status);
        goto Cleanup;
    }

    status = BCryptFinishHash(hHash, Hash, 32, 0);
    ok = NT_SUCCESS(status);
    if (!ok) {
        NODOKA_AUTH_LOG("BCryptFinishHash 0x%08X\n", status);
    }

Cleanup:
    if (hHash) BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
NodokaAuthVerifyResponse(
    _In_reads_bytes_(NODOKA_AUTH_NONCE_SIZE) const UCHAR Nonce[NODOKA_AUTH_NONCE_SIZE],
    _In_reads_bytes_(NODOKA_AUTH_SIG_SIZE) const UCHAR Signature[NODOKA_AUTH_SIG_SIZE]
    )
{
    UCHAR hash[32];
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_KEY_HANDLE  hKey = NULL;
    NTSTATUS status;
    BOOLEAN  ok = FALSE;

    if (!NodokaAuthDomainHash(Nonce, hash)) {
        return FALSE;
    }

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_ECDSA_P256_ALGORITHM, NULL, 0);
    if (!NT_SUCCESS(status)) {
        NODOKA_AUTH_LOG("BCryptOpenAlgorithmProvider(ECDSA_P256) 0x%08X\n", status);
        return FALSE;
    }

    status = BCryptImportKeyPair(hAlg, NULL, BCRYPT_ECCPUBLIC_BLOB, &hKey,
        (PUCHAR)g_AuthPubKeyBlob, sizeof(g_AuthPubKeyBlob), 0);
    if (!NT_SUCCESS(status)) {
        NODOKA_AUTH_LOG("BCryptImportKeyPair 0x%08X\n", status);
        goto Cleanup;
    }

    status = BCryptVerifySignature(hKey, NULL, hash, sizeof(hash),
        (PUCHAR)Signature, NODOKA_AUTH_SIG_SIZE, 0);
    ok = NT_SUCCESS(status);
    if (!ok) {
        NODOKA_AUTH_LOG("BCryptVerifySignature failed 0x%08X\n", status);
    }

Cleanup:
    if (hKey) BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}
