//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// dlglog.h
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#ifndef _DLGLOG_H
#define _DLGLOG_H

#include <windows.h>
#include "msgstream.h"

//
INT_PTR CALLBACK dlgLog_dlgProc(HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam);

enum
{
	///
	WM_APP_dlglogNotify = WM_APP + 115,
};

enum DlgLogNotify
{
	DlgLogNotify_logCleared, ///
	DlgLogNotify_reload,	 /// reload button
};

/// parameters for "Investigate" dialog box
class DlgLogData
{
public:
	tomsgstream *m_log;  /// log stream
	HWND m_hwndTaskTray; /// tasktray window
};

#endif // !_DLGLOG_H
