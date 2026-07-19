// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#include "authkey.h"
#include "..\d2\public2.h"

#include <tchar.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

// 本物の秘密鍵は git 管理対象外の authkey_secret.h に分離してある
// (無い場合は authkey_secret.example.h をコピーして作成すること。
// ダミー値なのでビルドは通るが実行時の L3 認証は必ず失敗する)。
#if __has_include("authkey_secret.h")
#include "authkey_secret.h"
#else
#include "authkey_secret.example.h"
#pragma message("authkey.cpp: authkey_secret.h not found -- using dummy key from authkey_secret.example.h (L3 auth will fail at runtime). Copy authkey_secret.example.h to authkey_secret.h and fill in the real key.")
#endif

namespace {

// ドライバ側 (nodoka\common\authchallenge.c) と同一のドメイン分離タグ。
const char kDomainTag[] = "NODOKA2-AUTH-CHALLENGE-V1";

bool Sha256DomainSeparated(const BYTE nonce[NODOKA2_AUTH_NONCE_SIZE], BYTE hash[32])
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    bool ok = false;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
        return false;

    if (BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0)) &&
        BCRYPT_SUCCESS(BCryptHashData(hHash, (PUCHAR)kDomainTag, (ULONG)(sizeof(kDomainTag) - 1), 0)) &&
        BCRYPT_SUCCESS(BCryptHashData(hHash, (PUCHAR)nonce, NODOKA2_AUTH_NONCE_SIZE, 0)) &&
        BCRYPT_SUCCESS(BCryptFinishHash(hHash, hash, 32, 0)))
    {
        ok = true;
    }

    if (hHash) BCryptDestroyHash(hHash);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

bool SignDomainHash(const BYTE hash[32], BYTE sig[NODOKA2_AUTH_SIG_SIZE])
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    ULONG sigLen = 0;
    bool ok = false;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0)))
        return false;

    if (BCRYPT_SUCCESS(BCryptImportKeyPair(hAlg, nullptr, BCRYPT_ECCPRIVATE_BLOB, &hKey,
            (PUCHAR)kPrivateKeyBlob, sizeof(kPrivateKeyBlob), 0)))
    {
        ok = BCRYPT_SUCCESS(BCryptSignHash(hKey, nullptr, (PUCHAR)hash, 32,
                 sig, NODOKA2_AUTH_SIG_SIZE, &sigLen, 0))
             && sigLen == NODOKA2_AUTH_SIG_SIZE;
    }

    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

} // namespace

bool NodokaAuthenticateV2Device(HANDLE device)
{
    BYTE  nonce[NODOKA2_AUTH_NONCE_SIZE] = {};
    DWORD bytes = 0;

    if (!DeviceIoControl(device, IOCTL_NODOKA2_AUTH_BEGIN, NULL, 0,
            nonce, sizeof(nonce), &bytes, NULL) || bytes != sizeof(nonce))
    {
        OutputDebugString(_T("nodoka NodokaAuthenticateV2Device: AUTH_BEGIN failed\n"));
        return false;
    }

    BYTE hash[32];
    if (!Sha256DomainSeparated(nonce, hash))
    {
        OutputDebugString(_T("nodoka NodokaAuthenticateV2Device: hash failed\n"));
        return false;
    }

    BYTE sig[NODOKA2_AUTH_SIG_SIZE] = {};
    if (!SignDomainHash(hash, sig))
    {
        OutputDebugString(_T("nodoka NodokaAuthenticateV2Device: sign failed\n"));
        return false;
    }

    if (!DeviceIoControl(device, IOCTL_NODOKA2_AUTH_RESPONSE, sig, sizeof(sig),
            NULL, 0, &bytes, NULL))
    {
        OutputDebugString(_T("nodoka NodokaAuthenticateV2Device: AUTH_RESPONSE rejected\n"));
        return false;
    }

    OutputDebugString(_T("nodoka NodokaAuthenticateV2Device: authenticated\n"));
    return true;
}
