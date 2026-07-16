#ifndef _ADDID_COMMON_REGUTIL_H_
#define _ADDID_COMMON_REGUTIL_H_

/*
 RegReadDword - レジストリから REG_DWORD 値を読み取る共通ユーティリティ
 - 呼び出しは PASSIVE_LEVEL を前提とします（関数本体は PAGED セクション）。
 - プロジェクトに addid/common/regutil.c を追加してください。
 - カーネルモード専用のヘッダのみを使用しています。
*/

#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return == STATUS_SUCCESS)
NTSTATUS
RegReadDword(
    _In_ PCWSTR serviceName,
    _In_ PCWSTR valueName,
    _Out_ PULONG outValue,
    _In_ ULONG defaultValue
);

/*
 RegReadServiceString - サービスの Parameters キーから REG_SZ 値を読み取る
 - HKLM\SYSTEM\CurrentControlSet\Services\<serviceName>\Parameters\<valueName>
 - 呼び出しは PASSIVE_LEVEL を前提とします。
 - outBuf に読み取った WCHAR 文字列を格納する（null 終端保証）。
 - 値が存在しない場合は outBuf[0] = 0 にして STATUS_SUCCESS を返す。
*/
_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return == STATUS_SUCCESS)
NTSTATUS
RegReadServiceString(
    _In_ PCWSTR serviceName,
    _In_ PCWSTR valueName,
    _Out_writes_z_(maxChars) PWCHAR outBuf,
    _In_ ULONG maxChars
);

#endif // _ADDID_COMMON_REGUTIL_H_