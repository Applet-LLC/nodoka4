//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// driver.h

#ifndef _DRIVER_H
#define _DRIVER_H

#include <winioctl.h>

/// nodoka device file name
#define NODOKA_DEVICE_FILE_NAME _T("\\\\.\\NodokaWalk1")
///
#define NODOKA_DRIVER_NAME _T("nodokad")

/// Ioctl value
#include "..\d\ioctl.h"

/// nodokad2 (kbfiltr 型 KMDF, inverted call) プロトコル定義。
/// アプリはこれで新旧どちらのドライバとも会話できる (feature flag で選択)。
#include "..\d2\public2.h"

/// nodokad2 コントロールデバイスの Win32 名。
#define NODOKA2_DEVICE_FILE_NAME _T("\\\\.\\Nodoka2Ctl")
///
#define NODOKA2_DRIVER_NAME _T("nodokad2")

/// GET_EVENTS 1 回で受け取る最大イベント数 (バッチ)。
#define NODOKA2_BATCH_MAX 32

/// derived from w2kddk/inc/ntddkbd.h
class KEYBOARD_INPUT_DATA
{
public:
	///
	enum
	{
		/// key release flag
		BREAK = 1,
		/// extended key flag
		E0 = 2,
		/// extended key flag
		E1 = 4,
		/// extended key flag (E0 | E1)
		E0E1 = 6,
		///
		TERMSRV_SET_LED = 8,
		/// Define the keyboard overrun MakeCode.
		KEYBOARD_OVERRUN_MAKE_CODE_ = 0xFF,
	};

public:
	/** Unit number.  E.g., for \Device\KeyboardPort0 the unit is '0', for
		\Device\KeyboardPort1 the unit is '1', and so on. */
	USHORT UnitId;

	/** The "make" scan code (key depression). */
	USHORT MakeCode;

	/** The flags field indicates a "break" (key release) and other miscellaneous
		scan code information defined above. */
	USHORT Flags;

	///
	USHORT Reserved;

	/** Device-specific additional information for the event. */
	ULONG ExtraInformation;
};

#endif // !_DRIVER_H
