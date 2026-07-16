#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>
#define ADDID_IMPLEMENT_HWIDUTIL
#include "hwidutil.h"

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE, NormalizeHardwareIdFirstElement)
#endif

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return == STATUS_SUCCESS)
NTSTATUS
NormalizeHardwareIdFirstElement(
    _In_opt_ PCWSTR multiSz,
    _Out_writes_(destChars) PWSTR dest,
    _In_ SIZE_T destChars
    )
{
    PAGED_CODE();

    if (dest == NULL || destChars == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Initialize output */
    dest[0] = L'\0';

    if (multiSz == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Find first element in MULTI_SZ */
    PCWSTR p = multiSz;
    if (p[0] == L'\0') {
        return STATUS_NOT_FOUND;
    }

    /* p now points to first null-terminated string */
    SIZE_T needed = 0;
    /* We'll copy allowed chars into a temp buffer, then uppercase */
    SIZE_T tempChars = destChars;
    PWSTR temp = (PWSTR)ExAllocatePool2(POOL_FLAG_PAGED, tempChars * sizeof(WCHAR), 'tHmI');
    if (!temp) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(temp, tempChars * sizeof(WCHAR));

    SIZE_T ti = 0;
    while (*p != L'\0' && ti + 1 < tempChars) {
        WCHAR c = *p++;

        /* Allow basic set: alnum, backslash, underscore, hyphen, dot, brace, ampersand */
        if ((c >= L'0' && c <= L'9') ||
            (c >= L'A' && c <= L'Z') ||
            (c >= L'a' && c <= L'z') ||
            c == L'\\' || c == L'_' || c == L'-' || c == L'.' ||
            c == L'{' || c == L'}' || c == L'&' || c == L':'
        ) {
            temp[ti++] = c;
        } else {
            /* skip other characters */
        }
    }

    temp[ti] = L'\0';
    needed = ti + 1;

    if (needed > destChars) {
        ExFreePoolWithTag(temp, 'tHmI');
        return STATUS_BUFFER_TOO_SMALL;
    }

    /* Uppercase in place */
    UNICODE_STRING uniTemp;
    RtlInitUnicodeString(&uniTemp, temp);
    UNICODE_STRING uniDest;
    RtlInitEmptyUnicodeString(&uniDest, dest, (USHORT)(destChars * sizeof(WCHAR)));

    NTSTATUS status = RtlUpcaseUnicodeString(&uniDest, &uniTemp, FALSE);
    if (!NT_SUCCESS(status)) {
        /* fallback: copy raw */
        RtlStringCchCopyW(dest, destChars, temp);
        ExFreePoolWithTag(temp, 'tHmI');
        return STATUS_SUCCESS;
    }

    ExFreePoolWithTag(temp, 'tHmI');
    return STATUS_SUCCESS;
}