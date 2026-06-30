// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#pragma warning(disable:4996)
#if DBG

#include <ntddk.h>
#include <stdarg.h>

#include "log.h"

#define BUF_MAX_SIZE 128

typedef struct _logEntry {
	LIST_ENTRY listEntry;
	UNICODE_STRING log;
	} logEntry;

static LIST_ENTRY s_logList;
static KSPIN_LOCK s_logListLock;
static PIRP s_irp;

void nodokaLogInit(const char *message)
	{
	InitializeListHead(&s_logList);
	KeInitializeSpinLock(&s_logListLock);
	s_irp = NULL;
	nodokaLogEnque(message);
	}

void nodokaLogTerm()
	{
	if (s_irp)
		{
		IoReleaseCancelSpinLock(s_irp->CancelIrql);
		s_irp->IoStatus.Status = STATUS_CANCELLED;
		s_irp->IoStatus.Information = 0;
		IoCompleteRequest(s_irp, IO_NO_INCREMENT);
		s_irp = NULL;
		}
	}

void nodokaLogEnque(const char *fmt, ...)
	{
	va_list argp;
	logEntry *entry;
	ANSI_STRING ansiBuf;
	UNICODE_STRING unicodeBuf;
	CHAR buf[BUF_MAX_SIZE] = "";
	WCHAR wbuf[BUF_MAX_SIZE] = L"";
	USHORT i, j;
	ULONG ul;
	PUNICODE_STRING punicode;

	entry = (logEntry*)ExAllocatePoolWithTag(NonPagedPool, sizeof(logEntry), NODOKA_LOG_ENTRY_TAG);
	if (!entry)
		return;

	RtlZeroMemory(entry, sizeof(logEntry)); 

	entry->log.Length = 0;
	entry->log.MaximumLength = BUF_MAX_SIZE;
	entry->log.Buffer = (PWSTR)ExAllocatePoolWithTag(NonPagedPool, BUF_MAX_SIZE, NODOKA_LOG_BUFFER_TAG);
	if (!(entry->log.Buffer)) {
		ExFreePoolWithTag(entry, NODOKA_LOG_ENTRY_TAG);
		return;
	}

	RtlZeroMemory(entry->log.Buffer, BUF_MAX_SIZE); 

	unicodeBuf.Length = 0;
	unicodeBuf.MaximumLength = BUF_MAX_SIZE;
	unicodeBuf.Buffer = wbuf;

	ansiBuf.Length = 0;
	ansiBuf.MaximumLength = BUF_MAX_SIZE;
	ansiBuf.Buffer = buf;

	va_start(argp, fmt);
	for (i = j = 0; i < BUF_MAX_SIZE; ++i)
		{
		ansiBuf.Buffer[i - j] = fmt[i];
		if (fmt[i] == '\0')
			{
			ansiBuf.Length = i - j;
			RtlAnsiStringToUnicodeString(&unicodeBuf, &ansiBuf, FALSE);
			RtlAppendUnicodeStringToString(&entry->log, &unicodeBuf);
			break;
			}
		if (fmt[i] == '%')
			{
			ansiBuf.Length = i - j;
			RtlAnsiStringToUnicodeString(&unicodeBuf, &ansiBuf, FALSE);
			RtlAppendUnicodeStringToString(&entry->log, &unicodeBuf);
			switch(fmt[++i])
				{
				case 'x':
					ul = va_arg(argp, ULONG);
					RtlIntegerToUnicodeString(ul, 16, &unicodeBuf);
					RtlAppendUnicodeStringToString(&entry->log, &unicodeBuf);
					break;
				case 'd':
					ul = va_arg(argp, ULONG);
					RtlIntegerToUnicodeString(ul, 10, &unicodeBuf);
					RtlAppendUnicodeStringToString(&entry->log, &unicodeBuf);
					break;
				case 'T':
					punicode = va_arg(argp, PUNICODE_STRING);
					RtlAppendUnicodeStringToString(&entry->log, punicode);
				}      
			j = i + 1;
			}
		}
	va_end(argp);

	DebugPrint(("%wZ\n", &entry->log));
	ExInterlockedInsertTailList(&s_logList, &entry->listEntry, &s_logListLock);

	if (s_irp)
		{
		KIRQL cancelIrql;

		nodokaLogDeque(s_irp);
		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(s_irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		IoCompleteRequest(s_irp, IO_NO_INCREMENT);
		s_irp = NULL;
		}
	}

VOID nodokaLogCancel(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	s_irp = NULL;
	IoReleaseCancelSpinLock(irp->CancelIrql);
	irp->IoStatus.Status = STATUS_CANCELLED;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	}

NTSTATUS nodokaLogDeque(PIRP irp)
	{
	KIRQL currentIrql;

	KeAcquireSpinLock(&s_logListLock, &currentIrql);
	if (IsListEmpty(&s_logList) == TRUE)
		{
		KIRQL cancelIrql;

		IoAcquireCancelSpinLock(&cancelIrql);
		IoMarkIrpPending(irp);
		s_irp = irp;
		IoSetCancelRoutine(irp, nodokaLogCancel);
		IoReleaseCancelSpinLock(cancelIrql);
		KeReleaseSpinLock(&s_logListLock, currentIrql);
		irp->IoStatus.Information = 0;
		irp->IoStatus.Status = STATUS_PENDING;
		}
	else
		{
		PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
		PLIST_ENTRY pListEntry;
		logEntry *pEntry;

		KeReleaseSpinLock(&s_logListLock, currentIrql);
		pListEntry = ExInterlockedRemoveHeadList(&s_logList, &s_logListLock);
		pEntry = CONTAINING_RECORD(pListEntry, logEntry, listEntry);
		RtlCopyMemory(irp->AssociatedIrp.SystemBuffer,
			pEntry->log.Buffer, pEntry->log.Length);
		irp->IoStatus.Information = pEntry->log.Length;
		irp->IoStatus.Status = STATUS_SUCCESS;
		ExFreePoolWithTag(pEntry->log.Buffer, NODOKA_LOG_BUFFER_TAG);
		ExFreePoolWithTag(pEntry, NODOKA_LOG_ENTRY_TAG);
		}
	return irp->IoStatus.Status;
	}

#endif // DBG
