//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// dlgversion.h
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#ifndef _DLGVERSION_H
#define _DLGVERSION_H

#include <windows.h>
#include "misc.h"

INT_PTR CALLBACK dlgVersion_dlgProc(HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam);

/// Rebuild and update the version dialog's text (call before ShowWindow after
/// kbdaddid license state may have changed since the dialog was created).
void refreshVersionDialog(HWND hwndVersion, const tstring &driverVersionStr);

#endif // !_DLGVERSION_H
