// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/
#pragma once

// L3 challenge-response (docs/driver-access-control-plan.md) のクライアント側。
// nodokad2 コントロールデバイスに対して、埋め込み ECDSA P-256 秘密鍵で認証する。
// ドライバ側の対応する検証コードは nodoka\common\authchallenge.c/h。
//
// 秘密鍵はこのバイナリに埋め込まれる時点で「秘密」ではなくなる (RE で抽出可能)。
// これは L3 の既知の限界であり、目的は T1 (無関係の無署名アプリからの
// デバイス直接 open) を遮断することであって、同一ユーザー権限での
// リバースエンジニアリング耐性ではない (docs/driver-access-control-plan.md §7)。

#include <windows.h>

// nodokad2 コントロールデバイスに対して L3 challenge-response を実行する。
// device は既に CreateFile 済みのハンドル (IOCTL_NODOKA2_AUTH_BEGIN /
// IOCTL_NODOKA2_AUTH_RESPONSE を発行する)。成功で true
// (以降 SET_MODE/GET_EVENTS/INJECT が使える)。失敗時、ドライバ側は
// そのハンドルを未認証のまま拒否し続けるので、呼び出し側は open 全体を
// 失敗扱いにすること。
bool NodokaAuthenticateV2Device(HANDLE device);
