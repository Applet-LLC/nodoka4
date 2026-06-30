///////////////////////////////////////////////////////////////////////////////
// keyque.c
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/
#include <ntddk.h>
#include <ntddkbd.h>
#include <devioctl.h>
#include "nodokad.h"

///////////////////////////////////////////////////////////////////////////////
// Definitions




///////////////////////////////////////////////////////////////////////////////
// Prototypes


NTSTATUS KqInitialize(KeyQue *kq);
void KqClear(KeyQue *kq);
NTSTATUS KqFinalize(KeyQue *kq);
BOOLEAN KqIsEmpty(KeyQue *kq);
ULONG KqEnque(KeyQue *kq, IN  KEYBOARD_INPUT_DATA *buf, IN ULONG lengthof_buf);
ULONG KqDeque(KeyQue *kq, OUT KEYBOARD_INPUT_DATA *buf, IN ULONG lengthof_buf);


#ifdef ALLOC_PRAGMA
#pragma alloc_text( init, KqInitialize )
#pragma alloc_text( page, KqFinalize )
#endif // ALLOC_PRAGMA

//typedef struct _KEYBOARD_INPUT_DATA {
//    USHORT UnitId;
//    USHORT MakeCode;
//    USHORT Flags;
//    USHORT Reserved;
//    ULONG ExtraInformation;
//} KEYBOARD_INPUT_DATA, *PKEYBOARD_INPUT_DATA;

///////////////////////////////////////////////////////////////////////////////
// Functions


NTSTATUS KqInitialize(KeyQue *kq)
{
	SIZE_T NumberOfBytes;
	kq->count = 0;
	kq->lengthof_que = KeyQueSize;

	NumberOfBytes = kq->lengthof_que * sizeof(KEYBOARD_INPUT_DATA);

	//
	// ExAllocatePoolWithTag is deprecated on modern WDK. Use ExAllocatePool2
	// with POOL_FLAG_NON_PAGED instead to keep the same allocation semantics.
	//
	kq->que = (KEYBOARD_INPUT_DATA*)ExAllocatePool2(POOL_FLAG_NON_PAGED, NumberOfBytes, NODOKA_POOL_TAG);

	kq->insert = kq->que;
	kq->remove = kq->que;

	if (kq->que == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	else
	{
		RtlZeroMemory(kq->que, NumberOfBytes);
		return STATUS_SUCCESS;
	}
}


void KqClear(KeyQue *kq)
{
	kq->count = 0;
	kq->insert = kq->que;
	kq->remove = kq->que;
}


NTSTATUS KqFinalize(KeyQue *kq)
{
	if (kq->que) {
		ExFreePoolWithTag(kq->que, NODOKA_POOL_TAG);
		kq->que    = NULL;
		kq->count  = 0;
		kq->insert = NULL;
		kq->remove = NULL;
	}
	return STATUS_SUCCESS;
}


BOOLEAN KqIsEmpty(KeyQue *kq)
{
	return 0 == kq->count;
}


// return: lengthof copied data
ULONG KqEnque(KeyQue *kq, IN KEYBOARD_INPUT_DATA *buf, IN ULONG lengthof_buf)
{
	SIZE_T rest;	// ULONG_PTR
	SIZE_T copy;

	if (kq->lengthof_que - kq->count < lengthof_buf) // overflow
		lengthof_buf = kq->lengthof_que - kq->count; // chop overflowed datum
	if (lengthof_buf <= 0)
		return 0;

	rest = kq->lengthof_que - (kq->insert - kq->que);
	if (rest < lengthof_buf)
	{
		copy = rest;
		if (0 < copy)
		{
			RtlMoveMemory((PCHAR)kq->insert, (PCHAR)buf,
				sizeof(KEYBOARD_INPUT_DATA) * copy);
			buf += copy;
		}
		copy = lengthof_buf - copy;
		if (0 < copy)
			RtlMoveMemory((PCHAR)kq->que, (PCHAR)buf,
			sizeof(KEYBOARD_INPUT_DATA) * copy);
		kq->insert = kq->que + copy;
	}
	else
	{
		RtlMoveMemory((PCHAR)kq->insert, (PCHAR)buf,
			sizeof(KEYBOARD_INPUT_DATA) * lengthof_buf);
		kq->insert += lengthof_buf;
	}
	kq->count += lengthof_buf;
	return lengthof_buf;
}


// return: lengthof copied data
ULONG KqDeque(KeyQue *kq, OUT KEYBOARD_INPUT_DATA *buf, IN ULONG lengthof_buf)
{
	SIZE_T rest;
	SIZE_T copy;

	if (kq->count < lengthof_buf)
		lengthof_buf = kq->count;
	if (lengthof_buf <= 0)
		return 0;

	rest = kq->lengthof_que - (kq->remove - kq->que);
	if (rest < lengthof_buf)
	{
		copy = rest;
		if (0 < copy)
		{
			RtlMoveMemory((PCHAR)buf, (PCHAR)kq->remove,
				sizeof(KEYBOARD_INPUT_DATA) * copy);
			buf += copy;
		}
		copy = lengthof_buf - copy;
		if (0 < copy)
			RtlMoveMemory((PCHAR)buf, (PCHAR)kq->que,
			sizeof(KEYBOARD_INPUT_DATA) * copy);
		kq->remove = kq->que + copy;
	}
	else
	{
		RtlMoveMemory((PCHAR)buf, (PCHAR)kq->remove,
			sizeof(KEYBOARD_INPUT_DATA) * lengthof_buf);
		kq->remove += lengthof_buf;
	}
	kq->count -= lengthof_buf;
	return lengthof_buf;
}
