//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// target.h

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
