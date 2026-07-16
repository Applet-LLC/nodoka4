#ifndef _ADDID_COMMON_HASH_H_
#define _ADDID_COMMON_HASH_H_

/*
  hash.h - 共通 FNV-1a 16bit ハッシュ (上位16bit を返す)
  - 呼び出しは PASSIVE_LEVEL を前提とします（関数は PAGED に置く想定）。
*/

#include <ntddk.h>
#include <wdm.h>

_IRQL_requires_max_(PASSIVE_LEVEL)
ULONG
Fnv1aHash16Upper(_In_ PCWSTR str);

#endif /* _ADDID_COMMON_HASH_H_ */