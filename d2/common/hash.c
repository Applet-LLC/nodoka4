#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>
#include "hash.h"

/*
 Fnv1aHash16Upper - strict 16-bit FNV-1a を実装し、戻り値は ULONG の上位16bit に格納する
 - 呼び出しは PASSIVE_LEVEL を前提とします（関数は PAGE セグメントへ配置）。
*/

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE, Fnv1aHash16Upper)
#endif

_IRQL_requires_max_(PASSIVE_LEVEL)
ULONG
Fnv1aHash16Upper(_In_ PCWSTR str)
{
    PAGED_CODE();

    if (str == NULL) {
        return 0;
    }

    const USHORT offset_basis = 0x9DC5;   // lower 16 bits of 0x811C9DC5
    const USHORT FNV_prime16 = 0x0193;    // lower 16 bits of 0x01000193
    USHORT hash = offset_basis;

    while (*str) {
        WCHAR ch = *str++;

        unsigned char b0 = (unsigned char)(ch & 0xFF);
        unsigned char b1 = (unsigned char)((ch >> 8) & 0xFF);

        // low byte
        hash ^= (USHORT)b0;
        hash = (USHORT)(hash * FNV_prime16);

        // high byte
        hash ^= (USHORT)b1;
        hash = (USHORT)(hash * FNV_prime16);
    }

    return ((ULONG)hash) << 16;
}