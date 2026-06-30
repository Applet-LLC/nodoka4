//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// target.h
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#ifndef _TARGET_H
#define _TARGET_H

#include <windows.h>

///
extern ATOM Register_target();

///
enum
{
	///
	WM_APP_targetNotify = WM_APP + 105,
};

#endif // !_TARGET_H
