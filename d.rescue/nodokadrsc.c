///////////////////////////////////////////////////////////////////////////////
// Rescue driver for Nodoka for Windows2000/XP/Vista
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/


#include <ntddk.h>

#pragma warning(3 : 4061 4100 4132 4701 4706)

///////////////////////////////////////////////////////////////////////////////
// Protorypes


NTSTATUS DriverEntry       (IN PDRIVER_OBJECT, IN PUNICODE_STRING);
NTSTATUS nodokaAddDevice     (IN PDRIVER_OBJECT, IN PDEVICE_OBJECT);
VOID nodokaUnloadDriver      (IN PDRIVER_OBJECT);
NTSTATUS nodokaGenericDispatch (IN PDEVICE_OBJECT, IN PIRP);

#ifdef ALLOC_PRAGMA
#pragma alloc_text( init, DriverEntry )
#endif // ALLOC_PRAGMA

///////////////////////////////////////////////////////////////////////////////
// Entry / Unload


// initialize driver
NTSTATUS DriverEntry(IN PDRIVER_OBJECT driverObject,
										 IN PUNICODE_STRING registryPath)
	{
	ULONG i;
	UNREFERENCED_PARAMETER(registryPath);

	// set major functions
	driverObject->DriverUnload = nodokaUnloadDriver;
	driverObject->DriverExtension->AddDevice = nodokaAddDevice;
	for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
		driverObject->MajorFunction[i] = nodokaGenericDispatch;
	driverObject->MajorFunction[IRP_MJ_PNP] = nodokaGenericDispatch;
	return STATUS_SUCCESS;
	}

NTSTATUS nodokaAddDevice(IN PDRIVER_OBJECT driverObject,
											 IN PDEVICE_OBJECT kbdClassDevObj)
	{
	UNREFERENCED_PARAMETER(driverObject);
	UNREFERENCED_PARAMETER(kbdClassDevObj);
	return STATUS_SUCCESS;
	}

// unload driver
VOID nodokaUnloadDriver(IN PDRIVER_OBJECT driverObject)
	{
	UNREFERENCED_PARAMETER(driverObject);
	}


///////////////////////////////////////////////////////////////////////////////
// Dispatch Functions


// Generic Dispatcher
NTSTATUS nodokaGenericDispatch(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
	{
	UNREFERENCED_PARAMETER(deviceObject);
	UNREFERENCED_PARAMETER(irp);
	return STATUS_SUCCESS;
	}
