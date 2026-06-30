//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// dlginvestigate.cpp
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#include "misc.h"
#include "engine.h"
#include "focus.h"
#include "hook.h"
#include "nodokarc.h"
#include "stringtool.h"
#include "target.h"
#include "windowstool.h"
#include "vkeytable.h"
#include "dlginvestigate.h"
#include <iomanip>

// QueryFullProcessImageName は Vista+ のAPI。
// _WIN32_WINNT が 0x0600 未満のビルド設定でも使えるよう直接宣言する。
#ifndef QueryFullProcessImageName
EXTERN_C BOOL WINAPI QueryFullProcessImageNameW(HANDLE, DWORD, LPWSTR, PDWORD);
#  ifdef UNICODE
#    define QueryFullProcessImageName QueryFullProcessImageNameW
#  else
EXTERN_C BOOL WINAPI QueryFullProcessImageNameA(HANDLE, DWORD, LPSTR, PDWORD);
#    define QueryFullProcessImageName QueryFullProcessImageNameA
#  endif
#endif

///
class DlgInvestigate
{
	HWND m_hwnd;			   ///
	UINT m_WM_NODOKA_MESSAGE;  ///
	DlgInvestigateData m_data; ///

public:
	///
	DlgInvestigate(HWND i_hwnd)
		: m_hwnd(i_hwnd),
		  m_WM_NODOKA_MESSAGE(RegisterWindowMessage(
			  addSessionId(WM_NODOKA_MESSAGE_NAME).c_str()))
	{
		m_data.m_engine = NULL;
		m_data.m_hwndLog = NULL;
	}

	/// WM_INITDIALOG
	BOOL wmInitDialog(HWND /* i_focus */, LPARAM i_lParam)
	{
		m_data = *reinterpret_cast<DlgInvestigateData *>(i_lParam);
		setSmallIcon(m_hwnd, IDI_ICON_nodoka);
		setBigIcon(m_hwnd, IDI_ICON_nodoka);
		return TRUE;
	}

	/// WM_DESTROY
	BOOL wmDestroy()
	{
		unsetSmallIcon(m_hwnd);
		unsetBigIcon(m_hwnd);
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
		}
		return FALSE;
	}

	/// WM_focusNotify
	BOOL wmFocusNotify(bool i_isFocused, HWND i_hwndFocus)
	{
		if (m_data.m_engine &&
			i_hwndFocus == GetDlgItem(m_hwnd, IDC_CUSTOM_scancode))
			m_data.m_engine->enableLogMode(i_isFocused);
		return TRUE;
	}

	/// WM_targetNotify
	BOOL wmTargetNotify(HWND i_hwndTarget)
	{
#ifndef NODOKA_NO_UIACCESS
		// UIAccess=true 環境では直接APIで取得可能。
		// hook.cpp の getClassNameTitleName と同等の処理:
		// ターゲットウィンドウから親チェーンを遡り class/title を階層結合し、
		// 先頭に EXE パスを付加する (exepath:parentClass:targetClass 形式)。

		_TCHAR cls[GANA_MAX_ATOM_LENGTH];
		if (!GetClassName(i_hwndTarget, cls, NUMBER_OF(cls)))
		{
			// GetClassName が失敗した場合のみDLL経由フォールバック
			CHECK_TRUE(PostMessage(i_hwndTarget, m_WM_NODOKA_MESSAGE,
								   NodokaMessage_notifyName, 0));
			return TRUE;
		}

		// 親チェーンを遡って class/title を階層結合
		tstring classStr, titleStr;
		bool isFirst = true;
		HWND hwnd = i_hwndTarget;
		while (hwnd)
		{
			// タイトルは256文字に制限する。
			// EditコントロールへのGetWindowTextは内容全体を返すため
			// バッファサイズで上限を設ける。
			_TCHAR wcls[GANA_MAX_ATOM_LENGTH];
			_TCHAR wtitle[256];
			GetClassName(hwnd, wcls, NUMBER_OF(wcls));
			if (GetWindowText(hwnd, wtitle, NUMBER_OF(wtitle)) == 0)
				wtitle[0] = _T('\0');
			if (isFirst)
			{
				classStr = wcls;
				titleStr = wtitle;
				isFirst = false;
			}
			else
			{
				classStr = tstring(wcls) + _T(":") + classStr;
				titleStr = tstring(wtitle) + _T(":") + titleStr;
			}
			hwnd = GetParent(hwnd);
		}

		// EXE パスを先頭に付加 (getClassNameTitleName の GetModuleFileName 相当)
		DWORD pid = 0;
		DWORD threadId = GetWindowThreadProcessId(i_hwndTarget, &pid);
		_TCHAR exePath[GANA_MAX_PATH] = _T("");
		if (pid)
		{
			HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
			if (hProc)
			{
				DWORD len = NUMBER_OF(exePath);
				QueryFullProcessImageName(hProc, 0, exePath, &len);
				CloseHandle(hProc);
			}
		}
		classStr = tstring(exePath) + _T(":") + classStr;
		titleStr = tstring(exePath) + _T(":") + titleStr;

		{
			Acquire a(&m_data.m_engine->m_log, 1);
			m_data.m_engine->m_log << _T("HWND:\t") << std::hex
								   << reinterpret_cast<DWORD_PTR>(i_hwndTarget)
								   << std::dec << std::endl;
			m_data.m_engine->m_log << _T("THREADID:") << static_cast<int>(threadId)
								   << std::endl;
		}
		{
			Acquire a(&m_data.m_engine->m_log, 0);
			m_data.m_engine->m_log << _T("CLASS:\t") << classStr << std::endl;
			m_data.m_engine->m_log << _T("TITLE:\t") << titleStr << std::endl;

			// トップレベルウィンドウの位置・サイズ (notifyHandler と同形式)
			HWND hwndTop = GetAncestor(i_hwndTarget, GA_ROOT);
			if (!hwndTop)
				hwndTop = i_hwndTarget;
			RECT rc;
			GetWindowRect(hwndTop, &rc);
			m_data.m_engine->m_log << _T("Toplevel Window Position/Size: (")
								   << rc.left << _T(", ") << rc.top << _T(") / (")
								   << rcWidth(&rc) << _T("x") << rcHeight(&rc)
								   << _T(")") << std::endl;
			SystemParametersInfo(SPI_GETWORKAREA, 0, (void *)&rc, FALSE);
			m_data.m_engine->m_log << _T("Desktop Window Position/Size: (")
								   << rc.left << _T(", ") << rc.top << _T(") / (")
								   << rcWidth(&rc) << _T("x") << rcHeight(&rc)
								   << _T(")") << std::endl;
			m_data.m_engine->m_log << std::endl;
		}
		return TRUE;
#else
		// NODOKA_NO_UIACCESS（nodoka64_nua.exe）: High IL 等では EXE からの
		// GetWindowText が UIPI で空になり得るため、ターゲットスレッド上の
		// フック DLL 経由で notifyHandler へ送る（DLL 未注入プロセスでは無反応）。
		// High IL ウィンドウへの PostMessage は UIPI でブロックされて FALSE を返すが、
		// その場合は結果なし（無反応）が期待動作なので CHECK_TRUE ではなく単純呼び出し。
		PostMessage(i_hwndTarget, m_WM_NODOKA_MESSAGE, NodokaMessage_notifyName, 0);
		return TRUE;
#endif
	}

	/// WM_vkeyNotify
	BOOL wmVkeyNotify(int i_nVirtKey, int /* i_repeatCount */,
					  BYTE /* i_scanCode */, bool i_isExtended,
					  bool /* i_isAltDown */, bool i_isKeyup)
	{
		Acquire a(&m_data.m_engine->m_log, 0);
		m_data.m_engine->m_log
			<< (i_isExtended ? _T(" E-") : _T("   "))
			<< _T("0x") << std::hex << std::setw(2) << std::setfill(_T('0'))
			<< i_nVirtKey << std::dec << _T("  &VK( ")
			<< (i_isExtended ? _T("E-") : _T("  "))
			<< (i_isKeyup ? _T("U-") : _T("D-"));

		for (const VKeyTable *vkt = g_vkeyTable; vkt->m_name; ++vkt)
		{
			if (vkt->m_code == i_nVirtKey)
			{
				m_data.m_engine->m_log << vkt->m_name << _T(" )") << std::endl;
				return TRUE;
			}
		}
		m_data.m_engine->m_log << _T("0x") << std::hex << std::setw(2)
							   << std::setfill(_T('0')) << i_nVirtKey << std::dec
							   << _T(" )") << std::endl;
		return TRUE;
	}

	BOOL wmMove(int /* i_x */, int /* i_y */)
	{
		RECT rc1, rc2;
		GetWindowRect(m_hwnd, &rc1);
		GetWindowRect(m_data.m_hwndLog, &rc2);

		MoveWindow(m_data.m_hwndLog, rc1.left, rc1.bottom,
				   rcWidth(&rc2), rcHeight(&rc2), TRUE);

		return TRUE;
	}
};

//
INT_PTR CALLBACK dlgInvestigate_dlgProc(HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam)
{
	DlgInvestigate *wc;
	getUserData(i_hwnd, &wc);
	if (!wc)
		switch (i_message)
		{
		case WM_INITDIALOG:
			wc = setUserData(i_hwnd, new DlgInvestigate(i_hwnd));
			return wc->wmInitDialog(reinterpret_cast<HWND>(i_wParam), i_lParam);
		}
	else
		switch (i_message)
		{
		case WM_MOVE:
			return wc->wmMove(static_cast<short>(LOWORD(i_lParam)),
							  static_cast<short>(HIWORD(i_lParam)));
		case WM_COMMAND:
			return wc->wmCommand(HIWORD(i_wParam), LOWORD(i_wParam),
								 reinterpret_cast<HWND>(i_lParam));
		case WM_CLOSE:
			return wc->wmClose();
		case WM_DESTROY:
			return wc->wmDestroy();
		case WM_APP_notifyFocus:
			return wc->wmFocusNotify(!!i_wParam,
									 reinterpret_cast<HWND>(i_lParam));
		case WM_APP_targetNotify:
			return wc->wmTargetNotify(reinterpret_cast<HWND>(i_lParam));
		case WM_APP_notifyVKey:
			return wc->wmVkeyNotify(
				static_cast<int>(i_wParam), static_cast<int>(i_lParam & 0xffff),
				static_cast<BYTE>((i_lParam >> 16) & 0xff),
				!!(i_lParam & (1 << 24)),
				!!(i_lParam & (1 << 29)),
				!!(i_lParam & (1 << 31)));
		case WM_NCDESTROY:
			delete wc;
			return TRUE;
		}
	return FALSE;
}
