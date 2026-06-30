//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// nodokaipc.h - nodoka inter process communication
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#ifndef _NODOKAIPC_H
#define _NODOKAIPC_H

#include <windows.h>

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

	///
#define WM_NodokaIPC_NAME _T("NodokaIPC{46269F4D-D560-40f9-B38B-DB5E280FEF47}")

	enum NodokaIPCCommand
	{
		// enable or disable Nodoka
		NodokaIPCCommand_Enable = 1,
	};

	BOOL NodokaIPC_PostMessage(NodokaIPCCommand i_wParam, LPARAM i_lParam);
	BOOL NodokaIPC_Enable(BOOL i_isEnabled);

#ifdef _NODOKAIPC_H_DEFINE_FUNCTIONS

	BOOL NodokaIPC_PostMessage(NodokaIPCCommand i_command, LPARAM i_lParam)
	{
		static UINT WM_NodokaIPC;
		HWND hwnd;

		if (WM_NodokaIPC == 0)
		{
			WM_NodokaIPC = RegisterWindowMessage(WM_NodokaIPC_NAME);
			if (WM_NodokaIPC == 0)
			{
				return FALSE;
			}
		}

		hwnd = FindWindow(_T("nodokaTasktray"), NULL);
		if (hwnd == NULL)
		{
			return FALSE;
		}
		PostMessage(hwnd, WM_NodokaIPC, i_command, i_lParam);
		return TRUE;
	}

	BOOL NodokaIPC_Enable(BOOL i_isEnabled)
	{
		return NodokaIPC_PostMessage(NodokaIPCCommand_Enable, i_isEnabled);
	}

#endif // _NODOKAIPC_H_DEFINE_FUNCTIONS

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // !_NODOKAIPC_H
