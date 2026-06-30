// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#ifndef _LOG_H
#define _LOG_H

#if DBG

// Initiallize logging queue and enqueue "message" as
// first log.
void nodokaLogInit(const char *message);

// Finalize logging queue.
void nodokaLogTerm(void);

// Enqueue one message to loggin queue.
// Use printf like format to enqueue,
// following types are available.
// %x: (ULONG)unsigned long in hexadecimal
// %d: (ULONG)unsigned long in decimal
// %T: (PUNICODE)pointer to unicode string
// Notice: specifing minimal width such as "%2d"
//         is unavailable yet.
void nodokaLogEnque(const char *fmt, ...);

// Dequeue one message from logging queue to "irp".
NTSTATUS nodokaLogDeque(PIRP irp);

// Define DebugPrint
#define DebugPrint(_x_) \
	DbgPrint("nodoka: ");\
	DbgPrint _x_;

#define NODOKA_LOG_ENTRY_TAG (ULONG) 'EakN'
#define NODOKA_LOG_BUFFER_TAG (ULONG) 'BakN'

#endif // DBG

#endif // !_LOG_H
