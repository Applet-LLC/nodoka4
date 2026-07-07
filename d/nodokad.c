///////////////////////////////////////////////////////////////////////////////
// Driver for Nodoka for W10, W11
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/


#include <ntddk.h>
#include <ntddkbd.h>
#include <devioctl.h>

#pragma warning(3 : 4061 4100 4132 4701 4706)

#include "nodokad.h"
#include "ioctl.h"

//#include "keyque.c"
extern NTSTATUS KqInitialize(KeyQue *kq);
extern void KqClear(KeyQue *kq);
extern NTSTATUS KqFinalize(KeyQue *kq);
extern BOOLEAN KqIsEmpty(KeyQue *kq);
extern ULONG KqEnque(KeyQue *kq, IN  KEYBOARD_INPUT_DATA *buf, IN ULONG lengthof_buf);
extern ULONG KqDeque(KeyQue *kq, OUT KEYBOARD_INPUT_DATA *buf, IN ULONG lengthof_buf);


//#define W2K			// force W2K for debug.
//#define CALCADDR      // force Calc. SpinLock address
//#define LOG 1			// enable DbgPrint logging (view with DebugView)

// CALCADDR: debug-only; requires kbdclass.h. Not used in normal (Windows 11) build.
#ifdef CALCADDR
#error "CALCADDR: add path to kbdclass.h (e.g. WDK input\\kbdclass) in project IncludePath"
#endif

#define USE_TOUCHPAD // very experimental!

/*
#if DBG
// Enable debug logging only on checked build:
// We use macro to avoid function call overhead
// in non-logging case, and use double paren such
// as DEBUG_LOG((...)) because of va_list in macro.
#include "log.h"
#define DEBUG_LOG_INIT(x) nodokaLogInit x
#define DEBUG_LOG_TERM(x) nodokaLogTerm x
#define DEBUG_LOG(x) nodokaLogEnque x
#define DEBUG_LOG_RETRIEVE(x) nodokaLogDeque x
#else
*/
#if LOG
#define DEBUG_LOG_INIT(x)
#define DEBUG_LOG_TERM(x)
#define DEBUG_LOG(_x_)	DbgPrint("nodoka: ");DbgPrint _x_;
#define DEBUG_LOG_RETRIEVE(x) STATUS_INVALID_DEVICE_REQUEST
#else
#define DEBUG_LOG_INIT(x)
#define DEBUG_LOG_TERM(x)
#define DEBUG_LOG(x)
#define DEBUG_LOG_RETRIEVE(x) STATUS_INVALID_DEVICE_REQUEST
#endif

/*
#endif
*/

/*
 NODOKAD_LOG: enable NODOKAD_TRACE in a Release build (viewable in DebugView
 with "Capture Kernel" enabled). No-op in Release when left undefined.
 Enable: uncomment the next line, or add NODOKAD_LOG to the project's
 preprocessor definitions. DBG (checked) builds always have tracing active
 regardless of this switch.
*/
//#define NODOKAD_LOG

/*
 NODOKAD_TRACE: debug trace macro.
 DBG build: always active (DbgPrintEx).
 Release build with NODOKAD_LOG defined: also active (viewable in DebugView with Capture Kernel).
 Release build without NODOKAD_LOG: no-op.
*/
#if DBG
#define NODOKAD_TRACE(fmt, ...) \
    DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL, "nodokad: " fmt, ##__VA_ARGS__)
#elif defined(NODOKAD_LOG)
#define NODOKAD_TRACE(fmt, ...) \
    DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL, "nodokad: " fmt, ##__VA_ARGS__)
#else
#define NODOKAD_TRACE(fmt, ...) ((void)0)
#endif
// WDM keyboard class upper filter: we traverse device stacks and dispatch tables using
// documented DEVICE_OBJECT/DRIVER_OBJECT members (NextDevice, AttachedDevice, DeviceQueue,
// DriverName, MajorFunction). These are permitted for filter drivers (see C28175 docs).
// Suppress 28175 with this justification for driver signing.
#pragma warning(push)
#pragma warning(disable: 28175)

///////////////////////////////////////////////////////////////////////////////
// Device Extensions

struct _DetourDeviceExtension;
struct _FilterDeviceExtension;

typedef struct _DetourDeviceExtension
	{
	PDRIVER_DISPATCH MajorFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];

	KSPIN_LOCK lock; // lock below datum
	PDEVICE_OBJECT filterDevObj;
	LONG isOpen;
	BOOLEAN wasCleanupInitiated; //
	PIRP irpq;
	PFILE_OBJECT irpOwnerFileObject; // file object that issued the pending ReadFile IRP
	KeyQue readQue; // when IRP_MJ_READ, the contents of readQue are returned
	HANDLE openProcessId; // PID of the process that currently has the device open (0 = none)
	ULONG openSessionId;  // session ID of the process that currently has the device open
	// Fix I: last time the app's read loop showed proof of life, in
	// KeQueryInterruptTime units (100ns, since boot). Set on detourCreate (grace
	// period on fresh open), refreshed on every detourRead() entry, and (J2)
	// refreshed by DetourShouldIntercept whenever a detour READ is parked in irpq
	// (app blocked in ReadFile = alive, however long since the last keystroke).
	// Read/written under lock.
	ULONGLONG heartbeatTick;
	} DetourDeviceExtension;

// J1b: how long after a self-cancel a CANCELLED completion is still attributed
// to that cancel (100ns units). Must exceed the worst-case delay between
// releasing filterDevExt->lock and the IoCancelIrp call landing (thread
// preemption), yet stay short enough that a genuine external cancel (device
// teardown) racing an unrelated recent detourWrite is unlikely: 50ms.
#define SELF_CANCEL_WINDOW_100NS (50 * 10000ULL)

typedef struct _FilterDeviceExtension
	{
	PDRIVER_DISPATCH MajorFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];

	PDEVICE_OBJECT detourDevObj;
	PDEVICE_OBJECT kbdClassDevObj; // keyboard class device object

#ifdef USE_TOUCHPAD
	BOOLEAN isKeyboard;
#endif
	KSPIN_LOCK lock; // lock below datum
	PIRP irpq;
	// READ IRP this driver itself cancelled (detourCreate/detourWrite) so the
	// completion routine can tell "our" STATUS_CANCELLED (rewrite to SUCCESS so
	// win32k re-issues the READ) from an external cancel such as RIM device
	// teardown (must pass through untouched; see bugcheck 0x18 fix). Compared by
	// pointer only, never dereferenced. Guarded by lock.
	PIRP selfCancelledIrp;
	// J1b: KeQueryInterruptTime of the most recent self-cancel issued on this
	// filter (0 = never). Secondary match for filterReadCompletion's guard: a
	// CANCELLED completion arriving shortly after we issued a cancel is treated
	// as ours even when selfCancelledIrp was already consumed. Covers the narrow
	// window where our IoCancelIrp (issued outside the lock) lands on a recycled
	// IRP address after the marked IRP completed naturally. Guarded by lock.
	ULONGLONG selfCancelTick;
	KeyQue readQue; // when IRP_MJ_READ, the contents of readQue are returned
#ifdef USE_TOUCHPAD
	BOOLEAN isTouched;
#endif
	} FilterDeviceExtension;

///////////////////////////////////////////////////////////////////////////////
// Protorypes (TODO)


// DriverEntry matches DRIVER_INITIALIZE signature.
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS DriverEntry(IN PDRIVER_OBJECT, IN PUNICODE_STRING);
NTSTATUS nodokaAddDevice     (IN PDRIVER_OBJECT, IN PDEVICE_OBJECT);
VOID nodokaUnloadDriver      (IN PDRIVER_OBJECT);
VOID NodokaProcessNotifyCallback(IN PEPROCESS, IN HANDLE, IN PPS_CREATE_NOTIFY_INFO);
NTSTATUS NodokaSessionNotificationCallback(IN PVOID, IN PVOID, IN ULONG, IN PVOID, IN PVOID, IN ULONG);
_IRQL_requires_(DISPATCH_LEVEL) _IRQL_requires_same_
VOID nodokaDetourReadCancel (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS filterRemoveCompletion(IN PDEVICE_OBJECT, IN PIRP, IN PVOID);

NTSTATUS filterGenericCompletion (IN PDEVICE_OBJECT, IN PIRP, IN PVOID);
NTSTATUS filterReadCompletion    (IN PDEVICE_OBJECT, IN PIRP, IN PVOID);

// Fix I: heartbeat/staleness safety net. Must be called with detourDevExt->lock held.
// Replaces the plain "isOpen && !wasCleanupInitiated" intercept check at both call
// sites so a hung app (read loop silent) or the registry "through mode only" switch
// can force passthrough even when isOpen/wasCleanupInitiated alone would say intercept.
BOOLEAN DetourShouldIntercept(IN DetourDeviceExtension*);

_Dispatch_type_(IRP_MJ_CREATE)
_Dispatch_type_(IRP_MJ_CREATE_NAMED_PIPE)
_Dispatch_type_(IRP_MJ_PNP)
NTSTATUS nodokaGenericDispatch (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS detourCreate        (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS detourClose         (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS detourRead          (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS detourWrite         (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS detourCleanup       (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS detourDeviceControl (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS filterRead          (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS filterPassThrough   (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS detourPnP           (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS filterPnP           (IN PDEVICE_OBJECT, IN PIRP);
_Dispatch_type_(IRP_MJ_POWER)
NTSTATUS filterPower         (IN PDEVICE_OBJECT, IN PIRP);
#ifndef NODOKAD_NT4
NTSTATUS detourPower         (IN PDEVICE_OBJECT, IN PIRP);
#endif // !NODOKAD_NT4

BOOLEAN CancelKeyboardClassRead(IN PIRP, IN PDEVICE_OBJECT);
NTSTATUS readq(KeyQue*, PIRP);

#ifdef USE_TOUCHPAD
NTSTATUS filterTouchpadCompletion (IN PDEVICE_OBJECT, IN PIRP, IN PVOID);
NTSTATUS filterTouchpad      (IN PDEVICE_OBJECT, IN PIRP);
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text( init, DriverEntry )
#endif // ALLOC_PRAGMA


///////////////////////////////////////////////////////////////////////////////
// Global Constants / Variables

BOOLEAN g_isPnP;
BOOLEAN g_isW2K;
// g_SpinLock_offset and g_RequestIsPending_offset removed — avoid accessing other driver internals

// Device names
#define UnicodeString(str) { sizeof(str) - sizeof(UNICODE_NULL),	\
	sizeof(str) - sizeof(UNICODE_NULL), str }

static UNICODE_STRING NodokaDetourDeviceName =
UnicodeString(L"\\Device\\NodokaWalk0");

static UNICODE_STRING NodokaDetourWin32DeviceName =
UnicodeString(L"\\DosDevices\\NodokaWalk1");

static UNICODE_STRING KeyboardClassDeviceName =
UnicodeString(DD_KEYBOARD_DEVICE_NAME_U L"0");

static UNICODE_STRING KeyboardClassDriverName =
UnicodeString(L"\\Driver\\kbdclass");

#ifdef USE_TOUCHPAD
#define TOUCHPAD_PRESSURE_OFFSET 7
#endif

// Global Variables
PDRIVER_DISPATCH _IopInvalidDeviceRequest; // Default dispatch function

// Fix F: reference to the detour device object for the process-termination callback
static PDEVICE_OBJECT g_detourDevObj = NULL;
// Fix G: registration handle for IoRegisterContainerNotification (session disconnect/logoff)
static PVOID g_containerRegistration = NULL;

// Fix I: heartbeat/staleness safety net. Read from
// HKLM\SYSTEM\CurrentControlSet\Services\nodokad\HeartbeatTimeout (REG_DWORD) in
// DriverEntry. Disabled (0) by default -- this is a last-resort safety net for
// hangs that Fix A/B/C/G do not cover (app-level bugs unrelated to any session
// state transition), not a normally-active mechanism.
//   0       : disabled -- behavior identical to no heartbeat code at all.
//   1-254   : seconds. If the app has NO detour READ parked AND has not entered
//             detourRead() for this long while the device is supposed to be
//             intercepting (J2: a parked READ counts as proof of life and keeps
//             refreshing the tick), force passthrough.
//   255     : "through mode only" -- force passthrough unconditionally, regardless
//             of isOpen/wasCleanupInitiated/timer. Emergency kill switch reachable
//             via the registry alone, without reinstalling the driver.
static ULONG g_heartbeatTimeoutSec = 0;

// Fix F: process termination callback — fires when any process exits.
// If the exiting process is the one that has the detour device open, set
// wasCleanupInitiated=TRUE immediately so filterReadCompletion stops
// diverting keyboard input before the login screen for the next user appears.
// This fires synchronously in kernel context, before user-mode handle cleanup.
VOID NodokaProcessNotifyCallback(
	PEPROCESS Process,
	HANDLE ProcessId,
	PPS_CREATE_NOTIFY_INFO CreateInfo)
	{
	UNREFERENCED_PARAMETER(Process);
	if (CreateInfo != NULL) return; // Ignore process creation; only care about termination
	if (g_detourDevObj == NULL) return;

	DetourDeviceExtension *devExt =
		(DetourDeviceExtension*)g_detourDevObj->DeviceExtension;

	KIRQL irql;
	KeAcquireSpinLock(&devExt->lock, &irql);
	if (devExt->openProcessId == ProcessId)
		{
		devExt->wasCleanupInitiated = TRUE;
		// openProcessId is cleared in detourCleanup; leave it here so that
		// the normal CLEANUP/CLOSE IRP sequence can still decrement isOpen correctly.
		}
	KeReleaseSpinLock(&devExt->lock, irql);
	}

// Fix G: session disconnect/logoff notification — fires in kernel context when the session
// that has the detour device open is disconnected or logs off, BEFORE the login screen.
// Registering with IoObject=detourDevObj scopes this to sessions with the device open,
// but a late-firing notification can still arrive after a new session has re-opened
// (Fix 6: use session-ID comparison to reject stale notifications).
NTSTATUS NodokaSessionNotificationCallback(
	PVOID SessionObject,
	PVOID IoObject,
	ULONG Event,
	PVOID Context,
	PVOID NotificationPayload,
	ULONG PayloadLength)
	{
	PDEVICE_OBJECT devObj = (PDEVICE_OBJECT)Context;
	DetourDeviceExtension *devExt = (DetourDeviceExtension*)devObj->DeviceExtension;
	KIRQL irql;

	UNREFERENCED_PARAMETER(IoObject);
	UNREFERENCED_PARAMETER(NotificationPayload);
	UNREFERENCED_PARAMETER(PayloadLength);
	UNREFERENCED_PARAMETER(Event); // only used by DEBUG_LOG/NODOKAD_TRACE, both no-ops in Release without NODOKAD_LOG

	DEBUG_LOG(("nodokad: NodokaSessionNotificationCallback event=%x", Event));

	// Fix 6: get the disconnecting session's ID before acquiring the spinlock.
	// IoGetContainerInformation requires IRQL <= APC_LEVEL.
	IO_SESSION_STATE_INFORMATION sessionInfo = {};
	NTSTATUS sessionStatus = IoGetContainerInformation(
		IoSessionStateInformation, SessionObject, &sessionInfo, sizeof(sessionInfo));

	KeAcquireSpinLock(&devExt->lock, &irql);
	if (NT_SUCCESS(sessionStatus)) {
		// Fix 6: set wasCleanupInitiated only when BOTH conditions are true:
		// (1) isOpen<=1: no other session still has the device open (multi-RDP safety).
		//     With 3+ simultaneous sessions, the last opener's disconnect should not
		//     cut keyboard access for sessions that opened earlier.
		// (2) openSessionId matches the disconnecting session: prevents a stale
		//     notification (fired after a new session re-opened) from overriding the
		//     fresh open's wasCleanupInitiated=FALSE (the original Case 5 race).
		// Together these replace the original isOpen<=1-only guard which failed Case 5,
		// while also correctly handling concurrent RDP sessions.
		NODOKAD_TRACE("SessionNotify: event=%lu sessId=%lu openSessId=%lu isOpen=%ld\n",
			Event, sessionInfo.SessionId, devExt->openSessionId, devExt->isOpen);
		if (devExt->isOpen <= 1 && devExt->openSessionId == sessionInfo.SessionId) {
			devExt->wasCleanupInitiated = TRUE;
			NODOKAD_TRACE("SessionNotify: -> wasCleanupInitiated=TRUE\n");
		} else {
			NODOKAD_TRACE("SessionNotify: -> unchanged (isOpen=%ld idMatch=%d)\n",
				devExt->isOpen, (devExt->openSessionId == sessionInfo.SessionId));
		}
	} else {
		// Fallback if IoGetContainerInformation fails (should not happen on Win7+):
		// use the original isOpen guard to avoid completely losing the protection.
		NODOKAD_TRACE("SessionNotify: event=%lu getInfo failed=%08lx isOpen=%ld\n",
			Event, sessionStatus, devExt->isOpen);
		if (devExt->isOpen <= 1) {
			devExt->wasCleanupInitiated = TRUE;
			NODOKAD_TRACE("SessionNotify: -> wasCleanupInitiated=TRUE (fallback)\n");
		}
	}
	KeReleaseSpinLock(&devExt->lock, irql);

	return STATUS_SUCCESS;
	}


#define NODOKAD_MODE L""
static UNICODE_STRING NodokaDriverVersion =
UnicodeString(L"$Revision: 1.40 $" NODOKAD_MODE);



///////////////////////////////////////////////////////////////////////////////
// Entry / Unload

void DEBUG_LOGChain(PDRIVER_OBJECT driverObject)
	{
	PDEVICE_OBJECT deviceObject = driverObject->DeviceObject;

	if (deviceObject)
		{
		while (deviceObject->NextDevice)
			{
			DEBUG_LOG(("nodokad: %x->", deviceObject));
			deviceObject = deviceObject->NextDevice;
			}
		DEBUG_LOG(("nodokad: %x", deviceObject));
		}
	return;
	}

// initialize driver (signature matches DRIVER_INITIALIZE; typedef is pointer type so cannot declare function with it)
#pragma warning(suppress: 28101)
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS DriverEntry(IN PDRIVER_OBJECT driverObject,
										 IN PUNICODE_STRING registryPath)
	{
	// DbgBreakPoint();  // DriverEntry breakpoint for debugging; remove or comment out for production
	NTSTATUS status;
	BOOLEAN is_symbolicLinkCreated = FALSE;
	ULONG i;
	PDEVICE_OBJECT detourDevObj = NULL;
	DetourDeviceExtension *detourDevExt = NULL;
	ULONG start = 0;
	ULONG heartbeatTimeoutValue = 0; // Fix I: default 0 = disabled when the value is absent
	RTL_QUERY_REGISTRY_TABLE query[3];

	UNREFERENCED_PARAMETER(registryPath);

	DEBUG_LOG_INIT(("nodokad: DriverEntry. start logging"));

	// Environment specific initialize
	RtlZeroMemory(query, sizeof(query));
	query[0].Name = L"Start";
	query[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
	query[0].EntryContext = &start;
	// Fix I: HKLM\SYSTEM\CurrentControlSet\Services\nodokad\HeartbeatTimeout (REG_DWORD).
	// 0=disabled (default), 1-254=timeout seconds, 255=through mode only. See
	// g_heartbeatTimeoutSec comment for details.
	query[1].Name = L"HeartbeatTimeout";
	query[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
	query[1].EntryContext = &heartbeatTimeoutValue;
	RtlQueryRegistryValues(RTL_REGISTRY_SERVICES, L"nodokad", query, NULL, NULL);
	g_heartbeatTimeoutSec = heartbeatTimeoutValue;
	DEBUG_LOG(("nodokad: HeartbeatTimeout=%lu", g_heartbeatTimeoutSec));
	if (start == 0x03) {
		g_isPnP = TRUE;
		DEBUG_LOG(("nodokad: is PnP"));
		} else {
			g_isPnP = FALSE;
			DEBUG_LOG(("nodokad: is not PnP"));
		}
#ifdef NODOKAD_NT4
	g_isXp = FALSE;
	/* removed reliance on target driver's internal offsets */
#else /* !NODOKAD_NT4 */
	if (IoIsWdmVersionAvailable(6, 0x00)) { // is Windows Vista
	DEBUG_LOG(("nodokad: is Windows Vista"));
	g_isW2K = FALSE;
	/* removed use of undocumented offsets */
#ifndef AMD64
	/* no target-driver offset assumptions on x86 */
#else
	/* no target-driver offset assumptions on AMD64 */
#endif
	} else if (IoIsWdmVersionAvailable(1, 0x20)) { // is Windows XP
			DEBUG_LOG(("nodokad: is Windows XP"));
			g_isW2K = FALSE;
			/* removed use of undocumented offsets */
#ifndef AMD64
			/* no target-driver offset assumptions on x86 */
#else
			/* no target-driver offset assumptions on AMD64 */
#endif
		} else if (IoIsWdmVersionAvailable(1, 0x10)) { // is Windows 2000
			DEBUG_LOG(("nodokad: is Windows 2000"));
			g_isW2K = TRUE;
			/* removed use of undocumented offsets */
			} else { // Unknown version
				DEBUG_LOG(("nodokad: unknown Windows"));
				status = STATUS_UNKNOWN_REVISION;
				goto error;
			}
#endif /* NODOKAD_NT4 */

		// initialize global variables
		_IopInvalidDeviceRequest = driverObject->MajorFunction[IRP_MJ_CREATE];

		// set major functions
		driverObject->DriverUnload = nodokaUnloadDriver;
		if (g_isPnP == TRUE) {
			driverObject->DriverExtension->AddDevice = nodokaAddDevice;
			}
		for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
#ifdef NODOKAD_NT4
			if (i != IRP_MJ_POWER)
#endif // NODOKAD_NT4
				driverObject->MajorFunction[i] = nodokaGenericDispatch;
		if (g_isPnP == TRUE) {
			driverObject->MajorFunction[IRP_MJ_PNP] = nodokaGenericDispatch;
			}
		driverObject->MajorFunction[IRP_MJ_POWER] = filterPower;


		// create a device
			{
			// create detour device
			status = IoCreateDevice(driverObject, sizeof(DetourDeviceExtension),
				&NodokaDetourDeviceName, FILE_DEVICE_KEYBOARD,
				0, FALSE, &detourDevObj);

			if (!NT_SUCCESS(status)) goto error;
			DEBUG_LOG(("nodokad: create detour device: %x", detourDevObj));
			DEBUG_LOGChain(driverObject);
			detourDevObj->Flags |= DO_BUFFERED_IO;
#ifndef NODOKAD_NT4
			detourDevObj->Flags |= DO_POWER_PAGABLE;
#endif // !NODOKAD_NT4

			// initialize detour device extension
			detourDevExt = (DetourDeviceExtension*)detourDevObj->DeviceExtension;
			RtlZeroMemory(detourDevExt, sizeof(DetourDeviceExtension));
			detourDevExt->filterDevObj = NULL;

			KeInitializeSpinLock(&detourDevExt->lock);
			detourDevExt->isOpen = FALSE;
			detourDevExt->wasCleanupInitiated = FALSE;
			detourDevExt->irpq = NULL;
			detourDevExt->irpOwnerFileObject = NULL;
			detourDevExt->openProcessId = NULL;
			status = KqInitialize(&detourDevExt->readQue);
			if (!NT_SUCCESS(status)) goto error;

			// Fix F: register process-termination callback and store detour device reference
			g_detourDevObj = detourDevObj;
			{
			NTSTATUS cbStatus = PsSetCreateProcessNotifyRoutineEx(
				NodokaProcessNotifyCallback, FALSE);
			if (!NT_SUCCESS(cbStatus))
				DEBUG_LOG(("nodokad: PsSetCreateProcessNotifyRoutineEx failed: %x", cbStatus));
			}

			// Fix G: register session disconnect/logoff notification.
			// IoObject=detourDevObj scopes the callback to sessions with the device open.
			// Note: IoSessionStateNotification's EventMask only supports CREATION,
			// TERMINATION, CONNECT, DISCONNECT, LOGON, LOGOFF (see
			// IO_SESSION_STATE_VALID_EVENT_MASK in wdm.h) -- there is no LOCK/UNLOCK
			// event in this kernel API, so a plain Win+L lock (session stays connected
			// and logged on) cannot be caught here at all. That gap is covered instead
			// by the heartbeat safety net (Fix I, g_heartbeatTimeoutSec) below.
			{
			IO_SESSION_STATE_NOTIFICATION sessionNotif;
			NTSTATUS cbStatus;
			RtlZeroMemory(&sessionNotif, sizeof(sessionNotif));
			sessionNotif.Size = sizeof(sessionNotif);
			sessionNotif.IoObject = detourDevObj;
			sessionNotif.EventMask = IO_SESSION_STATE_DISCONNECT_EVENT |
			                         IO_SESSION_STATE_LOGOFF_EVENT;
			sessionNotif.Context = detourDevObj;
			cbStatus = IoRegisterContainerNotification(
				IoSessionStateNotification,
				(PIO_CONTAINER_NOTIFICATION_FUNCTION)NodokaSessionNotificationCallback,
				&sessionNotif,
				sizeof(sessionNotif),
				&g_containerRegistration);
			if (!NT_SUCCESS(cbStatus))
				DEBUG_LOG(("nodokad: IoRegisterContainerNotification failed: %x", cbStatus));
			}

			// create symbolic link for detour
			status =
				IoCreateSymbolicLink(&NodokaDetourWin32DeviceName, &NodokaDetourDeviceName);
			if (!NT_SUCCESS(status)) goto error;
			is_symbolicLinkCreated = TRUE;

			if (g_isPnP == FALSE)
				// attach filter device to keyboard class device
				{
				PFILE_OBJECT f;
				PDEVICE_OBJECT kbdClassDevObj;

				status = IoGetDeviceObjectPointer(&KeyboardClassDeviceName,
					FILE_ALL_ACCESS, &f,
					&kbdClassDevObj);
				if (!NT_SUCCESS(status)) goto error;
				ObDereferenceObject(f);
				status = nodokaAddDevice(driverObject, kbdClassDevObj);

				// why cannot I do below ?
				//      status = IoAttachDevice(filterDevObj, &KeyboardClassDeviceName,
				//			      &filterDevExt->kbdClassDevObj);
				if (!NT_SUCCESS(status)) goto error;
				}

			// initialize Major Functions
			for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
				{
				detourDevExt->MajorFunction[i] = _IopInvalidDeviceRequest;
				}

			detourDevExt->MajorFunction[IRP_MJ_READ] = detourRead;
			detourDevExt->MajorFunction[IRP_MJ_WRITE] = detourWrite;
			detourDevExt->MajorFunction[IRP_MJ_CREATE] = detourCreate;
			detourDevExt->MajorFunction[IRP_MJ_CLOSE] = detourClose;
			detourDevExt->MajorFunction[IRP_MJ_CLEANUP] = detourCleanup;
			detourDevExt->MajorFunction[IRP_MJ_DEVICE_CONTROL] = detourDeviceControl;

#ifndef NODOKAD_NT4
			detourDevExt->MajorFunction[IRP_MJ_POWER] = detourPower;
#endif // !NODOKAD_NT4
			if (g_isPnP == TRUE) {
				detourDevExt->MajorFunction[IRP_MJ_PNP] = detourPnP;
				}
			}
			detourDevObj->Flags &= ~DO_DEVICE_INITIALIZING;  

			return STATUS_SUCCESS;

error:
				{
				if (is_symbolicLinkCreated)
					IoDeleteSymbolicLink(&NodokaDetourWin32DeviceName);
				if (detourDevObj)
					{
					KqFinalize(&detourDevExt->readQue);
					IoDeleteDevice(detourDevObj);
					}
				}
				return status;
	}

NTSTATUS nodokaAddDevice(IN PDRIVER_OBJECT driverObject,
											 IN PDEVICE_OBJECT kbdClassDevObj)
	{
	NTSTATUS status;
	PDEVICE_OBJECT devObj;
	PDEVICE_OBJECT filterDevObj;
	PDEVICE_OBJECT attachedDevObj;
	DetourDeviceExtension *detourDevExt;
	FilterDeviceExtension *filterDevExt;
	ULONG i;

	DEBUG_LOG(("nodokad: nodokaAddDevice()"));
	DEBUG_LOG(("nodokad: attach to device: %x", kbdClassDevObj));
	DEBUG_LOG(("nodokad: type of device: %x", kbdClassDevObj->DeviceType));
	DEBUG_LOG(("nodokad: name of driver: %T", &(kbdClassDevObj->DriverObject->DriverName)));

	// create filter device
	status = IoCreateDevice(driverObject, sizeof(FilterDeviceExtension),
		NULL, FILE_DEVICE_KEYBOARD,
		0, FALSE, &filterDevObj);
	DEBUG_LOG(("nodokad: add filter device: %x", filterDevObj));
	DEBUG_LOGChain(driverObject);
	if (!NT_SUCCESS(status)) return status;
	filterDevObj->Flags |= DO_BUFFERED_IO;
#ifndef NODOKAD_NT4
	filterDevObj->Flags |= DO_POWER_PAGABLE;
#endif // !NODOKAD_NT4

	// initialize filter device extension
	filterDevExt = (FilterDeviceExtension*)filterDevObj->DeviceExtension;
	RtlZeroMemory(filterDevExt, sizeof(FilterDeviceExtension));

	KeInitializeSpinLock(&filterDevExt->lock);
	filterDevExt->irpq = NULL;
	status = KqInitialize(&filterDevExt->readQue);
	if (!NT_SUCCESS(status)) goto error;
#ifdef USE_TOUCHPAD
	filterDevExt->isKeyboard = FALSE;
	filterDevExt->isTouched = FALSE;
#endif

	attachedDevObj = kbdClassDevObj->AttachedDevice;
	while (attachedDevObj)
		{
		DEBUG_LOG(("nodokad: attached to %T", &(attachedDevObj->DriverObject->DriverName)));
		DEBUG_LOG(("nodokad: type of attched device: %x", attachedDevObj->DeviceType));
#ifdef USE_TOUCHPAD
		if (RtlCompareUnicodeString(&KeyboardClassDriverName, &attachedDevObj->DriverObject->DriverName, TRUE) == 0)
			filterDevExt->isKeyboard = TRUE;
#endif
		attachedDevObj = attachedDevObj->AttachedDevice;
		}

	devObj = filterDevObj->NextDevice;
	while (devObj->NextDevice) {
		devObj = devObj->NextDevice;
		}
	filterDevExt->detourDevObj = devObj;
	detourDevExt = (DetourDeviceExtension*)devObj->DeviceExtension;
	if (!detourDevExt->filterDevObj) {
		detourDevExt->filterDevObj = filterDevObj;
		}

	filterDevExt->kbdClassDevObj =
		IoAttachDeviceToDeviceStack(filterDevObj, kbdClassDevObj);
	if (!filterDevExt->kbdClassDevObj) goto error;

	for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
		filterDevExt->MajorFunction[i] =
			(filterDevExt->kbdClassDevObj->DriverObject->MajorFunction[i]
		== _IopInvalidDeviceRequest) ?
_IopInvalidDeviceRequest : filterPassThrough;
		}
#ifdef USE_TOUCHPAD
	if (filterDevExt->isKeyboard == FALSE)
		{
		DEBUG_LOG(("nodokad: filter read: GlidePoint"));
		filterDevObj->DeviceType = FILE_DEVICE_MOUSE;
		filterDevExt->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = filterTouchpad;
		}
	else
#endif
		{
		DEBUG_LOG(("nodokad: filter read: Keyboard"));
		filterDevExt->MajorFunction[IRP_MJ_READ] = filterRead;
		}
#ifndef NODOKAD_NT4
	filterDevExt->MajorFunction[IRP_MJ_POWER] = filterPower;
#endif // !NODOKAD_NT4
	if (g_isPnP == TRUE) {
		filterDevExt->MajorFunction[IRP_MJ_PNP] = filterPnP;
		}
	filterDevObj->Flags &= ~DO_DEVICE_INITIALIZING;  

	return STATUS_SUCCESS;

error:
	DEBUG_LOG(("nodokad: nodokaAddDevice: error"));
	if (filterDevObj) {
		KqFinalize(&filterDevExt->readQue);
		IoDeleteDevice(filterDevObj);
		}
	return status;
	}

BOOLEAN CancelKeyboardClassRead(PIRP cancelIrp, PDEVICE_OBJECT kbdClassDevObj)
	{
	UNREFERENCED_PARAMETER(kbdClassDevObj);

	DEBUG_LOG(("nodokad: CancelKeyboardClassRead()"));

	// Use documented API to cancel the IRP. IoCancelIrp acquires the cancel spin lock
	// internally and returns TRUE if it successfully set the IRP to be canceled.
	if (cancelIrp == NULL) {
		DEBUG_LOG(("nodokad: CancelKeyboardClassRead: null irp"));
		return FALSE;
	}

	// Rely on IoCancelIrp instead of peeking into other driver's device extension.
	// This avoids accessing undocumented offsets and is compatible with modern Windows.
	return IoCancelIrp(cancelIrp);
	}

// unload driver
VOID nodokaUnloadDriver(IN PDRIVER_OBJECT driverObject)
	{
	KIRQL currentIrql;
	PIRP cancelIrp;
	PDEVICE_OBJECT devObj;
	DetourDeviceExtension *detourDevExt;

	DEBUG_LOG(("nodokad: nodokaUnloadDriver"));

	// walk on device chain(the last one is detour device?)
	devObj = driverObject->DeviceObject;
	while (devObj->NextDevice) {
		FilterDeviceExtension *filterDevExt
			= (FilterDeviceExtension*)devObj->DeviceExtension;
		PDEVICE_OBJECT delObj;
		PDEVICE_OBJECT kbdClassDevObj;

		// detach
		IoDetachDevice(filterDevExt->kbdClassDevObj);
		// cancel filter IRP_MJ_READ
		KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);
		// TODO: at this point, the irp may be completed (but what can I do for it ?)
		// finalize read que
		KqFinalize(&filterDevExt->readQue);
		cancelIrp = filterDevExt->irpq;
		filterDevExt->irpq = NULL;
		kbdClassDevObj = filterDevExt->kbdClassDevObj;
		KeReleaseSpinLock(&filterDevExt->lock, currentIrql);
		if (cancelIrp) {
			// Retry with a limit: IoCancelIrp returns FALSE when the cancel routine
			// has already been cleared (IRP is completing). Spinning forever would
			// hang the system; give the completing thread time to finish.
			int retry = 100;
			LARGE_INTEGER interval;
			interval.QuadPart = -1000; // 100us in 100-nanosecond units
			while (!CancelKeyboardClassRead(cancelIrp, kbdClassDevObj) && --retry > 0)
				KeDelayExecutionThread(KernelMode, FALSE, &interval);
			}
		// delete device objects
		delObj= devObj;
		devObj = devObj->NextDevice;
		IoDeleteDevice(delObj);
		}

	detourDevExt = (DetourDeviceExtension*)devObj->DeviceExtension;
	// cancel filter IRP_MJ_READ
	KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
	// TODO: at this point, the irp may be completed (but what can I do for it ?)
	cancelIrp = detourDevExt->irpq;
	// clear stored pointer under lock to avoid races with concurrent cancel routine
	detourDevExt->irpq = NULL;
	// finalize read que
	KqFinalize(&detourDevExt->readQue);
	KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
	if (cancelIrp)
		IoCancelIrp(cancelIrp);

	// Fix G: unregister session notification before freeing device memory (use-after-free guard)
	if (g_containerRegistration)
		{
		IoUnregisterContainerNotification(g_containerRegistration);
		g_containerRegistration = NULL;
		}

	// Fix F: unregister process-termination callback and clear device reference
	// (must be done before device memory is freed)
	g_detourDevObj = NULL;
	PsSetCreateProcessNotifyRoutineEx(NodokaProcessNotifyCallback, TRUE);

	// delete device objects (after all callbacks are unregistered)
	IoDeleteDevice(devObj);

	// delete symbolic link
	IoDeleteSymbolicLink(&NodokaDetourWin32DeviceName);
	DEBUG_LOG_TERM(());
	}


///////////////////////////////////////////////////////////////////////////////
// Cancel Functionss


// detour read cancel (device lock released on all paths; called at DISPATCH_LEVEL)
#pragma warning(suppress: 28166)
_IRQL_requires_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID nodokaDetourReadCancel(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	DetourDeviceExtension *devExt =
		(DetourDeviceExtension *)deviceObject->DeviceExtension;
	KIRQL oldIrql;

	DEBUG_LOG(("nodokad: detourReadCancel()"));

	// Cancel routine is invoked with the cancel spin lock held.
	// Use the recommended pattern: atomically clear the cancel routine,
	// release the cancel spin lock, then acquire the device lock to
	// safely manipulate device state. This prevents deadlocks caused by
	// inconsistent lock ordering (device lock <-> cancel spin lock).
	if (IoSetCancelRoutine(irp, NULL) == NULL) {
		// Another thread already cleared the cancel routine or it's being completed.
		// Release cancel spin lock (held by caller) and return.
		IoReleaseCancelSpinLock(irp->CancelIrql);
		return;
	}

	// We successfully cleared the cancel routine while holding the cancel spin lock.
	// Release the cancel spin lock before acquiring the device lock to avoid
	// inversion with code paths that acquire the device lock first then the cancel spin lock.
	IoReleaseCancelSpinLock(irp->CancelIrql);

	KeAcquireSpinLock(&devExt->lock, &oldIrql);

	// Clear driver's stored IRP pointer if it references this IRP.
	if (devExt->irpq == irp) {
		devExt->irpq = NULL;
		devExt->irpOwnerFileObject = NULL;
	}

	KeReleaseSpinLock(&devExt->lock, oldIrql);

	irp->IoStatus.Status = STATUS_CANCELLED;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_KEYBOARD_INCREMENT);
	}

///////////////////////////////////////////////////////////////////////////////
// Complete Functions


// 
NTSTATUS filterGenericCompletion(IN PDEVICE_OBJECT deviceObject,
																 IN PIRP irp, IN PVOID context)
	{
	UNREFERENCED_PARAMETER(deviceObject);
	UNREFERENCED_PARAMETER(context);

	DEBUG_LOG(("nodokad: filterGenericCompletion()"));

	if (irp->PendingReturned)
		IoMarkIrpPending(irp);
	return STATUS_SUCCESS;
	}

/*
 * Completion routine for IRP_MN_REMOVE_DEVICE / IRP_MN_SURPRISE_REMOVAL forwarded
 * to the lower driver. Perform filter cleanup here once lower driver has completed.
 */
NTSTATUS filterRemoveCompletion(IN PDEVICE_OBJECT deviceObject, IN PIRP irp, IN PVOID context)
	{
	PDEVICE_OBJECT filterDevObj = (PDEVICE_OBJECT)context;
	FilterDeviceExtension *filterDevExt;
	DetourDeviceExtension *detourDevExt;
	KIRQL currentIrql;
	PIRP cancelIrp = NULL;
	PDRIVER_OBJECT driverObject;

	UNREFERENCED_PARAMETER(deviceObject);

	if (irp->PendingReturned)
		IoMarkIrpPending(irp);

	filterDevExt = (FilterDeviceExtension*)filterDevObj->DeviceExtension;
	driverObject = filterDevObj->DriverObject;

	DEBUG_LOG(("nodokad: filterRemoveCompletion() status=%x", irp->IoStatus.Status));

	// If remove succeeded, clean up association in detour device
	detourDevExt = (DetourDeviceExtension*)filterDevExt->detourDevObj->DeviceExtension;
	KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
	if (detourDevExt->filterDevObj == filterDevObj) {
		detourDevExt->filterDevObj = NULL;
	}
	KeReleaseSpinLock(&detourDevExt->lock, currentIrql);

	// Perform filter-specific cleanup: detach, finalize queues, cancel pending IRP
	KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);
	cancelIrp = filterDevExt->irpq;
	filterDevExt->irpq = NULL;
	KqFinalize(&filterDevExt->readQue);
	KeReleaseSpinLock(&filterDevExt->lock, currentIrql);

	if (cancelIrp) {
		IoCancelIrp(cancelIrp);
	}

	// Detach from lower device stack and delete this filter device
	if (filterDevExt->kbdClassDevObj) {
		IoDetachDevice(filterDevExt->kbdClassDevObj);
	}
	IoDeleteDevice(filterDevObj);
	DEBUG_LOG(("nodokad: delete filter device: %x", filterDevObj));
	DEBUG_LOGChain(driverObject);

	// Propagate completion status
	return irp->IoStatus.Status;
	}


// Fix I: heartbeat/staleness safety net (see g_heartbeatTimeoutSec comment above).
// Callable at DISPATCH_LEVEL; caller must hold detourDevExt->lock.
BOOLEAN DetourShouldIntercept(IN DetourDeviceExtension *detourDevExt)
	{
	if (g_heartbeatTimeoutSec == 255)
		return FALSE; // registry kill switch: through mode only

	if (!detourDevExt->isOpen || detourDevExt->wasCleanupInitiated)
		return FALSE;

	if (g_heartbeatTimeoutSec != 0) {
		// J2: a parked detour READ proves the app is alive (blocked in ReadFile),
		// no matter how long ago the last keystroke was. Refresh the tick so the
		// timeout only measures time during which the app has NO read outstanding
		// and has not entered detourRead() -- i.e. a genuinely stalled read loop.
		// Without this, heartbeatTick went stale during any normal typing pause
		// longer than the timeout; the first key afterwards then leaked through
		// raw, the app's parked READ never completed, the tick never refreshed,
		// and interception never resumed (sticky passthrough).
		if (detourDevExt->irpq != NULL) {
			detourDevExt->heartbeatTick = KeQueryInterruptTime();
			} else {
				ULONGLONG now = KeQueryInterruptTime();
				ULONGLONG elapsed100ns = now - detourDevExt->heartbeatTick;
				if (elapsed100ns > (ULONGLONG)g_heartbeatTimeoutSec * 10000000ULL) {
					NODOKAD_TRACE("DetourShouldIntercept: heartbeat timeout (%lu s) exceeded -> passthrough\n",
						g_heartbeatTimeoutSec);
					return FALSE;
					}
			}
		}

	return TRUE;
	}


//
NTSTATUS filterReadCompletion(IN PDEVICE_OBJECT deviceObject,
															IN PIRP irp, IN PVOID context)
	{
	NTSTATUS status;
	KIRQL currentIrql, cancelIrql;
	BOOLEAN selfCancelled;
	BOOLEAN recentSelfCancel = FALSE;
	FilterDeviceExtension *filterDevExt =
		(FilterDeviceExtension*)deviceObject->DeviceExtension;
	PDEVICE_OBJECT detourDevObj = filterDevExt->detourDevObj;
	DetourDeviceExtension *detourDevExt =
		(DetourDeviceExtension*)detourDevObj->DeviceExtension;

	UNREFERENCED_PARAMETER(context);

	DEBUG_LOG(("nodokad: filterReadCompletion()"));

	KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);
	filterDevExt->irpq = NULL;
	// Consume the self-cancel mark unconditionally: this IRP is completing, so
	// the pointer must not linger and false-match a future IRP at the same address.
	selfCancelled = (filterDevExt->selfCancelledIrp == irp);
	if (selfCancelled)
		filterDevExt->selfCancelledIrp = NULL;
	// J1b: secondary self-cancel attribution. If detourCreate/detourWrite issued
	// a cancel on this filter only moments ago, a CANCELLED completing now is
	// almost certainly the fallout of that cancel (possibly landed on a recycled
	// IRP address after the marked IRP completed naturally) and must be rewritten
	// below like a direct match, or win32k stops re-issuing READs for this device.
	if (filterDevExt->selfCancelTick != 0 &&
	    KeQueryInterruptTime() - filterDevExt->selfCancelTick < SELF_CANCEL_WINDOW_100NS)
		recentSelfCancel = TRUE;
	KeReleaseSpinLock(&filterDevExt->lock, currentIrql);
	if (irp->PendingReturned) {
		status = STATUS_PENDING;
		IoMarkIrpPending(irp);
		} else {
			status = STATUS_SUCCESS;
		}

	// Pass unsuccessful completions through untouched — EXCEPT cancellations this
	// driver issued itself. detourCreate/detourWrite cancel the parked READ to
	// take over the device / to deliver injected keys; win32k does not re-issue
	// a READ after seeing STATUS_CANCELLED, so those self-cancels must keep being
	// rewritten to STATUS_SUCCESS below or the keyboard goes dead the moment the
	// detour opens. When RIM (win32kbase) tears down a raw input device it cancels
	// its pending READ itself and must see the cancellation status; rewriting that
	// external STATUS_CANCELLED to STATUS_SUCCESS makes RIM's teardown
	// over-dereference its RawInputManager object (bugcheck 0x18
	// REFERENCE_BY_POINTER in rimDereferenceDev).
	if (!NT_SUCCESS(irp->IoStatus.Status) &&
	    !(irp->IoStatus.Status == STATUS_CANCELLED &&
	      (selfCancelled || recentSelfCancel))) {
		NODOKAD_TRACE("filterReadCompletion: status=0x%08X -> passthrough (unsuccessful, external)\n",
			irp->IoStatus.Status);
		return irp->IoStatus.Status;
		}

	KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);

	{
	BOOLEAN shouldIntercept = DetourShouldIntercept(detourDevExt);
	NODOKAD_TRACE("filterReadCompletion: isOpen=%ld wasCleanupInitiated=%d -> %s\n",
		detourDevExt->isOpen, detourDevExt->wasCleanupInitiated,
		shouldIntercept ? "intercept" : "passthrough");

	if (shouldIntercept)
		{
		// if detour is opened, key datum are forwarded to detour
			if (irp->IoStatus.Status == STATUS_SUCCESS)
			{
#if DBG
			{
			ULONG nKeys = (ULONG)irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA);
			PKEYBOARD_INPUT_DATA kd = (PKEYBOARD_INPUT_DATA)irp->AssociatedIrp.SystemBuffer;
			KqEnque(&detourDevExt->readQue, kd, nKeys);
			for (ULONG _di = 0; _di < nKeys; _di++)
				NODOKAD_TRACE("filterReadCompletion enqueue[%lu] sc=0x%X flags=0x%X ExtraInfo=0x%08X\n",
				              _di, kd[_di].MakeCode, kd[_di].Flags, kd[_di].ExtraInformation);
			}
#else
			KqEnque(&detourDevExt->readQue,
				(KEYBOARD_INPUT_DATA *)irp->AssociatedIrp.SystemBuffer,
				(ULONG)irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA));
#endif

			irp->IoStatus.Status = STATUS_CANCELLED;
			irp->IoStatus.Information = 0;
			detourDevExt->filterDevObj = deviceObject;
			}

		IoAcquireCancelSpinLock(&cancelIrql);

		if (detourDevExt->irpq && !KqIsEmpty(&detourDevExt->readQue)) {
			PIRP savedIrp = detourDevExt->irpq;
			// Fix 2: claim the IRP before dequeuing to prevent data loss.
			// If we dequeued first and then found the IRP was being cancelled,
			// the dequeued keys would be silently discarded.
			if (IoSetCancelRoutine(savedIrp, NULL) != NULL) {
				detourDevExt->irpq = NULL;
				IoReleaseCancelSpinLock(cancelIrql);
				readq(&detourDevExt->readQue, savedIrp);
				KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
				IoCompleteRequest(savedIrp, IO_KEYBOARD_INCREMENT);
				goto _filterReadCompletion_after_detour_irp_unlocked;
			}
			// IoSetCancelRoutine returned NULL: cancel routine is running.
			// Data remains in readQue; the cancel routine will complete the IRP.
		}
		IoReleaseCancelSpinLock(cancelIrql);
		KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
_filterReadCompletion_after_detour_irp_unlocked:

		KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);
		status = readq(&filterDevExt->readQue, irp);
		if (status == STATUS_PENDING) {
			// No inject data: complete with STATUS_SUCCESS/0 bytes.
			// Win10 x86 win32k does not re-issue READ after STATUS_CANCELLED but
			// does re-issue after STATUS_SUCCESS with zero bytes (NT_SUCCESS=TRUE).
			// Sync drop investigation: note that filterDevExt->irpq is NOT re-armed
			// here. Until kbdclass issues a brand new IRP_MJ_READ (-> filterRead()),
			// any further detourWrite() in the meantime has nothing to wake.
			NODOKAD_TRACE("filterReadCompletion: queue empty at drain, irpq left unarmed\n");
			irp->IoStatus.Status = STATUS_SUCCESS;
			irp->IoStatus.Information = 0;
			} else {
				NODOKAD_TRACE("filterReadCompletion: drained %lu byte(s) from queue\n",
					(ULONG)irp->IoStatus.Information);
			}
		KeReleaseSpinLock(&filterDevExt->lock, currentIrql);
		}
	else
		{
		KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
		}
	}

	if (status == STATUS_SUCCESS)
		irp->IoStatus.Status = STATUS_SUCCESS;
	// Guard: ensure STATUS_PENDING never appears in IoStatus at completion routine exit
	// (Driver Verifier 0xC9/0x224: kbdclass synchronous completion on Win10 x86).
	if (irp->IoStatus.Status == (NTSTATUS)STATUS_PENDING)
		irp->IoStatus.Status = STATUS_CANCELLED;
	return irp->IoStatus.Status;
	}

NTSTATUS readq(KeyQue *readQue, PIRP irp)
	{
	DEBUG_LOG(("nodokad: readq()"));

	if (!KqIsEmpty(readQue)) {
		PIO_STACK_LOCATION irpSp;
		ULONG len;

		irpSp = IoGetCurrentIrpStackLocation(irp);
		len = KqDeque(readQue,
			(KEYBOARD_INPUT_DATA *)irp->AssociatedIrp.SystemBuffer,
			irpSp->Parameters.Read.Length / sizeof(KEYBOARD_INPUT_DATA));
#if DBG
		{
		PKEYBOARD_INPUT_DATA kd = (PKEYBOARD_INPUT_DATA)irp->AssociatedIrp.SystemBuffer;
		for (ULONG _di = 0; _di < len; _di++)
			NODOKAD_TRACE("readq dequeue[%lu] sc=0x%X flags=0x%X ExtraInfo=0x%08X (->user)\n",
			              _di, kd[_di].MakeCode, kd[_di].Flags, kd[_di].ExtraInformation);
		}
#endif
		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = len * sizeof(KEYBOARD_INPUT_DATA);
		irpSp->Parameters.Read.Length = (ULONG)irp->IoStatus.Information;
		return STATUS_SUCCESS;
		} else {
			irp->IoStatus.Status = STATUS_PENDING;
			irp->IoStatus.Information = 0;
			return STATUS_PENDING;
		}
	}

///////////////////////////////////////////////////////////////////////////////
// Dispatch Functions


// Generic Dispatcher
_Dispatch_type_(IRP_MJ_CREATE)
_Dispatch_type_(IRP_MJ_CREATE_NAMED_PIPE)
_Dispatch_type_(IRP_MJ_PNP)
NTSTATUS nodokaGenericDispatch(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);

	DEBUG_LOG(("nodokad: nodokaGenericDispatch()"));

	if (deviceObject->NextDevice) {
		FilterDeviceExtension *filterDevExt =
			(FilterDeviceExtension *)deviceObject->DeviceExtension;

#ifdef USE_TOUCHPAD
		if (filterDevExt->isKeyboard == FALSE)
			{
			DEBUG_LOG(("nodokad: MajorFunction: %x", irpSp->MajorFunction));
			}
#endif
		return filterDevExt->MajorFunction[irpSp->MajorFunction](deviceObject, irp);
		} else {
			DetourDeviceExtension *detourDevExt = 
				(DetourDeviceExtension *)deviceObject->DeviceExtension;

			return detourDevExt->MajorFunction[irpSp->MajorFunction](deviceObject, irp);
		}
	}


// detour IRP_MJ_CREATE
NTSTATUS detourCreate(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	DetourDeviceExtension *detourDevExt =
		(DetourDeviceExtension*)deviceObject->DeviceExtension;

	DEBUG_LOG(("nodokad: detourCreate()"));
	NODOKAD_TRACE("detourCreate: isOpen=%ld before increment\n", detourDevExt->isOpen);

	// Fix 5: allow true reference counting for FUS (Fast User Switching) scenarios
	// where two sessions may have the device open simultaneously.
	// Previously "Fix B" reset isOpen to 1 unconditionally, which broke the
	// reference count when user A opened while user B still had it open:
	// A open → isOpen=2→1 (Fix B); B close → isOpen=0 (wrong, A still open).
	InterlockedIncrement(&detourDevExt->isOpen);

	{
	PIRP irpCancel;
	KIRQL currentIrql;
	PDEVICE_OBJECT filterDevObj;

	// Fix 6: record the opener's session ID for stale-notification detection.
	// IoGetContainerInformation requires IRQL <= APC_LEVEL — call before spinlock.
	IO_SESSION_STATE_INFORMATION sessionInfo = {};
	NTSTATUS sessionStatus = IoGetContainerInformation(
		IoSessionStateInformation, NULL, &sessionInfo, sizeof(sessionInfo));
	ULONG openSessionId = NT_SUCCESS(sessionStatus) ? sessionInfo.SessionId : (ULONG)~0;

	NODOKAD_TRACE("detourCreate: sessId=%lu wasClean=%d isOpen=%ld -> opening\n",
		openSessionId, detourDevExt->wasCleanupInitiated, detourDevExt->isOpen);
	KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
	detourDevExt->wasCleanupInitiated = FALSE;
	detourDevExt->irpOwnerFileObject = NULL;
	detourDevExt->openProcessId = PsGetCurrentProcessId(); // Fix F: track opening process
	detourDevExt->openSessionId = openSessionId;           // Fix 6: track opening session
	detourDevExt->heartbeatTick = KeQueryInterruptTime();  // Fix I: grace period on fresh open
	KqClear(&detourDevExt->readQue);
	filterDevObj = detourDevExt->filterDevObj;
	KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
	if (filterDevObj) {
		FilterDeviceExtension *filterDevExt =
			(FilterDeviceExtension*)filterDevObj->DeviceExtension;
		PDEVICE_OBJECT kbdClassDevObj;

		// Fix C: cancel the IRP the filter actually owns (irpq), not kbdClassDevObj->CurrentIrp
		KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);
		irpCancel = filterDevExt->irpq;
		filterDevExt->irpq = NULL;
		if (irpCancel) {
			filterDevExt->selfCancelledIrp = irpCancel; // mark as our own cancel
			filterDevExt->selfCancelTick = KeQueryInterruptTime(); // J1b
			}
		kbdClassDevObj = filterDevExt->kbdClassDevObj;
		KeReleaseSpinLock(&filterDevExt->lock, currentIrql);
		if (irpCancel) {
			CancelKeyboardClassRead(irpCancel, kbdClassDevObj);
			}
		}

	irp->IoStatus.Status = STATUS_SUCCESS;
	}
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return irp->IoStatus.Status;
	}


// detour IRP_MJ_CLOSE
NTSTATUS detourClose(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	DetourDeviceExtension *detourDevExt = (DetourDeviceExtension*)deviceObject->DeviceExtension;
	KIRQL currentIrql;

	DEBUG_LOG(("nodokad: detourClose()"));

	KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
	InterlockedDecrement(&detourDevExt->isOpen);
	NODOKAD_TRACE("detourClose: isOpen=%ld after decrement wasCleanupInitiated=%d\n",
		detourDevExt->isOpen, detourDevExt->wasCleanupInitiated);
	KeReleaseSpinLock(&detourDevExt->lock, currentIrql);

	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	DEBUG_LOG_TERM(());
	return STATUS_SUCCESS;
	}


// detour IRP_MJ_READ
NTSTATUS detourRead(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	NTSTATUS status;
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
	DetourDeviceExtension *detourDevExt =
		(DetourDeviceExtension*)deviceObject->DeviceExtension;
	KIRQL currentIrql, cancelIrql;

	DEBUG_LOG(("nodokad: detourRead()"));

	KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
	// Fix I: every entry proves the app's read loop is alive (whether this call
	// is served immediately from readQue or goes pending waiting for a key).
	detourDevExt->heartbeatTick = KeQueryInterruptTime();
	if (irpSp->Parameters.Read.Length == 0)
		status = STATUS_SUCCESS;
	else if (irpSp->Parameters.Read.Length % sizeof(KEYBOARD_INPUT_DATA))
		status = STATUS_BUFFER_TOO_SMALL;
	else
		status = readq(&detourDevExt->readQue, irp);

	if (status == STATUS_PENDING) {
		// Follow safe cancel pattern:
		// Acquire the cancel spin lock, check if IRP was already cancelled,
		// then set cancel routine and store the pointer while holding the cancel lock.
		IoAcquireCancelSpinLock(&cancelIrql);
		if (irp->Cancel) {
			// IRP already canceled — complete outside of device lock.
			IoReleaseCancelSpinLock(cancelIrql);
			status = STATUS_CANCELLED;
		} else {
			IoMarkIrpPending(irp);
			IoSetCancelRoutine(irp, nodokaDetourReadCancel);
			detourDevExt->irpq = irp;
			detourDevExt->irpOwnerFileObject = irpSp->FileObject;
			IoReleaseCancelSpinLock(cancelIrql);
			// Keep STATUS_PENDING to indicate queued.
		}
	}

	// Release spinlock before IoCompleteRequest to avoid holding a lock
	// while the I/O manager runs completion processing (deadlock risk on UP).
	KeReleaseSpinLock(&detourDevExt->lock, currentIrql);

	if (status != STATUS_PENDING) {
		IoCompleteRequest(irp, IO_KEYBOARD_INCREMENT);
	}
	return status;
	}


// detour IRP_MJ_WRITE
NTSTATUS detourWrite(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	NTSTATUS status;
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
	ULONG len = irpSp->Parameters.Write.Length;
	DetourDeviceExtension *detourDevExt =
		(DetourDeviceExtension*)deviceObject->DeviceExtension;

	DEBUG_LOG(("nodokad: detourWrite()"));

	irp->IoStatus.Information = 0;
	if (len == 0)
		status = STATUS_SUCCESS;
	else if (len % sizeof(KEYBOARD_INPUT_DATA))
		status = STATUS_INVALID_PARAMETER;
	else {
		// write to filter que
		KIRQL currentIrql;
		PIRP irpCancel;
		PDEVICE_OBJECT filterDevObj;

		KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
		filterDevObj = detourDevExt->filterDevObj;
		KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
		// enque filter que
		if (filterDevObj) {
			FilterDeviceExtension *filterDevExt =
				(FilterDeviceExtension*)filterDevObj->DeviceExtension;
			PDEVICE_OBJECT kbdClassDevObj;

			KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);

			len /= sizeof(KEYBOARD_INPUT_DATA);
			len = KqEnque(&filterDevExt->readQue,
				(KEYBOARD_INPUT_DATA *)irp->AssociatedIrp.SystemBuffer,
				len);
			irp->IoStatus.Information = len * sizeof(KEYBOARD_INPUT_DATA);
			irpSp->Parameters.Write.Length = (ULONG)irp->IoStatus.Information;

			irpCancel = filterDevExt->irpq;
			filterDevExt->irpq = NULL;
			if (irpCancel) {
				filterDevExt->selfCancelledIrp = irpCancel; // mark as our own cancel
				filterDevExt->selfCancelTick = KeQueryInterruptTime(); // J1b
				}
			kbdClassDevObj = filterDevExt->kbdClassDevObj;
			KeReleaseSpinLock(&filterDevExt->lock, currentIrql);
			if (irpCancel) {
				// J1: single attempt only; the former Fix 1 retry loop is removed.
				// IoCancelIrp returns FALSE when the cancel routine has already
				// been cleared -- the lower driver is completing this IRP naturally
				// (a real key arrived at the same moment) and that case
				// self-resolves via filterReadCompletion()'s own queue drain.
				// Retrying is not merely useless but fatal: once the IRP may have
				// completed, its memory can be recycled (lookaside) for the next
				// parked READ, and a retried IoCancelIrp then cancels that fresh,
				// unmarked READ. win32k sees the resulting STATUS_CANCELLED pass
				// through, never re-issues a READ, and the keyboard dies
				// mid-typing. The residual race of this single call is covered by
				// the J1b selfCancelTick window in filterReadCompletion.
				if (!CancelKeyboardClassRead(irpCancel, kbdClassDevObj)) {
					NODOKAD_TRACE("detourWrite: cancel declined, irp completing naturally (self-resolves)\n");
					}
				}
			status = STATUS_SUCCESS;
			} else {
				irp->IoStatus.Information = 0;
				irpSp->Parameters.Write.Length = (ULONG)irp->IoStatus.Information;
				status = STATUS_CANCELLED;
			}
		}
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return status;
	}


// detour IRP_MJ_CLEANUP
NTSTATUS detourCleanup(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	KIRQL currentIrql, cancelIrql;
	PIRP  irpToCancel = NULL;
	DetourDeviceExtension *detourDevExt =
		(DetourDeviceExtension*)deviceObject->DeviceExtension;
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
	PFILE_OBJECT cleanupFileObj = irpSp->FileObject;

	UNREFERENCED_PARAMETER(deviceObject);
	DEBUG_LOG(("nodokad: detourCleanup()"));
	NODOKAD_TRACE("detourCleanup: isOpen=%ld wasClean=%d irpq=%p irpOwner=%p cleanupFO=%p\n",
		detourDevExt->isOpen, detourDevExt->wasCleanupInitiated,
		detourDevExt->irpq, detourDevExt->irpOwnerFileObject, cleanupFileObj);

	KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
	IoAcquireCancelSpinLock(&cancelIrql);

	// Fix 4+5: FUS support — only update shared state when this is the last (or only) opener.
	// When two sessions both have the device open (FUS: user A resumed, user B still closing),
	// isOpen > 1 at B's CLEANUP time. Setting wasCleanupInitiated or clearing openProcessId
	// here would suppress key forwarding for A, causing a freeze.
	if (detourDevExt->isOpen <= 1) {
		detourDevExt->wasCleanupInitiated = TRUE;
		detourDevExt->openProcessId = NULL; // Fix F: clear tracked process
		NODOKAD_TRACE("detourCleanup: -> wasCleanupInitiated=TRUE (isOpen<=1)\n");
	} else {
		NODOKAD_TRACE("detourCleanup: -> skipped (isOpen=%ld)\n", detourDevExt->isOpen);
	}

	// detourRead stores the pending IRP in irpq (via IoMarkIrpPending),
	// not in deviceObject->CurrentIrp. Cancel it here so that the FILE_OBJECT
	// reference is released and IRP_MJ_CLOSE can be dispatched.
	// Fix 4: only cancel irpq if it was issued by this file object.
	// If another opener's ReadFile is stored in irpq, do not touch it.
	if (detourDevExt->irpq != NULL &&
		detourDevExt->irpOwnerFileObject == cleanupFileObj) {
		irpToCancel = detourDevExt->irpq;
		detourDevExt->irpq = NULL;
		detourDevExt->irpOwnerFileObject = NULL;
		NODOKAD_TRACE("detourCleanup: -> irpq cancelled (owner match)\n");
		if (IoSetCancelRoutine(irpToCancel, NULL) == NULL) {
			// Cancel routine is already running and will complete the IRP.
			// Do not touch the IRP here to avoid double-completion.
			irpToCancel = NULL;
		}
	} else if (detourDevExt->irpq != NULL) {
		NODOKAD_TRACE("detourCleanup: -> irpq NOT cancelled (owner mismatch)\n");
	}

	IoReleaseCancelSpinLock(cancelIrql);
	KeReleaseSpinLock(&detourDevExt->lock, currentIrql);

	// Complete the pending read IRP with STATUS_CANCELLED (outside spin locks).
	if (irpToCancel != NULL) {
		irpToCancel->IoStatus.Status = STATUS_CANCELLED;
		irpToCancel->IoStatus.Information = 0;
		IoCompleteRequest(irpToCancel, IO_NO_INCREMENT);
	}

	// Complete the cleanup IRP itself.
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
	}


// detour IRP_MJ_DEVICE_CONTROL
NTSTATUS detourDeviceControl(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	NTSTATUS status;
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
	DetourDeviceExtension *detourDevExt =
		(DetourDeviceExtension*)deviceObject->DeviceExtension;

	DEBUG_LOG(("nodokad: detourDeviceControl()"));

	irp->IoStatus.Information = 0;
	if (irpSp->Parameters.DeviceIoControl.IoControlCode != IOCTL_NODOKA_GET_LOG) DEBUG_LOG(("nodokad: DeviceIoControl: %x", irpSp->Parameters.DeviceIoControl.IoControlCode));
	status = STATUS_INVALID_DEVICE_REQUEST;
	switch (irpSp->Parameters.DeviceIoControl.IoControlCode)
		{
		case IOCTL_NODOKA_DETOUR_CANCEL:
			{
			KIRQL currentIrql;
			PIRP irpCancel = NULL;

			KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
			if (detourDevExt->isOpen) {
				irpCancel = detourDevExt->irpq;
				// clear stored pointer under lock to avoid races with cancel routine
				detourDevExt->irpq = NULL;
			}
			KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
			
			if (irpCancel)
				IoCancelIrp(irpCancel);// at this point, the irpCancel may be completed
			status = STATUS_SUCCESS;
			break;
			}
		case IOCTL_NODOKA_GET_VERSION:
			{
			if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
				NodokaDriverVersion.Length)
				{
				status = STATUS_INVALID_PARAMETER;
				break;
				}
			RtlCopyMemory(irp->AssociatedIrp.SystemBuffer,
				NodokaDriverVersion.Buffer, NodokaDriverVersion.Length);
			irp->IoStatus.Information = NodokaDriverVersion.Length;

			status = STATUS_SUCCESS;
			break;
			}
		case IOCTL_NODOKA_GET_LOG:
			status = DEBUG_LOG_RETRIEVE((irp));
			break;
		default:
			status = STATUS_INVALID_DEVICE_REQUEST;
			break;
		}
	irp->IoStatus.Status = status;
	if (status != STATUS_PENDING)
		IoCompleteRequest(irp, IO_NO_INCREMENT);

	return status;
	}


#ifndef NODOKAD_NT4
// detour IRP_MJ_POWER
NTSTATUS detourPower(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	UNREFERENCED_PARAMETER(deviceObject);

	DEBUG_LOG(("nodokad: detourPower()"));

	// If there's a lower device, forward the power IRP; otherwise complete.
	PoStartNextPowerIrp(irp);
	if (deviceObject->NextDevice) {
		IoCopyCurrentIrpStackLocationToNext(irp);
		return PoCallDriver(deviceObject->NextDevice, irp);
	} else {
		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = 0;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;
	}
	}
#endif // !NODOKAD_NT4


// filter IRP_MJ_READ
NTSTATUS filterRead(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	NTSTATUS status;
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
	FilterDeviceExtension *filterDevExt =
		(FilterDeviceExtension*)deviceObject->DeviceExtension;
	DetourDeviceExtension *detourDevExt =
		(DetourDeviceExtension*)filterDevExt->detourDevObj->DeviceExtension;
	KIRQL currentIrql;

	DEBUG_LOG(("nodokad: filterRead()"));

	KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
	if (DetourShouldIntercept(detourDevExt))
		// read from que
		{
		ULONG len = irpSp->Parameters.Read.Length;

		KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
		irp->IoStatus.Information = 0;
		KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);
		if (len == 0)
			status = STATUS_SUCCESS;
		else if (len % sizeof(KEYBOARD_INPUT_DATA))
			status = STATUS_BUFFER_TOO_SMALL;
		else
			status = readq(&filterDevExt->readQue, irp);
		KeReleaseSpinLock(&filterDevExt->lock, currentIrql);
		if (status != STATUS_PENDING) {
			// Sync drop investigation: confirms the fast path (data already queued,
			// no cancel/round-trip needed) is what serves bursts after the first item.
			NODOKAD_TRACE("filterRead: served from queue immediately (no round trip)\n");
			irp->IoStatus.Status = status;
			IoCompleteRequest(irp, IO_NO_INCREMENT);
			return status;
			}
		}
	else
		{
		KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
		}

	// Sync drop investigation: queue was empty, so this read is being parked
	// (forwarded toward real hardware) and will become the next thing detourWrite()
	// can wake. If kbdclass is slow to re-issue reads after delivery, writes that
	// land here find nothing to wake until this trace's IRP eventually completes.
	NODOKAD_TRACE("filterRead: queue empty, parking read (irp=%p)\n", irp);
	KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);
	filterDevExt->irpq = irp;
	KeReleaseSpinLock(&filterDevExt->lock, currentIrql);

	IoCopyCurrentIrpStackLocationToNext(irp);
	IoSetCompletionRoutine(irp, filterReadCompletion, NULL, TRUE, TRUE, TRUE);
	return IoCallDriver(filterDevExt->kbdClassDevObj, irp);
	}


// pass throught irp to keyboard class driver
NTSTATUS filterPassThrough(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	FilterDeviceExtension *filterDevExt =
		(FilterDeviceExtension*)deviceObject->DeviceExtension;

	DEBUG_LOG(("nodokad: filterPassThrough()"));

	IoCopyCurrentIrpStackLocationToNext(irp);
	IoSetCompletionRoutine(irp, filterGenericCompletion,
		NULL, TRUE, TRUE, TRUE);
	return IoCallDriver(filterDevExt->kbdClassDevObj, irp);
	}


//#ifndef NODOKAD_NT4
// filter IRP_MJ_POWER
_Dispatch_type_(IRP_MJ_POWER)
NTSTATUS filterPower(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);

	DEBUG_LOG(("nodokad: filterPower()"));

	PoStartNextPowerIrp(irp);
	if (deviceObject->NextDevice) {
		// Filter device: forward power IRP down the device stack
		FilterDeviceExtension *filterDevExt =
			(FilterDeviceExtension*)deviceObject->DeviceExtension;
		// Fix D: on Fast Startup shutdown (S4 hibernate), proactively reset the
		// detour device's isOpen to 0 before the hibernate image is written.
		// This prevents stale isOpen=1 from blocking re-open after resume when
		// the user session's CLEANUP/CLOSE did not complete before hibernation.
		if (irpSp->MinorFunction == IRP_MN_SET_POWER &&
			irpSp->Parameters.Power.Type == SystemPowerState &&
			irpSp->Parameters.Power.State.SystemState == PowerSystemHibernate)
			{
			KIRQL detourIrql;
			DetourDeviceExtension *detourDevExt =
				(DetourDeviceExtension*)filterDevExt->detourDevObj->DeviceExtension;
			KeAcquireSpinLock(&detourDevExt->lock, &detourIrql);
			InterlockedExchange(&detourDevExt->isOpen, 0);
			detourDevExt->wasCleanupInitiated = TRUE;
			KeReleaseSpinLock(&detourDevExt->lock, detourIrql);
			}
		IoCopyCurrentIrpStackLocationToNext(irp);
		return PoCallDriver(filterDevExt->kbdClassDevObj, irp);
		} else {
			// Detour device: virtual, not in PnP tree, complete immediately
			irp->IoStatus.Status = STATUS_SUCCESS;
			irp->IoStatus.Information = 0;
			IoCompleteRequest(irp, IO_NO_INCREMENT);
			return STATUS_SUCCESS;
		}
	}
//#endif // !NODOKAD_NT4
NTSTATUS filterPnP(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
	FilterDeviceExtension *filterDevExt =
		(FilterDeviceExtension*)deviceObject->DeviceExtension;
	ULONG minor = irpSp->MinorFunction;

	DEBUG_LOG(("nodokad: filterPnP() minor=%d(%x)", minor, minor));

	switch (minor) {
	case IRP_MN_SURPRISE_REMOVAL:
	case IRP_MN_REMOVE_DEVICE:
		//
		// Forward remove to lower driver and perform cleanup in completion routine.
		//
		IoCopyCurrentIrpStackLocationToNext(irp);
		IoSetCompletionRoutine(irp, filterRemoveCompletion, deviceObject, TRUE, TRUE, TRUE);
		return IoCallDriver(filterDevExt->kbdClassDevObj, irp);

	default:
		IoCopyCurrentIrpStackLocationToNext(irp);
		return IoCallDriver(filterDevExt->kbdClassDevObj, irp);
	}
	}


#ifdef USE_TOUCHPAD
// 
NTSTATUS filterTouchpadCompletion(IN PDEVICE_OBJECT deviceObject,
																	IN PIRP irp, IN PVOID context)
	{
	KIRQL currentIrql;
	FilterDeviceExtension *filterDevExt =
		(FilterDeviceExtension*)deviceObject->DeviceExtension;
	UCHAR *data = irp->UserBuffer;
	UCHAR pressure;

	DEBUG_LOG(("nodokad: filterTouchpadCompletion()"));

	UNREFERENCED_PARAMETER(context);

	if (irp->PendingReturned)
		IoMarkIrpPending(irp);

	if (data)
		pressure = data[TOUCHPAD_PRESSURE_OFFSET];
	else
		pressure = 0;

	if (data)
		{
		//DEBUG_LOG(("nodokad: UserBuffer: %2x %2x %2x %2x", data[4], data[5], data[6], data[7]));
		//DEBUG_LOG(("nodokad: UserBuffer: %x %x %x %x %x %x %x %x", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]));
		}
	KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);
	if (filterDevExt->isTouched == FALSE && pressure)
		{
		KIRQL cancelIrql, detourIrql;
		PDEVICE_OBJECT detourDevObj = filterDevExt->detourDevObj;
		DetourDeviceExtension *detourDevExt =
			(DetourDeviceExtension*)detourDevObj->DeviceExtension;
		if (detourDevExt->isOpen)
			{
			KEYBOARD_INPUT_DATA PadKey = {0, TOUCHPAD_SCANCODE, 0, 0, 0};
			KeAcquireSpinLock(&detourDevExt->lock, &detourIrql);
			// if detour is opened, key datum are forwarded to detour
			KqEnque(&detourDevExt->readQue, &PadKey, 1);
			detourDevExt->filterDevObj = deviceObject;

			if (detourDevExt->irpq) {
				PIRP savedIrp = detourDevExt->irpq;
				if (readq(&detourDevExt->readQue, savedIrp) == STATUS_SUCCESS) {
					IoAcquireCancelSpinLock(&cancelIrql);
					if (IoSetCancelRoutine(savedIrp, NULL) != NULL) {
						detourDevExt->irpq = NULL;
						IoReleaseCancelSpinLock(cancelIrql);
						IoCompleteRequest(savedIrp, IO_KEYBOARD_INCREMENT);
					} else {
						// Cancel routine is running; do not touch completion here.
						IoReleaseCancelSpinLock(cancelIrql);
					}
				}
			}
			KeReleaseSpinLock(&detourDevExt->lock, detourIrql);
			}
		filterDevExt->isTouched = TRUE;
		}
	else
		{
		if (filterDevExt->isTouched == TRUE && pressure == 0)
			{
			KIRQL cancelIrql, detourIrql;
			PDEVICE_OBJECT detourDevObj = filterDevExt->detourDevObj;
			DetourDeviceExtension *detourDevExt =
				(DetourDeviceExtension*)detourDevObj->DeviceExtension;
			if (detourDevExt->isOpen)
				{
				KEYBOARD_INPUT_DATA PadKey = {0, TOUCHPAD_SCANCODE, 1, 0, 0};
				KeAcquireSpinLock(&detourDevExt->lock, &detourIrql);
				// if detour is opened, key datum are forwarded to detour
				KqEnque(&detourDevExt->readQue, &PadKey, 1);
				detourDevExt->filterDevObj = deviceObject;

				if (detourDevExt->irpq) {
					PIRP savedIrp = detourDevExt->irpq;
					if (readq(&detourDevExt->readQue, savedIrp) == STATUS_SUCCESS) {
						IoAcquireCancelSpinLock(&cancelIrql);
						if (IoSetCancelRoutine(savedIrp, NULL) != NULL) {
							detourDevExt->irpq = NULL;
							IoReleaseCancelSpinLock(cancelIrql);
							IoCompleteRequest(savedIrp, IO_KEYBOARD_INCREMENT);
						} else {
							IoReleaseCancelSpinLock(cancelIrql);
						}
					}
				}
				KeReleaseSpinLock(&detourDevExt->lock, detourIrql);
				}
			filterDevExt->isTouched = FALSE;
			}
		}
	//DEBUG_LOG(("nodokad: touchpad pressed: out=%u in=%u code=%u status=%x SystemBuffer=%x UserBuffer=%x", irpSp->Parameters.DeviceIoControl.OutputBufferLength, irpSp->Parameters.DeviceIoControl.InputBufferLength, irpSp->Parameters.DeviceIoControl.IoControlCode, irp->IoStatus.Status, irp->AssociatedIrp.SystemBuffer, irp->UserBuffer));
	KeReleaseSpinLock(&filterDevExt->lock, currentIrql);
	return STATUS_SUCCESS;
	}


// filter touchpad input
NTSTATUS filterTouchpad(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	FilterDeviceExtension *filterDevExt =
		(FilterDeviceExtension*)deviceObject->DeviceExtension;

	DEBUG_LOG(("nodokad: filterTouchpad()"));

	IoCopyCurrentIrpStackLocationToNext(irp);
	IoSetCompletionRoutine(irp, filterTouchpadCompletion,
		NULL, TRUE, TRUE, TRUE);
	return IoCallDriver(filterDevExt->kbdClassDevObj, irp);
	}
#endif


NTSTATUS detourPnP(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	DEBUG_LOG(("nodokad: detourPnP()"));

	// Forward PnP IRPs to lower driver if present; otherwise complete.
	if (deviceObject->NextDevice) {
		IoCopyCurrentIrpStackLocationToNext(irp);
		return IoCallDriver(deviceObject->NextDevice, irp);
	} else {
		// No lower device to forward to — complete the PnP IRP here.
		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = 0;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;
	}
	}

#pragma warning(pop)
