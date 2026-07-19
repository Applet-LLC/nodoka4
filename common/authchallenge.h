// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

/*++

Module Name:

    authchallenge.h

Abstract:

    L3 (docs/driver-access-control-plan.md) 呼び出し元認証: challenge-response。
    open 直後は未認証。ドライバが nonce を発行し (NodokaAuthGenerateNonce)、
    クライアントがアプリ埋め込みの秘密鍵 (BCRYPT_ECCPRIVATE_BLOB, ECDSA P-256) で
    署名して提出、ドライバは埋め込み公開鍵で検証する (NodokaAuthVerifyResponse)。

    ここで使う鍵ペアはライセンストークン検証用
    (nodoka_subscribe\addid\kbdaddid\token.c の s_PubKeyBlob) とは別物。
    再利用しているのは BCrypt CNG の呼び出しパターン (BCryptOpenAlgorithmProvider /
    BCryptImportKeyPair / BCryptVerifySignature / BCryptGenRandom) のみ。

    署名対象は nonce 生値ではなく SHA-256(domain tag || nonce)。ドメイン分離用の
    タグを混ぜることで、万一将来別プロトコルと鍵が混同されても署名の使い回しが
    成立しないようにしている。

Environment:

    Kernel mode only. Call at PASSIVE_LEVEL (BCrypt はページアウト可能コードで
    DISPATCH_LEVEL 不可)。d / d2 両ドライバから ../common/authchallenge.h として
    参照される想定 (Link 設定に ksecdd.lib の追加が必要)。

--*/

#ifndef _NODOKA_AUTHCHALLENGE_H
#define _NODOKA_AUTHCHALLENGE_H

#include <ntddk.h>

#define NODOKA_AUTH_NONCE_SIZE 32
#define NODOKA_AUTH_SIG_SIZE   64

// BCryptGenRandom で nonce を生成する。
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
NodokaAuthGenerateNonce(
    _Out_writes_bytes_(NODOKA_AUTH_NONCE_SIZE) PUCHAR Nonce
    );

// Nonce に対する Signature (raw r||s, 64 bytes) を埋め込み公開鍵で検証する。
// TRUE = 検証成功 (nonce をこの秘密鍵の所持者が署名した)。
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
NodokaAuthVerifyResponse(
    _In_reads_bytes_(NODOKA_AUTH_NONCE_SIZE) const UCHAR Nonce[NODOKA_AUTH_NONCE_SIZE],
    _In_reads_bytes_(NODOKA_AUTH_SIG_SIZE) const UCHAR Signature[NODOKA_AUTH_SIG_SIZE]
    );

#endif // _NODOKA_AUTHCHALLENGE_H
