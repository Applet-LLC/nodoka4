//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// dlginvestigate.h
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#ifndef _DLGINVESTIGATE_H
#define _DLGINVESTIGATE_H

#include <windows.h>

/// dialog procedure of "Investigate" dialog box
INT_PTR CALLBACK dlgInvestigate_dlgProc(
	HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam);

class Engine;

/// parameters for "Investigate" dialog box
class DlgInvestigateData
{
public:
	Engine *m_engine; /// engine
	HWND m_hwndLog;   /// log
};

#endif // !_DLGINVESTIGATE_H
