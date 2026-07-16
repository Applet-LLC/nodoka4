#ifndef _ADDID_COMMON_FALLBACK_H_
#define _ADDID_COMMON_FALLBACK_H_

/*
  fallback.h - AddDevice のフォールバック試行群のプロトタイプ定義
  - 各関数は PASSIVE_LEVEL で呼び出すことを想定しています（同期的 / スタブ実装）。
  - 非同期 IRP 用の非破壊拡張として TryQueryIdAsync を追加しました:
      BOOLEAN TryQueryIdAsync(PDEVICE_OBJECT FilterDeviceObject, PDEVICE_OBJECT PhysicalDeviceObject, ULONG uniqNum);
    この関数は WorkItem を使って IRP_MN_QUERY_ID を非同期に発行し、完了時に
    FilterDeviceObject->DeviceExtension 内の DeviceId を設定します。
  - addid/common/fallback.c をプロジェクトに追加してください。
*/

#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>

#ifdef __cplusplus
extern "C" {
#endif

/* フォールバック試行関数の関数型
   OutDeviceId に DeviceId を書き込んで TRUE を返すこと（呼び出し側が devExt->DeviceId に代入する）。
*/
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN
(*PFALLBACK_TRY)(
    _In_ PDEVICE_OBJECT Pdo,
    _Out_ PULONG OutDeviceId,
    _In_ ULONG uniqNum
    );

/* 試行関数プロトタイプ（同期/スタブ実装を fallback.c に実装） */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN TryContainerId(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum);

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN TryQueryId(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum);

/* 非破壊の非同期版: FilterDeviceObject を渡して WorkItem 内で IRP を投げ、
   完了時に FilterDeviceObject->DeviceExtension->DeviceId を設定します。
   戻り値: WorkItem のキューに成功したら TRUE（AddDevice は即 validPdoFound=TRUE にできる）。
*/
_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN TryQueryIdAsync(_In_ PDEVICE_OBJECT FilterDeviceObject, _In_ PDEVICE_OBJECT PhysicalDeviceObject, _In_ ULONG uniqNum);

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN TryIoGetDeviceProperty(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum);

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN TryCompatibleIds(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum);

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN TryHardwareIdWithUniq(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum);

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN UseFixedValue(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum);

#ifdef __cplusplus
}
#endif

#endif /* _ADDID_COMMON_FALLBACK_H_ */