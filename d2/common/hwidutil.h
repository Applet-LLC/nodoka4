/*
  addid/common/hwidutil.h
  HardwareId 正規化ユーティリティ（ヘッダ内インライン実装を条件付きで提供）
  - MULTI_SZ (HardwareId) から先頭要素を抽出して大文字化し、不要文字を除去して返す。
  - ヘッダ内に static inline 実装を置くのは便宜上だが、同名の外部実装がある場合は
    翻訳単位側で ADDID_IMPLEMENT_HWIDUTIL を定義してヘッダ内実装を無効化してください。
*/

#ifndef ADDID_COMMON_HWIDUTIL_H
#define ADDID_COMMON_HWIDUTIL_H

#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 NormalizeHardwareIdFirstElement
  - multiSz: MULTI_SZ 形式の HardwareId (NULL terminated list). NULL 可（その場合は STATUS_INVALID_PARAMETER）。
  - dest: 出力バッファ（UNICODE 文字列、NUL 終端される）。
  - destChars: dest の WCHAR 単位の長さ（バイト長ではない）。
  - 戻り値: STATUS_SUCCESS（正常）、STATUS_BUFFER_TOO_SMALL（出力バッファ不足）、STATUS_INVALID_PARAMETER 等。
*/
/* 宣言 (既存の外部実装があればそちらが使われます) */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return == STATUS_SUCCESS)
NTSTATUS
NormalizeHardwareIdFirstElement(
    _In_opt_ PCWSTR multiSz,
    _Out_writes_(destChars) PWSTR dest,
    _In_ SIZE_T destChars
    );

/* ヘッダ内の static inline 実装 (翻訳単位が外部実装を提供しない場合にのみ有効にする) */
#ifndef ADDID_IMPLEMENT_HWIDUTIL

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return == STATUS_SUCCESS)
static
NTSTATUS
NormalizeHardwareIdFirstElement_inline(
    _In_opt_ PCWSTR multiSz,
    _Out_writes_(destChars) PWSTR dest,
    _In_ SIZE_T destChars
    )
{
    if (dest == NULL || destChars == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    dest[0] = L'\0';

    if (multiSz == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    PCWSTR p = multiSz;
    if (p[0] == L'\0') {
        return STATUS_NOT_FOUND;
    }

    SIZE_T di = 0;
    /* copy allowed chars from first element, uppercasing ASCII letters */
    while (*p != L'\0' && di + 1 < destChars) {
        WCHAR c = *p++;
        /* Allow basic set: alnum, backslash, underscore, hyphen, dot, brace, ampersand, colon */
        BOOLEAN ok =
            ((c >= L'0' && c <= L'9') ||
             (c >= L'A' && c <= L'Z') ||
             (c >= L'a' && c <= L'z') ||
             c == L'\\' || c == L'_' || c == L'-' || c == L'.' ||
             c == L'{' || c == L'}' || c == L'&' || c == L':');
        if (!ok) {
            continue;
        }
        /* Uppercase ASCII letters */
        if (c >= L'a' && c <= L'z') {
            dest[di++] = (WCHAR)(c - (L'a' - L'A'));
        } else {
            dest[di++] = c;
        }
    }
    dest[di] = L'\0';

    if (di + 1 >= destChars) {
        dest[0] = L'\0';
        return STATUS_BUFFER_TOO_SMALL;
    }

    return STATUS_SUCCESS;
}

/* マクロで通常の名前に紐づける: 呼び出し側がヘッダを include していればこの inline 実装が使われる */
#undef NormalizeHardwareIdFirstElement
#define NormalizeHardwareIdFirstElement NormalizeHardwareIdFirstElement_inline

#endif /* ADDID_IMPLEMENT_HWIDUTIL */

#ifdef __cplusplus
}
#endif

#endif /* ADDID_COMMON_HWIDUTIL_H */