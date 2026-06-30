//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// dlglog.cpp
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#include "misc.h"
#include "nodoka.h"
#include "nodokarc.h"
#include "registry.h"
#include "windowstool.h"
#include "msgstream.h"
#include "layoutmanager.h"
#include "dlglog.h"
#include <windowsx.h>

///
class DlgLog : public LayoutManager
{
	HWND m_hwndEdit;	   ///
	HWND m_hwndTaskTray;   /// tasktray window
	LOGFONT m_lf;		   ///
	HFONT m_hfontOriginal; ///
	HFONT m_hfont;		   ///
	tomsgstream *m_log;	///

public:
	///
	DlgLog(HWND i_hwnd)
		: LayoutManager(i_hwnd),
		  m_hwndEdit(GetDlgItem(m_hwnd, IDC_EDIT_log)),
		  m_hwndTaskTray(NULL),
		  m_hfontOriginal(GetWindowFont(m_hwnd)),
		  m_hfont(NULL)
	{
	}

	/// WM_INITDIALOG
	BOOL wmInitDialog(HWND /* i_focus */, LPARAM i_lParam)
	{
		DlgLogData *dld = reinterpret_cast<DlgLogData *>(i_lParam);
		m_log = dld->m_log;
		m_hwndTaskTray = dld->m_hwndTaskTray;

		// set icons
		setSmallIcon(m_hwnd, IDI_ICON_nodoka);
		setBigIcon(m_hwnd, IDI_ICON_nodoka);

		// set font
		Registry::read(NODOKA_REGISTRY_ROOT, _T("logFont"), &m_lf,
					   loadString(IDS_logFont));
		m_hfont = CreateFontIndirect(&m_lf);
		SetWindowFont(m_hwndEdit, m_hfont, false);

		// resize
		RECT rc;
		CHECK_TRUE(GetClientRect(m_hwnd, &rc));
		wmSize(0, (short)rc.right, (short)rc.bottom);

		// debug level
		bool isChecked =
			(IsDlgButtonChecked(m_hwnd, IDC_CHECK_detail) == BST_CHECKED);
		m_log->setDebugLevel(isChecked ? 1 : 0);

		// set layout manager
		typedef LayoutManager LM;
		addItem(GetDlgItem(m_hwnd, IDOK),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_EDIT_log),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_TOP_EDGE,
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_clearLog),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_changeFont),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_setting),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_CHECK_detail),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_reload),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
		restrictSmallestSize();

		// enlarge window
		GetWindowRect(m_hwnd, &rc);
		rc.bottom += (rc.bottom - rc.top) * 3;

		int sx = GetSystemMetrics(SM_CXSCREEN);
		int sy = GetSystemMetrics(SM_CYSCREEN);
		int new_x = rc.right - rc.left;
		int new_y = rc.bottom - rc.top;

		MoveWindow(m_hwnd, (sx - new_x) / 2, (sy - new_y) / 2, new_x, new_y, true);

		return TRUE;
	}

	/// WM_DESTROY
	BOOL wmDestroy()
	{
		// unset font
		SetWindowFont(m_hwndEdit, m_hfontOriginal, false);
		DeleteObject(m_hfont);

		// unset icons
		unsetBigIcon(m_hwnd);
		unsetSmallIcon(m_hwnd);
		return TRUE;
	}

	/// WM_CLOSE
	BOOL wmClose()
	{
		ShowWindow(m_hwnd, SW_HIDE);
		return TRUE;
	}

	/// WM_COMMAND
	BOOL wmCommand(int /* i_notifyCode */, int i_id, HWND /* i_hwndControl */)
	{
		switch (i_id)
		{
		case IDOK:
		{
			ShowWindow(m_hwnd, SW_HIDE);
			return TRUE;
		}

		case IDC_BUTTON_clearLog:
		{
			Edit_SetSel(m_hwndEdit, 0, Edit_GetTextLength(m_hwndEdit));
			Edit_ReplaceSel(m_hwndEdit, _T(""));
			SendMessage(m_hwndTaskTray, WM_APP_dlglogNotify,
						DlgLogNotify_logCleared, 0);
			return TRUE;
		}

		case IDC_BUTTON_setting:
		{
			HWND i_hwnd = FindWindow(_T("nodokaTasktray"), NULL);
			SendMessage(i_hwnd, WM_COMMAND, MAKELONG(ID_MENUITEM_setting, 0), 0);
			return TRUE;
		}

		case IDC_BUTTON_changeFont:
		{
			CHOOSEFONT cf;
			memset(&cf, 0, sizeof(cf));
			cf.lStructSize = sizeof(cf);
			cf.hwndOwner = m_hwnd;
			cf.lpLogFont = &m_lf;
			cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
			if (ChooseFont(&cf))
			{
				HFONT hfontNew = CreateFontIndirect(&m_lf);
				SetWindowFont(m_hwndEdit, hfontNew, true);
				UpdateWindow(m_hwndEdit);
				DeleteObject(m_hfont);
				m_hfont = hfontNew;
				Registry::write(NODOKA_REGISTRY_ROOT, _T("logFont"), m_lf);
			}
			return TRUE;
		}

		case IDC_CHECK_detail:
		{
			bool isChecked =
				(IsDlgButtonChecked(m_hwnd, IDC_CHECK_detail) == BST_CHECKED);
			m_log->setDebugLevel(isChecked ? 1 : 0);
			return TRUE;
		}

		case IDC_BUTTON_reload:
		{
			SendMessage(m_hwndTaskTray, WM_APP_dlglogNotify,
						DlgLogNotify_reload, 0);
			return TRUE;
		}
		}
		return FALSE;
	}
};

//
INT_PTR CALLBACK dlgLog_dlgProc(HWND i_hwnd, UINT i_message,
								WPARAM i_wParam, LPARAM i_lParam)
{
	DlgLog *wc;
	getUserData(i_hwnd, &wc);
	if (!wc)
		switch (i_message)
		{
		case WM_INITDIALOG:
			wc = setUserData(i_hwnd, new DlgLog(i_hwnd));
			return wc->wmInitDialog(reinterpret_cast<HWND>(i_wParam), i_lParam);
		}
	else
		switch (i_message)
		{
		case WM_COMMAND:
			return wc->wmCommand(HIWORD(i_wParam), LOWORD(i_wParam),
								 reinterpret_cast<HWND>(i_lParam));
		case WM_CLOSE:
			return wc->wmClose();
		case WM_DESTROY:
			return wc->wmDestroy();
		case WM_NCDESTROY:
			delete wc;
			return TRUE;
		default:
			return wc->defaultWMHandler(i_message, i_wParam, i_lParam);
		}
	return FALSE;
}
