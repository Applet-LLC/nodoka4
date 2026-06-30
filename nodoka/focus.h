//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// focus.h
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#ifndef _FOCUS_H
#define _FOCUS_H

#include <windows.h>

///
extern ATOM Register_focus();

enum
{
	WM_APP_notifyFocus = WM_APP + 103,
	WM_APP_notifyVKey = WM_APP + 104,
};

#endif // !_FOCUS_H
