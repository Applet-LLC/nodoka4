/*
  fallback.c - AddDevice のフォールバック試行（警告抑制・改良）
  - TryQueryId は IRP_MN_QUERY_ID を非同期で投げ、Completion で待機して結果を同期的に返します。
  - TryQueryIdAsync を追加し、WorkItem ベースで IRP を非同期発行して FilterDeviceObject->DeviceExtension->DeviceId を設定します（非破壊）。
  - 各関数は PASSIVE_LEVEL で呼び出すことを想定しています。
  - このファイルをプロジェクトに追加してください。 // TODO: add to project
*/

#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>
/* Avoid including the SDK devpkey header (pulls many dependencies); use our minimal common devprop.h */
#include "..\\common\\devprop.h"
#include "..\\common\\hash.h"
/* Note: hwidutil.h is intentionally NOT included here to avoid link-time dependency on a separate object;
   a local, internal normalizer is provided below to guarantee fallback.c builds even if hwidutil.c
   is not added to the project's compile/link units.
*/
/* #include "..\\common\\hwidutil.h" */

/* Forward declarations for functions used in #pragma alloc_text
   (must appear before pragmas to avoid C2157)
*/
_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN
TryContainerId(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum);

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN
TryQueryId(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum);

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN
TryIoGetDeviceProperty(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum);

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN
TryCompatibleIds(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum);

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN
TryHardwareIdWithUniq(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum);

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN
UseFixedValue(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum);

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN
TryQueryIdAsync(_In_ PDEVICE_OBJECT FilterDeviceObject, _In_ PDEVICE_OBJECT PhysicalDeviceObject, _In_ ULONG uniqNum);

/* Forward declare the local normalizer so we can place it into the PAGE segment via pragma */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return == STATUS_SUCCESS)
static
NTSTATUS
LocalNormalizeHardwareIdFirstElement(
    _In_opt_ PCWSTR multiSz,
    _Out_writes_(destChars) PWSTR dest,
    _In_ SIZE_T destChars
    );

/* export to PAGE section for functions that call PAGED_CODE() */
#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE, TryContainerId)
#pragma alloc_text(PAGE, TryQueryId)
#pragma alloc_text(PAGE, TryIoGetDeviceProperty)
#pragma alloc_text(PAGE, TryCompatibleIds)
#pragma alloc_text(PAGE, TryHardwareIdWithUniq)
#pragma alloc_text(PAGE, UseFixedValue)
#pragma alloc_text(PAGE, TryQueryIdAsync)
#pragma alloc_text(PAGE, LocalNormalizeHardwareIdFirstElement)
#endif

/* Internal lightweight normalizer.
   This is a local copy of the NormalizeHardwareIdFirstElement logic used to avoid
   unresolved externals when common/hwidutil.c is not linked into a given driver project.
   It is file-static to avoid external symbol collisions.
*/
_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return == STATUS_SUCCESS)
static
NTSTATUS
LocalNormalizeHardwareIdFirstElement(
    _In_opt_ PCWSTR multiSz,
    _Out_writes_(destChars) PWSTR dest,
    _In_ SIZE_T destChars
    )
{
    PAGED_CODE();

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

    SIZE_T needed = 0;
    SIZE_T tempChars = destChars;
    PWSTR temp = (PWSTR)ExAllocatePool2(POOL_FLAG_PAGED, tempChars * sizeof(WCHAR), 'tHmI');
    if (!temp) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(temp, tempChars * sizeof(WCHAR));

    SIZE_T ti = 0;
    while (*p != L'\0' && ti + 1 < tempChars) {
        WCHAR c = *p++;

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

    UNICODE_STRING uniTemp;
    RtlInitUnicodeString(&uniTemp, temp);
    UNICODE_STRING uniDest;
    RtlInitEmptyUnicodeString(&uniDest, dest, (USHORT)(destChars * sizeof(WCHAR)));

    NTSTATUS status = RtlUpcaseUnicodeString(&uniDest, &uniTemp, FALSE);
    if (!NT_SUCCESS(status)) {
        RtlStringCchCopyW(dest, destChars, temp);
        ExFreePoolWithTag(temp, 'tHmI');
        return STATUS_SUCCESS;
    }

    ExFreePoolWithTag(temp, 'tHmI');
    return STATUS_SUCCESS;
}

/* Completion context used by TryQueryId */
typedef struct _QUERY_CONTEXT {
    KEVENT Event;
    PVOID InstanceId; /* returned pointer from lower driver (must be ExFreePool'd) */
    NTSTATUS Status;
} QUERY_CONTEXT, *PQUERY_CONTEXT;

/* Completion routine for IRP_MN_QUERY_ID used by synchronous TryQueryId */
_At_(Context, _Notnull_)
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
FallbackQueryCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PQUERY_CONTEXT q = (PQUERY_CONTEXT)Context;

    /* capture status and Information pointer */
    q->Status = Irp->IoStatus.Status;
    q->InstanceId = (PVOID)Irp->IoStatus.Information;

    /* signal the waiting thread */
    KeSetEvent(&q->Event, IO_NO_INCREMENT, FALSE);

    /* tell the I/O manager that we will handle finalization (caller will IoFreeIrp) */
    return STATUS_MORE_PROCESSING_REQUIRED;
}

/* Completion routine for asynchronous query (will compute hash and set DeviceId on filter devExt) */
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
FallbackQueryCompletionAsync(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    UNREFERENCED_PARAMETER(DeviceObject);

    struct _ASYNC_CTX {
        PDEVICE_OBJECT FilterDevice;
        PIRP Irp;
        PIO_WORKITEM WorkItem;
    };

    PVOID ctx = Context;
    if (ctx == NULL) {
        IoFreeIrp(Irp);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    /* Context is pointer to ASYNC_CTX */
    PVOID instancePtr = (PVOID)Irp->IoStatus.Information;
    NTSTATUS status = Irp->IoStatus.Status;

    if (NT_SUCCESS(status) && instancePtr != NULL) {
        WCHAR* instanceId = (WCHAR*)instancePtr;
        /* Compute hash and set to filter device extension safely */
        PDEVICE_OBJECT filterDev = ((struct _ASYNC_CTX*)ctx)->FilterDevice;
        if (filterDev && filterDev->DeviceExtension) {
            /* Use common hash function */
            ULONG hid = Fnv1aHash16Upper(instanceId);
            /* Assume DeviceExtension layout has DeviceId at known offset (both drivers use same first fields) */
            /* For safety, cast to ULONG* pointer to Field DeviceId location: second field in struct in drivers */
            ULONG* devIdPtr = (ULONG*)((PUCHAR)filterDev->DeviceExtension + sizeof(PDEVICE_OBJECT));
            /* This is brittle but consistent with current layouts; alternatively provide setter API. */
            *devIdPtr = hid;
            DbgPrint("fallback-async: Set DeviceId=%08X for filterDev=%p\n", hid, filterDev);
        }

        /* free buffer allocated by lower driver */
        ExFreePool(instancePtr);
    }

    /* free the IRP */
    IoFreeIrp(Irp);

    /* free workitem and context */
    if (((struct _ASYNC_CTX*)ctx)->WorkItem) {
        IoFreeWorkItem(((struct _ASYNC_CTX*)ctx)->WorkItem);
    }
    ExFreePoolWithTag(ctx, 'aQcx');

    return STATUS_MORE_PROCESSING_REQUIRED;
}

/* WorkItem context for TryQueryIdAsync */
typedef struct _QUERY_ASYNC_CTX {
    PDEVICE_OBJECT FilterDevice;
    PDEVICE_OBJECT PhysicalDevice;
    ULONG uniqNum;
    PIO_WORKITEM WorkItem;
} QUERY_ASYNC_CTX, *PQUERY_ASYNC_CTX;

/* WorkItem routine: issues IRP_MN_QUERY_ID to PhysicalDevice and sets completion routine */
VOID
TryQueryIdWorkRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID Context
    )
{
    UNREFERENCED_PARAMETER(DeviceObject);
    PQUERY_ASYNC_CTX ctx = (PQUERY_ASYNC_CTX)Context;
    if (!ctx || !ctx->PhysicalDevice || !ctx->FilterDevice) {
        if (ctx) {
            if (ctx->WorkItem) IoFreeWorkItem(ctx->WorkItem);
            ExFreePoolWithTag(ctx, 'aQcx');
        }
        return;
    }

    PIRP irp = IoAllocateIrp(ctx->PhysicalDevice->StackSize, FALSE);
    if (!irp) {
        DbgPrint("fallback-async: IoAllocateIrp failed\n");
        if (ctx->WorkItem) IoFreeWorkItem(ctx->WorkItem);
        ExFreePoolWithTag(ctx, 'aQcx');
        return;
    }

    PIO_STACK_LOCATION stack = IoGetNextIrpStackLocation(irp);
    stack->MajorFunction = IRP_MJ_PNP;
    stack->MinorFunction = IRP_MN_QUERY_ID;
    stack->Parameters.QueryId.IdType = BusQueryInstanceID;
    irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    irp->IoStatus.Information = 0;

    /* allocate small ASYNC context to be passed to completion routine */
    struct _ASYNC_CTX {
        PDEVICE_OBJECT FilterDevice;
        PIRP Irp;
        PIO_WORKITEM WorkItem;
    } *ac = ExAllocatePool2(POOL_FLAG_PAGED, sizeof(struct _ASYNC_CTX), 'aQcx');

    if (!ac) {
        DbgPrint("fallback-async: ExAllocatePool2 failed for ac\n");
        IoFreeIrp(irp);
        if (ctx->WorkItem) IoFreeWorkItem(ctx->WorkItem);
        ExFreePoolWithTag(ctx, 'aQcx');
        return;
    }
    ac->FilterDevice = ctx->FilterDevice;
    ac->Irp = irp;
    ac->WorkItem = ctx->WorkItem;

    IoSetCompletionRoutine(irp, FallbackQueryCompletionAsync, ac, TRUE, TRUE, TRUE);

    /* Call driver (non-blocking). completion routine will finalize and free resources. */
    NTSTATUS status = IoCallDriver(ctx->PhysicalDevice, irp);
    UNREFERENCED_PARAMETER(status);

    /* free the ctx allocation (except workitem which completion routine will free) */
    ExFreePoolWithTag(ctx, 'aQcx');
}

/* Non-blocking variant: queue a work item to perform QueryId asynchronously.
   Returns TRUE if work item was queued successfully. */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN
TryQueryIdAsync(_In_ PDEVICE_OBJECT FilterDeviceObject, _In_ PDEVICE_OBJECT PhysicalDeviceObject, _In_ ULONG uniqNum)
{
    PAGED_CODE();

    if (!FilterDeviceObject || !PhysicalDeviceObject) {
        return FALSE;
    }

    PIO_WORKITEM workItem = IoAllocateWorkItem(FilterDeviceObject);
    if (!workItem) {
        DbgPrint("fallback-async: IoAllocateWorkItem failed\n");
        return FALSE;
    }

    PQUERY_ASYNC_CTX ctx = ExAllocatePool2(POOL_FLAG_PAGED, sizeof(QUERY_ASYNC_CTX), 'aQcx');
    if (!ctx) {
        IoFreeWorkItem(workItem);
        DbgPrint("fallback-async: ExAllocatePool2 failed for async ctx\n");
        return FALSE;
    }

    ctx->FilterDevice = FilterDeviceObject;
    ctx->PhysicalDevice = PhysicalDeviceObject;
    ctx->uniqNum = uniqNum;
    ctx->WorkItem = workItem;

    /* Queue work item (DelayedWorkQueue) */
    IoQueueWorkItem(workItem, TryQueryIdWorkRoutine, DelayedWorkQueue, ctx);
    return TRUE;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN
TryContainerId(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum)
{
    PAGED_CODE();

    (void)uniqNum; /* 未使用だが将来拡張用に保持 */

    if (!Pdo || !OutDeviceId) {
        return FALSE;
    }

    /* Try to get ContainerId (GUID) */
    GUID containerId;
    ULONG containerIdLen = sizeof(containerId);
    DEVPROPTYPE propertyType = 0;

    /* Use a local DEVPROPKEY to avoid requiring an external DEVPKEY symbol.
       This mirrors the pattern used in driver translation units that avoid
       linking a separate devprop.c translation unit.
    */
    struct _LOCAL_DEVPROPKEY { GUID fmtid; unsigned long pid; } localContainerKey = {
        {0x8C7ED206, 0x3F8A, 0x4827, {0xB3, 0xAB, 0xAE, 0x9E, 0x1F, 0xAE, 0xFC, 0x6C}},
        2
    };

    NTSTATUS status = IoGetDevicePropertyData(
        Pdo,
        (DEVPROPKEY*)&localContainerKey,
        0,
        0,
        containerIdLen,
        &containerId,
        &containerIdLen,
        &propertyType
    );

    if (!NT_SUCCESS(status) || propertyType != DEVPROP_TYPE_GUID) {
        DbgPrint("fallback: TryContainerId: ContainerId unavailable status=0x%08X type=0x%08X\n", status, propertyType);
        return FALSE;
    }

    /* Format GUID to string */
    WCHAR containerIdStr[64];
    RtlStringCchPrintfW(containerIdStr, RTL_NUMBER_OF(containerIdStr),
                        L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                        containerId.Data1, containerId.Data2, containerId.Data3,
                        containerId.Data4[0], containerId.Data4[1], containerId.Data4[2], containerId.Data4[3],
                        containerId.Data4[4], containerId.Data4[5], containerId.Data4[6], containerId.Data4[7]);

    /* Try to get HardwareId (MULTI_SZ). Prefer the first string if present. */
    WCHAR hwidBufStack[512] = {0};
    ULONG hwidLen = sizeof(hwidBufStack);
    NTSTATUS hwidStatus = IoGetDeviceProperty(
        Pdo,
        DevicePropertyHardwareId,
        hwidLen,
        hwidBufStack,
        &hwidLen
    );

    WCHAR *hwidBuf = hwidBufStack;
    BOOLEAN allocated = FALSE;

    if (hwidStatus == STATUS_BUFFER_TOO_SMALL || hwidLen > sizeof(hwidBufStack)) {
        /* allocate required buffer */
        hwidBuf = (WCHAR*)ExAllocatePool2(POOL_FLAG_PAGED, hwidLen, 'fCtb');
        if (!hwidBuf) {
            DbgPrint("fallback: TryContainerId: ExAllocatePool2 failed for hwidBuf len=%lu\n", hwidLen);
            return FALSE;
        }
        allocated = TRUE;
        RtlZeroMemory(hwidBuf, hwidLen);
        hwidStatus = IoGetDeviceProperty(
            Pdo,
            DevicePropertyHardwareId,
            hwidLen,
            hwidBuf,
            &hwidLen
        );
    }

    if (!NT_SUCCESS(hwidStatus) || hwidBuf[0] == L'\0') {
        DbgPrint("fallback: TryContainerId: HardwareId unavailable status=0x%08X\n", hwidStatus);
        if (allocated) ExFreePoolWithTag(hwidBuf, 'fCtb');
        return FALSE;
    }

    /* Normalize first element of hwid */
    WCHAR normHwid[512] = {0};
    if (!NT_SUCCESS(LocalNormalizeHardwareIdFirstElement(hwidBuf, normHwid, RTL_NUMBER_OF(normHwid)))) {
        /* fallback to raw */
        RtlStringCchCopyW(normHwid, RTL_NUMBER_OF(normHwid), hwidBuf);
    }

    /* Combine hwid + containerIdStr to produce stable identifier */
    WCHAR combined[1024];
    RtlStringCchPrintfW(combined, RTL_NUMBER_OF(combined), L"%ws_%ws", normHwid, containerIdStr);

    ULONG hid = Fnv1aHash16Upper(combined);
    *OutDeviceId = hid;
    DbgPrint("fallback: TryContainerId succeeded DeviceId=%08X (combined)\n", hid);

    if (allocated) ExFreePoolWithTag(hwidBuf, 'fCtb');
    return TRUE;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN
TryQueryId(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum)
{
    PAGED_CODE();

    (void)uniqNum; /* 未使用であることを明示 */

    if (!Pdo || !OutDeviceId) {
        return FALSE;
    }

    PIRP irp = IoAllocateIrp(Pdo->StackSize, FALSE);
    if (!irp) {
        DbgPrint("fallback: TryQueryId: IoAllocateIrp failed\n");
        return FALSE;
    }

    PIO_STACK_LOCATION stack = IoGetNextIrpStackLocation(irp);
    stack->MajorFunction = IRP_MJ_PNP;
    stack->MinorFunction = IRP_MN_QUERY_ID;
    stack->Parameters.QueryId.IdType = BusQueryInstanceID;
    irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    irp->IoStatus.Information = 0;

    QUERY_CONTEXT ctx;
    KeInitializeEvent(&ctx.Event, NotificationEvent, FALSE);
    ctx.InstanceId = NULL;
    ctx.Status = STATUS_NOT_SUPPORTED;

    /* Set completion routine that will signal our event and capture Information */
    IoSetCompletionRoutine(irp, FallbackQueryCompletion, &ctx, TRUE, TRUE, TRUE);

    NTSTATUS status = IoCallDriver(Pdo, irp);
    if (status == STATUS_PENDING) {
        /* wait for completion signaled by completion routine */
        KeWaitForSingleObject(&ctx.Event, Executive, KernelMode, FALSE, NULL);
    } else {
        /* completed synchronously; completion routine may still run, wait if necessary */
        if (ctx.Status == STATUS_NOT_SUPPORTED) {
            KeWaitForSingleObject(&ctx.Event, Executive, KernelMode, FALSE, NULL);
        }
    }

    /* At this point ctx.Status and ctx.InstanceId reflect the result */
    if (NT_SUCCESS(ctx.Status) && ctx.InstanceId != NULL) {
        WCHAR* instanceId = (WCHAR*)ctx.InstanceId;
        /* Compute hash using common function */
        ULONG hid = Fnv1aHash16Upper(instanceId);
        *OutDeviceId = hid;
        DbgPrint("fallback: TryQueryId succeeded DeviceId=%08X (InstanceId=%ws)\n", hid, instanceId);

        /* free buffer allocated by lower driver */
        ExFreePool(instanceId);

        /* free IRP allocated earlier */
        IoFreeIrp(irp);
        return TRUE;
    }

    /* cleanup: free instance buffer if completion returned one on failure */
    if (ctx.InstanceId != NULL) {
        ExFreePool(ctx.InstanceId);
    }
    IoFreeIrp(irp);
    return FALSE;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN
TryIoGetDeviceProperty(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum)
{
    PAGED_CODE();

    /* reuse TryQueryId implementation */
    (void)uniqNum;
    return TryQueryId(Pdo, OutDeviceId, uniqNum);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN
TryCompatibleIds(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum)
{
    PAGED_CODE();

    (void)uniqNum;

    if (!Pdo || !OutDeviceId) {
        return FALSE;
    }

    /* Attempt to read CompatibleIds (MULTI_SZ). Use stack buffer first, allocate if needed. */
    WCHAR compatStack[1024] = {0};
    ULONG compatLen = sizeof(compatStack);
    NTSTATUS status = IoGetDeviceProperty(
        Pdo,
        DevicePropertyCompatibleIds,
        compatLen,
        compatStack,
        &compatLen
    );

    WCHAR *compatBuf = compatStack;
    BOOLEAN allocated = FALSE;

    if (status == STATUS_BUFFER_TOO_SMALL || compatLen > sizeof(compatStack)) {
        compatBuf = (WCHAR*)ExAllocatePool2(POOL_FLAG_PAGED, compatLen, 'fCpm');
        if (!compatBuf) {
            DbgPrint("fallback: TryCompatibleIds: ExAllocatePool2 failed len=%lu\n", compatLen);
            return FALSE;
        }
        allocated = TRUE;
        RtlZeroMemory(compatBuf, compatLen);
        status = IoGetDeviceProperty(
            Pdo,
            DevicePropertyCompatibleIds,
            compatLen,
            compatBuf,
            &compatLen
        );
    }

    if (!NT_SUCCESS(status) || compatBuf[0] == L'\0') {
        DbgPrint("fallback: TryCompatibleIds: CompatibleIds unavailable status=0x%08X\n", status);
        if (allocated) ExFreePoolWithTag(compatBuf, 'fCpm');
        return FALSE;
    }

    /* compatBuf is MULTI_SZ: iterate through each null-terminated entry and pick the first non-empty */
    WCHAR *ptr = compatBuf;
    while (*ptr) {
        /* Normalize and use ptr as candidate */
        WCHAR norm[512] = {0};
        if (!NT_SUCCESS(LocalNormalizeHardwareIdFirstElement(ptr, norm, RTL_NUMBER_OF(norm)))) {
            RtlStringCchCopyW(norm, RTL_NUMBER_OF(norm), ptr);
        }
        if (norm[0] != L'\0') {
            ULONG hid = Fnv1aHash16Upper(norm);
            *OutDeviceId = hid;
            DbgPrint("fallback: TryCompatibleIds succeeded DeviceId=%08X (CompatibleId=%ws)\n", hid, norm);
            if (allocated) ExFreePoolWithTag(compatBuf, 'fCpm');
            return TRUE;
        }
        ptr += wcslen(ptr) + 1;
    }

    if (allocated) ExFreePoolWithTag(compatBuf, 'fCpm');
    return FALSE;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN
TryHardwareIdWithUniq(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum)
{
    PAGED_CODE();

    if (!Pdo || !OutDeviceId) {
        return FALSE;
    }

    /* Read HardwareId (MULTI_SZ). Prefer first element normalized. */
    WCHAR hwidStack[512] = {0};
    ULONG hwidLen = sizeof(hwidStack);
    NTSTATUS hwidStatus = IoGetDeviceProperty(
        Pdo,
        DevicePropertyHardwareId,
        hwidLen,
        hwidStack,
        &hwidLen
    );

    WCHAR *hwidBuf = hwidStack;
    BOOLEAN allocated = FALSE;

    if (hwidStatus == STATUS_BUFFER_TOO_SMALL || hwidLen > sizeof(hwidStack)) {
        hwidBuf = (WCHAR*)ExAllocatePool2(POOL_FLAG_PAGED, hwidLen, 'fHuq');
        if (!hwidBuf) {
            DbgPrint("fallback: TryHardwareIdWithUniq: ExAllocatePool2 failed len=%lu\n", hwidLen);
            return FALSE;
        }
        allocated = TRUE;
        RtlZeroMemory(hwidBuf, hwidLen);
        hwidStatus = IoGetDeviceProperty(
            Pdo,
            DevicePropertyHardwareId,
            hwidLen,
            hwidBuf,
            &hwidLen
        );
    }

    if (!NT_SUCCESS(hwidStatus) || hwidBuf[0] == L'\0') {
        DbgPrint("fallback: TryHardwareIdWithUniq: HardwareId unavailable status=0x%08X\n", hwidStatus);
        if (allocated) ExFreePoolWithTag(hwidBuf, 'fHuq');
        return FALSE;
    }

    WCHAR normHwid[512] = {0};
    if (!NT_SUCCESS(LocalNormalizeHardwareIdFirstElement(hwidBuf, normHwid, RTL_NUMBER_OF(normHwid)))) {
        RtlStringCchCopyW(normHwid, RTL_NUMBER_OF(normHwid), hwidBuf);
    }

    /* Combine HardwareId and uniqNum */
    WCHAR combined[1024];
    RtlStringCchPrintfW(combined, RTL_NUMBER_OF(combined), L"%ws_%lu", normHwid, uniqNum);

    ULONG hid = Fnv1aHash16Upper(combined);
    *OutDeviceId = hid;
    DbgPrint("fallback: TryHardwareIdWithUniq succeeded DeviceId=%08X (HardwareId+uniq=%ws)\n", hid, combined);

    if (allocated) ExFreePoolWithTag(hwidBuf, 'fHuq');
    return TRUE;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN
UseFixedValue(_In_ PDEVICE_OBJECT Pdo, _Out_ PULONG OutDeviceId, _In_ ULONG uniqNum)
{
    PAGED_CODE();
    (void)Pdo;
    if (OutDeviceId) {
        *OutDeviceId = (0xFFF2 << 16) | (uniqNum & 0xFFFF);
        DbgPrint("fallback: UseFixedValue: OutDeviceId=%08X\n", *OutDeviceId);
    }
    return TRUE;
}