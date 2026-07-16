#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>

#include "regutil.h"

/*
 RegReadDword - レジストリから REG_DWORD 値を読み取る共通ユーティリティ
 - 呼び出しは PASSIVE_LEVEL を前提とします（関数本体は PAGED セクション）。
 - プロジェクトに必ず addid/common/regutil.c を追加してください。
 - プールタグ: 'rRgK'
*/

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE, RegReadDword)
#pragma alloc_text(PAGE, RegReadServiceString)
#endif

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return == STATUS_SUCCESS)
NTSTATUS
RegReadDword(
    _In_ PCWSTR serviceName,
    _In_ PCWSTR valueName,
    _Out_ PULONG outValue,
    _In_ ULONG defaultValue
)
{
    PAGED_CODE();

    UNICODE_STRING keyName;
    UNICODE_STRING valueUni;
    WCHAR keyPath[256];
    OBJECT_ATTRIBUTES oa;
    HANDLE regKey = NULL;
    NTSTATUS status;
    ULONG resultLength = 0;
    PKEY_VALUE_PARTIAL_INFORMATION info = NULL;

    /* outValue が NULL の場合は何もしない（互換性のため STATUS_SUCCESS を返す） */
    if (outValue == NULL) {
        return STATUS_SUCCESS;
    }

    /* まずデフォルトを設定 */
    *outValue = defaultValue;

    if (!serviceName || !valueName) {
        return STATUS_SUCCESS;
    }

    status = RtlStringCchPrintfW(keyPath, RTL_NUMBER_OF(keyPath),
                                 L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\%ws\\Parameters",
                                 serviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("regutil: RtlStringCchPrintfW failed, status=0x%08X\n", status);
        return STATUS_SUCCESS;
    }

    RtlInitUnicodeString(&keyName, keyPath);
    RtlInitUnicodeString(&valueUni, valueName);
    InitializeObjectAttributes(&oa, &keyName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwOpenKey(&regKey, KEY_READ, &oa);
    if (!NT_SUCCESS(status)) {
        DbgPrint("regutil: ZwOpenKey(%ws) failed, status=0x%08X; using default %lu\n", keyPath, status, defaultValue);
        return STATUS_SUCCESS;
    }

    status = ZwQueryValueKey(regKey, &valueUni, KeyValuePartialInformation, NULL, 0, &resultLength);
    if (status != STATUS_BUFFER_TOO_SMALL && status != STATUS_SUCCESS) {
        DbgPrint("regutil: ZwQueryValueKey(size) failed, status=0x%08X; using default %lu\n", status, defaultValue);
        ZwClose(regKey);
        return STATUS_SUCCESS;
    }

    info = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePool2(POOL_FLAG_PAGED, resultLength, 'rRgK');
    if (!info) {
        DbgPrint("regutil: ExAllocatePool2 failed, using default %lu\n", defaultValue);
        ZwClose(regKey);
        return STATUS_SUCCESS;
    }

    status = ZwQueryValueKey(regKey, &valueUni, KeyValuePartialInformation, info, resultLength, &resultLength);
    if (NT_SUCCESS(status)) {
        if (info->Type == REG_DWORD && info->DataLength == sizeof(ULONG)) {
            ULONG val = *(ULONG*)info->Data;
            *outValue = val;
            DbgPrint("regutil: %ws\\%ws = %lu\n", serviceName, valueName, val);
        } else {
            DbgPrint("regutil: invalid type=%lu or length=%lu for %ws\\%ws; using default %lu\n",
                     info->Type, info->DataLength, serviceName, valueName, defaultValue);
        }
    } else {
        DbgPrint("regutil: ZwQueryValueKey(read) failed, status=0x%08X; using default %lu\n", status, defaultValue);
    }

    if (info) {
        ExFreePoolWithTag(info, 'rRgK');
    }
    ZwClose(regKey);

    /* 常に STATUS_SUCCESS を返す（既存コード互換） */
    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return == STATUS_SUCCESS)
NTSTATUS
RegReadServiceString(
    _In_ PCWSTR serviceName,
    _In_ PCWSTR valueName,
    _Out_writes_z_(maxChars) PWCHAR outBuf,
    _In_ ULONG maxChars
)
{
    PAGED_CODE();

    if (!outBuf || maxChars == 0) return STATUS_INVALID_PARAMETER;
    outBuf[0] = 0;
    if (!serviceName || !valueName) return STATUS_SUCCESS;

    UNICODE_STRING keyName;
    UNICODE_STRING valueUni;
    WCHAR keyPath[256];
    OBJECT_ATTRIBUTES oa;
    HANDLE regKey = NULL;
    NTSTATUS status;
    ULONG resultLength = 0;
    PKEY_VALUE_PARTIAL_INFORMATION info = NULL;

    status = RtlStringCchPrintfW(keyPath, RTL_NUMBER_OF(keyPath),
                                 L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\%ws\\Parameters",
                                 serviceName);
    if (!NT_SUCCESS(status)) return STATUS_SUCCESS;

    RtlInitUnicodeString(&keyName, keyPath);
    RtlInitUnicodeString(&valueUni, valueName);
    InitializeObjectAttributes(&oa, &keyName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwOpenKey(&regKey, KEY_READ, &oa);
    if (!NT_SUCCESS(status)) return STATUS_SUCCESS;

    status = ZwQueryValueKey(regKey, &valueUni, KeyValuePartialInformation, NULL, 0, &resultLength);
    if (status != STATUS_BUFFER_TOO_SMALL && !NT_SUCCESS(status)) {
        ZwClose(regKey);
        return STATUS_SUCCESS;
    }

    info = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePool2(POOL_FLAG_PAGED, resultLength, 'sRgK');
    if (!info) {
        ZwClose(regKey);
        return STATUS_SUCCESS;
    }

    status = ZwQueryValueKey(regKey, &valueUni, KeyValuePartialInformation, info, resultLength, &resultLength);
    if (NT_SUCCESS(status) && info->Type == REG_SZ && info->DataLength >= sizeof(WCHAR)) {
        ULONG wchars = info->DataLength / sizeof(WCHAR);
        ULONG copyChars = (wchars < maxChars) ? wchars : maxChars - 1;
        RtlCopyMemory(outBuf, info->Data, copyChars * sizeof(WCHAR));
        outBuf[copyChars] = 0;
    }

    ExFreePoolWithTag(info, 'sRgK');
    ZwClose(regKey);
    return STATUS_SUCCESS;
}