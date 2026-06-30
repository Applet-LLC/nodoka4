//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// dlgversion.cpp
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#include "misc.h"

#include "nodoka.h"
#include "nodokarc.h"
#include "windowstool.h"
#include "compiler_specific_func.h"
#include "layoutmanager.h"
#include <sstream>

#include <cstdio>
#include <windowsx.h>

///
class DlgVersion : public LayoutManager
{
	HWND m_hwnd; ///

public:
	///
	DlgVersion(HWND i_hwnd)
		: LayoutManager(i_hwnd),
		  m_hwnd(i_hwnd)
	{
	}

	/// Build the version text and set it on IDC_EDIT_builtBy.
	static void setVersionText(HWND hwnd, const TCHAR *nodokadVersion)
	{
		_TCHAR modulebuf[1024];
		CHECK_TRUE(GetModuleFileName(g_hInst, modulebuf, NUMBER_OF(modulebuf)));

		_TCHAR buf[1024];
		_sntprintf_s(buf, NUMBER_OF(buf), _TRUNCATE, loadString(IDS_version).c_str(),
					 _T(VERSION)
#ifdef _WIN64
					 _T(" for x64 ")
#else
					 _T(" for x86 ")
#endif
					 ,
					 nodokadVersion,
					 loadString(IDS_homepage).c_str(),
					 (_T(LOGNAME) _T("@") + toLower(_T(COMPUTERNAME))).c_str(),
					 _T(__DATE__) _T(" ") _T(__TIME__),
					 getCompilerVersionString().c_str(),
					 modulebuf);
		tstring versionText(buf);
		if (!g_hookDllStatus.empty())
		{
			versionText += _T("\r\n\r\n");
			versionText += g_hookDllStatus;
		}
		Edit_SetText(GetDlgItem(hwnd, IDC_EDIT_builtBy), versionText.c_str());
	}

	/// WM_INITDIALOG
	BOOL wmInitDialog(HWND /* i_focus */, LPARAM i_lParam)
	{
		TCHAR *nodokadVersion = (TCHAR *)i_lParam;
		setSmallIcon(m_hwnd, IDI_ICON_nodoka);
		setBigIcon(m_hwnd, IDI_ICON_nodoka);

		setVersionText(m_hwnd, nodokadVersion);

		// set layout manager
		typedef LayoutManager LM;

		addItem(GetDlgItem(m_hwnd, IDC_STATIC_nodokaIcon),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_TOP_EDGE,
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_TOP_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_EDIT_builtBy),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_TOP_EDGE,
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_download),
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDOK),
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE);
		restrictSmallestSize();

		return TRUE;
	}

	/// WM_CLOSE
	BOOL wmClose()
	{
		CHECK_TRUE(EndDialog(m_hwnd, 0));
		return TRUE;
	}

	/// WM_COMMAND
	BOOL wmCommand(int /* i_notifyCode */, int i_id, HWND /* i_hwndControl */)
	{
		switch (i_id)
		{
		case IDOK:
		{
			CHECK_TRUE(EndDialog(m_hwnd, 0));
			return TRUE;
		}
		case IDC_BUTTON_download:
		{
			ShellExecute(NULL, NULL, loadString(IDS_homepage).c_str(),
						 NULL, NULL, SW_SHOWNORMAL);
			CHECK_TRUE(EndDialog(m_hwnd, 0));
			return TRUE;
		}
		}
		return FALSE;
	}
};

//
INT_PTR CALLBACK dlgVersion_dlgProc(HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam)
{
	DlgVersion *wc;
	getUserData(i_hwnd, &wc);
	if (!wc)
		switch (i_message)
		{
		case WM_INITDIALOG:
			wc = setUserData(i_hwnd, new DlgVersion(i_hwnd));
			return wc->wmInitDialog(reinterpret_cast<HWND>(i_wParam), i_lParam);
		}
	else
		switch (i_message)
		{
		case WM_COMMAND:
			return wc->wmCommand(HIWORD(i_wParam), LOWORD(i_wParam), reinterpret_cast<HWND>(i_lParam));
		case WM_CLOSE:
			return wc->wmClose();
		case WM_NCDESTROY:
			delete wc;
			return TRUE;
		default:
			return wc->defaultWMHandler(i_message, i_wParam, i_lParam);
		}
	return FALSE;
}

// Update the version dialog's text to reflect the current driver version string.
// Call this before ShowWindow when the dialog may have been created before the
// kbdaddid license was activated.
void refreshVersionDialog(HWND hwndVersion, const tstring &driverVersionStr)
{
	if (!hwndVersion || !IsWindow(hwndVersion)) return;
	DlgVersion::setVersionText(hwndVersion, driverVersionStr.c_str());
}
