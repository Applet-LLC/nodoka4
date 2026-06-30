//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// hook.cpp
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#define _HOOK_CPP

#include "..\nodoka\misc.h"

#include "..\nodoka\hook.h"
#include "..\nodoka\stringtool.h"
#include "..\nodoka\nodoka.h"
#include "..\nodoka\rawinput.h"

#include <locale.h>
#include <imm.h>
#include <richedit.h>
#include <VersionHelpers.h>

// Some applications use different values for below messages
// when double click of title bar.
#define SC_MAXIMIZE2 (SC_MAXIMIZE + 2)
#define SC_MINIMIZE2 (SC_MINIMIZE + 2)
#define SC_RESTORE2 (SC_RESTORE + 2)

#define LLKHF_LOWER_IL_INJECTED 0x00000002
#define LLMHF_LOWER_IL_INJECTED 0x00000002

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Global Variables

#pragma comment(linker, "/section:shared,rws")
#pragma data_seg("shared")

HHOOK m_hHookGetMessage = NULL;  ///
HHOOK m_hHookCallWndProc = NULL; ///
HHOOK m_hHookGetSync = NULL;	 ///
bool m_isHooked = false;
HHOOK m_hHookMouseProc = NULL;	///
HHOOK m_hHookKeyboardProc = NULL; ///

#pragma data_seg()

static HHOOK m_hHookLocalKeyboardProc = NULL; // per-process, not shared

struct Globals
{
	HANDLE m_hHookDataDll;		///
	HookData *hookDataDll;		///
	bool m_isMaped;				///
	HWND m_hwndFocus;			///
	HINSTANCE m_hInstDLL;		///
	bool m_isInMenu;			///
	UINT m_WM_NODOKA_MESSAGE;   ///
	bool m_isImeLock;			///
	bool m_isImeCompositioning; ///
	bool m_isKanaLock;			///
	bool m_isCandidateWindow;   ///
	INPUT_DETOUR m_keyboardDetour;
	INPUT_DETOUR m_mouseDetour;
	Engine *m_engine;
};

static Globals g;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Prototypes

static void notifyThreadDetach(void);
static void notifyShow(NotifyShow::Show i_show, bool i_isMDI);
static void notifyLog(_TCHAR *i_msg);

static bool mapHookData(void);
static void unmapHookData(void) noexcept;

void EnableChangeWindowMessageFilter(void);
void DisableChangeWindowMessageFilter(void);

#ifndef IsWindowsVistaOrGreater
// Vista以降かどうかを判定する関数を自前で定義
inline BOOL IsWindowsVistaOrGreater() noexcept
{
	OSVERSIONINFOEX osvi = { 0 };
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	osvi.dwMajorVersion = 6;
	osvi.dwMinorVersion = 0;
	DWORDLONG dwlConditionMask = 0;
	VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
	return VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION, dwlConditionMask);
}
#endif

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Functions

/// EntryPoint
BOOL WINAPI DllMain(HINSTANCE i_hInstDLL, DWORD i_fdwReason,
					LPVOID /* i_lpvReserved */)
{
	const HMODULE hModule = GetModuleHandle(TEXT("user32.dll"));

	switch (i_fdwReason)
	{
	case DLL_PROCESS_ATTACH:
	{
		g.m_hInstDLL = i_hInstDLL;
		_tsetlocale(LC_ALL, _T(""));

		if (hModule != NULL)
			g.m_WM_NODOKA_MESSAGE = RegisterWindowMessage(addSessionId(WM_NODOKA_MESSAGE_NAME).c_str());
		EnableChangeWindowMessageFilter();

		break;
	}

	case DLL_THREAD_ATTACH:
		// EnableChangeWindowMessageFilter() はここで呼ばない。
		// このDLLは WH_CALLWNDPROC/WH_GETMESSAGE で全プロセスに注入されるため、
		// 他プロセスの全スレッド生成のたびに呼ばれてしまう。
		// ChangeWindowMessageFilterEx は呼び出し元プロセスのウィンドウにしか適用できないため、
		// nodoka.exe 以外のプロセス内での呼び出しは全て無効（サイレントに無視される）。
		// DLL_PROCESS_ATTACH（nodoka.exe 内）での呼び出しのみが有効。
		break;

	case DLL_PROCESS_DETACH:
		notifyThreadDetach();
		unmapHookData();
		break;

	case DLL_THREAD_DETACH:
		notifyThreadDetach();
		break;

	default:
		break;
	}
	return TRUE;
}

// ChangeWindowMessageFilter helper
static void myChangeWindowMessageFilter(UINT message, DWORD flag) noexcept
{
    // user32.dll は全プロセスで常にロード済みのため LoadLibrary ではなく GetModuleHandle を使用
    HMODULE dll = GetModuleHandle(TEXT("user32.dll"));
    static FUNCTYPE ChangeWindowMessageFilter = reinterpret_cast<FUNCTYPE>(GetProcAddress(dll, "ChangeWindowMessageFilter"));
    static FUNCTYPE7 ChangeWindowMessageFilterEx = reinterpret_cast<FUNCTYPE7>(GetProcAddress(dll, "ChangeWindowMessageFilterEx"));

    DWORD flag7 = MSGFLT_RESET;
    HWND tmpNodokaTasktray = NULL;

    if (g.hookDataDll == NULL)
    {
        tmpNodokaTasktray = FindWindow(L"nodokaTasktray", NULL);
    }
    else
    {
        tmpNodokaTasktray = (HWND)(ULONG_PTR)(g.hookDataDll->m_hwndTaskTray);
    }

    if (flag == MSGFLT_ADD)
    {
        flag7 = MSGFLT_ALLOW;
    }
    else if (flag == MSGFLT_REMOVE)
    {
        flag7 = MSGFLT_RESET;
    }

    if (ChangeWindowMessageFilterEx != NULL) // 7 or later
    {
        if (tmpNodokaTasktray != NULL)
            ChangeWindowMessageFilterEx(tmpNodokaTasktray, message, flag7, 0);
        else if (ChangeWindowMessageFilter != NULL)
            ChangeWindowMessageFilter(message, flag); // UIAccess=true 対応: ウィンドウ未取得時はプロセス全体に適用
    }
    else if (ChangeWindowMessageFilter != NULL) // Vista
    {
        ChangeWindowMessageFilter(message, flag);
    }
}

/// Enable/DisableChangeWindowMessageFilter() for Vista
void EnableChangeWindowMessageFilter()
{
	const UINT WM_NODOKA_MESSAGE = RegisterWindowMessage(addSessionId(WM_NODOKA_MESSAGE_NAME).c_str());

	if (!g.m_isMaped)
		mapHookData();

	myChangeWindowMessageFilter(WM_APP + 101, MSGFLT_ADD); // WM_APP_taskTrayNotify
	myChangeWindowMessageFilter(WM_APP + 102, MSGFLT_ADD); // WM_APP_msgStreamNotify
	myChangeWindowMessageFilter(WM_APP + 103, MSGFLT_ADD); // WM_APP_notifyFocus
	myChangeWindowMessageFilter(WM_APP + 104, MSGFLT_ADD); // WM_APP_notifyVKey
	myChangeWindowMessageFilter(WM_APP + 105, MSGFLT_ADD); // WM_APP_targetNotify
	myChangeWindowMessageFilter(WM_APP + 110, MSGFLT_ADD); // WM_APP_engineNotify
	myChangeWindowMessageFilter(WM_APP + 115, MSGFLT_ADD); // WM_APP_dlglogNotify
	myChangeWindowMessageFilter(WM_APP + 116, MSGFLT_ADD); // WM_APP_SendKey
	myChangeWindowMessageFilter(WM_APP + 120, MSGFLT_ADD); // WM_APP_NotifyThreadDetach
	//ChangeWindowMessageFilter(WM_APP + 121, MSGFLT_ADD);	// WM_APP_NotifySync	not use.
	myChangeWindowMessageFilter(WM_APP + 122, MSGFLT_ADD);			  // WM_APP_NotifyLockState
	myChangeWindowMessageFilter(WM_APP + 123, MSGFLT_ADD);			  // WM_APP_NotifyTSF
	myChangeWindowMessageFilter(WM_APP + 124, MSGFLT_ADD);			  // WM_APP_escapeNLSKeysFailed
	myChangeWindowMessageFilter(WM_APP + 201, MSGFLT_ADD);			  // for Touchpad
	myChangeWindowMessageFilter(WM_APP + 202, MSGFLT_ADD);			  // for gamepad
	myChangeWindowMessageFilter(WM_APP + 203, MSGFLT_ADD);			  // for mouse
	myChangeWindowMessageFilter(WM_NODOKA_MESSAGE, MSGFLT_ADD);		  // for Touchpad
	myChangeWindowMessageFilter(WM_CREATE, MSGFLT_ADD);				  //
	myChangeWindowMessageFilter(WM_DESTROY, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_MOVE, MSGFLT_ADD);				  //
	myChangeWindowMessageFilter(WM_SIZE, MSGFLT_ADD);				  //
	myChangeWindowMessageFilter(WM_ACTIVATE, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_SETFOCUS, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_KILLFOCUS, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_PAINT, MSGFLT_ADD);				  //
	myChangeWindowMessageFilter(WM_CLOSE, MSGFLT_ADD);				  //
	myChangeWindowMessageFilter(WM_QUERYENDSESSION, MSGFLT_ADD);	  //
	myChangeWindowMessageFilter(WM_ACTIVATEAPP, MSGFLT_ADD);		  //
	myChangeWindowMessageFilter(WM_MOUSEACTIVATE, MSGFLT_ADD);		  //
	myChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_NOTIFY, MSGFLT_ADD);				  //
	myChangeWindowMessageFilter(WM_SETICON, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_NCDESTROY, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_NCHITTEST, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_NCACTIVATE, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_GETDLGCODE, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_INPUT, MSGFLT_ADD);				  //
	myChangeWindowMessageFilter(WM_INPUT_DEVICE_CHANGE, MSGFLT_ADD);  //
	myChangeWindowMessageFilter(WM_KEYDOWN, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_KEYUP, MSGFLT_ADD);				  //
	myChangeWindowMessageFilter(WM_CHAR, MSGFLT_ADD);				  //
	myChangeWindowMessageFilter(WM_DEADCHAR, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_SYSKEYDOWN, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_SYSKEYUP, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_IME_STARTCOMPOSITION, MSGFLT_ADD); //
	myChangeWindowMessageFilter(WM_IME_ENDCOMPOSITION, MSGFLT_ADD);   //
	myChangeWindowMessageFilter(WM_INITDIALOG, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_COMMAND, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_SYSCOMMAND, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_TIMER, MSGFLT_ADD);				  //
	myChangeWindowMessageFilter(WM_MOUSEMOVE, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_LBUTTONDOWN, MSGFLT_ADD);		  //
	myChangeWindowMessageFilter(WM_LBUTTONUP, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_ENTERMENULOOP, MSGFLT_ADD);		  //
	myChangeWindowMessageFilter(WM_EXITMENULOOP, MSGFLT_ADD);		  //
	myChangeWindowMessageFilter(WM_SIZING, MSGFLT_ADD);				  //
	myChangeWindowMessageFilter(WM_IME_NOTIFY, MSGFLT_ADD);			  //
	myChangeWindowMessageFilter(WM_WTSSESSION_CHANGE, MSGFLT_ADD);	//
	myChangeWindowMessageFilter(WM_LBUTTONDBLCLK, MSGFLT_ADD);		  //
	myChangeWindowMessageFilter(WM_MBUTTONDOWN, MSGFLT_ADD);		  //
}

void DisableChangeWindowMessageFilter()
{
	const UINT WM_NODOKA_MESSAGE = RegisterWindowMessage(addSessionId(WM_NODOKA_MESSAGE_NAME).c_str());

	myChangeWindowMessageFilter(WM_APP + 101, MSGFLT_REMOVE); // WM_APP_taskTrayNotify
	myChangeWindowMessageFilter(WM_APP + 102, MSGFLT_REMOVE); // WM_APP_msgStreamNotify
	myChangeWindowMessageFilter(WM_APP + 103, MSGFLT_REMOVE); // WM_APP_notifyFocus
	myChangeWindowMessageFilter(WM_APP + 104, MSGFLT_REMOVE); // WM_APP_notifyVKey
	myChangeWindowMessageFilter(WM_APP + 105, MSGFLT_REMOVE); // WM_APP_targetNotify
	myChangeWindowMessageFilter(WM_APP + 110, MSGFLT_REMOVE); // WM_APP_engineNotify
	myChangeWindowMessageFilter(WM_APP + 115, MSGFLT_REMOVE); // WM_APP_dlglogNotify
	myChangeWindowMessageFilter(WM_APP + 116, MSGFLT_REMOVE); // WM_APP_SendKey
	myChangeWindowMessageFilter(WM_APP + 120, MSGFLT_REMOVE); // WM_APP_NotifyThreadDetach
	//myChangeWindowMessageFilter(WM_APP + 121, MSGFLT_REMOVE);	// WM_APP_NotifySync
	myChangeWindowMessageFilter(WM_APP + 122, MSGFLT_REMOVE);	  // WM_APP_NotifyLockState
	myChangeWindowMessageFilter(WM_APP + 123, MSGFLT_REMOVE);	  // WM_APP_NotifyTSF
	myChangeWindowMessageFilter(WM_APP + 124, MSGFLT_REMOVE);	  // WM_APP_escapeNLSKeysFailed
	myChangeWindowMessageFilter(WM_APP + 201, MSGFLT_REMOVE);	  // for Touchpad
	myChangeWindowMessageFilter(WM_APP + 202, MSGFLT_REMOVE);	  // for gamepad
	myChangeWindowMessageFilter(WM_APP + 203, MSGFLT_REMOVE);	  // for mouse
	myChangeWindowMessageFilter(WM_NODOKA_MESSAGE, MSGFLT_REMOVE); // for Touchpad
}

// determine processor architecture
void getSysInfo(SYSTEM_INFO *sysInfo) noexcept
{
	static bool first = true;
	static void(WINAPI * pGetNativeSystemInfo)(LPSYSTEM_INFO);
	if (first)
	{
		first = false;
		HMODULE hKernel32 = LoadLibrary(_T("kernel32"));
		if (hKernel32 != NULL)
		{
			pGetNativeSystemInfo =
				reinterpret_cast<void(WINAPI *)(LPSYSTEM_INFO)>(GetProcAddress(hKernel32, "GetNativeSystemInfo"));
		}
		else
		{
			pGetNativeSystemInfo = nullptr;
		}
	}
	if (pGetNativeSystemInfo)
	{
		pGetNativeSystemInfo(sysInfo);
		return;
	}
	GetSystemInfo(sysInfo);
}

/// Check OS
BOOL isXP() noexcept
{
	// Vista以降なら FALSE. XP, 2000だったら TRUE;
	return !IsWindowsVistaOrGreater();
}

/// map hook data
static bool mapHookData()
{
	DWORD dwDesiredAccess = FILE_MAP_READ | FILE_MAP_WRITE;

	g.m_hHookDataDll = OpenFileMapping(dwDesiredAccess, FALSE, addSessionId(HOOK_DATA_NAME).c_str());

	if (g.m_hHookDataDll == NULL)
	{
		dwDesiredAccess = FILE_MAP_READ;
		g.m_hHookDataDll = OpenFileMapping(dwDesiredAccess, FALSE, addSessionId(HOOK_DATA_NAME).c_str());

		if (g.m_hHookDataDll == NULL)
		{
			g.m_isMaped = false;
			return false;
		}
	}

	g.hookDataDll = (HookData *)MapViewOfFile(g.m_hHookDataDll, dwDesiredAccess, 0, 0, sizeof(HookData));
	if (g.hookDataDll == NULL)
	{
		unmapHookData();
		g.m_isMaped = false;
		return false;
	}

	g.m_isMaped = true;
	return true;
}

/// unmap hook data
static void unmapHookData() noexcept
{
	g.m_isMaped = false;

	if (g.hookDataDll != NULL)
		if (!UnmapViewOfFile(g.hookDataDll))
			return;
	g.hookDataDll = NULL;

	if (g.m_hHookDataDll != NULL)
		CloseHandle(g.m_hHookDataDll);
	g.m_hHookDataDll = NULL;
}

/// notify
bool notify(void *i_data, size_t i_dataSize) noexcept
{
	COPYDATASTRUCT cd;
#ifdef _WIN64
	DWORD_PTR result;
#else
	DWORD result;
#endif
	HMODULE hModule;

	if (!g.m_isMaped || !m_isHooked)
		return false;

	cd.dwData = reinterpret_cast<Notify *>(i_data)->m_type;
	cd.cbData = (DWORD)i_dataSize;
	cd.lpData = (LPVOID)i_data;

	bool retVal = false;
	DWORD dwLock = 0;

	hModule = GetModuleHandle(TEXT("user32.dll"));

	if (hModule != NULL)
	{
		switch (cd.dwData)
		{
			/* Type_sync は、hook.cppから、nodoka.cpp notifyHandler(COPYDATASTRUCT *cd) での処理に移動したため、ここでは処理しない。
		case Notify::Type_sync:
			SendNotifyMessage((HWND)(ULONG_PTR)g.hookDataDll->m_hwndTaskTray,
				WM_APP_NotifySync,
				NULL, NULL);
			retVal = true;
			break;
		*/
		case Notify::Type_lockState:
			dwLock = reinterpret_cast<NotifyLockState *>(i_data)->m_isNumLockToggled ? 1 : 0;
			dwLock += reinterpret_cast<NotifyLockState *>(i_data)->m_isCapsLockToggled ? 2 : 0;
			dwLock += reinterpret_cast<NotifyLockState *>(i_data)->m_isScrollLockToggled ? 4 : 0;
			dwLock += reinterpret_cast<NotifyLockState *>(i_data)->m_isKanaLockToggled ? 8 : 0;
			dwLock += reinterpret_cast<NotifyLockState *>(i_data)->m_isImeLockToggled ? 16 : 0;
			dwLock += reinterpret_cast<NotifyLockState *>(i_data)->m_isImeCompToggled ? 32 : 0;
			dwLock += reinterpret_cast<NotifyLockState *>(i_data)->m_isCandidateWindow ? 64 : 0;

			SendNotifyMessage((HWND)(ULONG_PTR)g.hookDataDll->m_hwndTaskTray,
                  WM_APP_NotifyLockState,
                  (WPARAM)dwLock, NULL);
			retVal = true;
			break;
		case Notify::Type_threadDetach:
			SendNotifyMessage((HWND)(ULONG_PTR)g.hookDataDll->m_hwndTaskTray,
                  WM_APP_NotifyThreadDetach,
                  reinterpret_cast<NotifyThreadDetach *>(i_data)->m_threadId, NULL);
			retVal = true;
			break;
		default:
			if (!SendMessageTimeout((HWND)(ULONG_PTR)g.hookDataDll->m_hwndTaskTray, WM_COPYDATA, NULL, reinterpret_cast<LPARAM>(&cd), SMTO_ABORTIFHUNG | SMTO_NORMAL, 5000, &result))
				retVal = false;
			else
				retVal = true;
			break;
		}
	}
	return retVal;
}

/// get class name and title name
static void getClassNameTitleName(HWND i_hwnd, bool i_isInMenu,
								  tstringi *o_className,
								  tstring *o_titleName)
{
	tstringi &className = *o_className;
	tstring &titleName = *o_titleName;

	bool isTheFirstTime = true;

	if (i_isInMenu)
	{
		className = titleName = _T("MENU");
		isTheFirstTime = false;
	}

	while (true)
	{
		_TCHAR buf[MAX(GANA_MAX_PATH, GANA_MAX_ATOM_LENGTH)];

		// get class name
		if (i_hwnd)
			GetClassName(i_hwnd, buf, NUMBER_OF(buf));
		else
			GetModuleFileName(GetModuleHandle(NULL), buf, NUMBER_OF(buf));
		buf[NUMBER_OF(buf) - 1] = _T('\0');
		if (isTheFirstTime)
			className = buf;
		else
			className = tstringi(buf) + _T(":") + className;

		// get title name
		if (i_hwnd)
		{
			GetWindowText(i_hwnd, buf, NUMBER_OF(buf));
			buf[NUMBER_OF(buf) - 1] = _T('\0');
			for (_TCHAR *b = buf; *b; ++b)
				if (_istlead(*b) && b[1])
					b++;
				else if (_istcntrl(*b))
					*b = _T('?');
		}
		if (isTheFirstTime)
			titleName = buf;
		else
			titleName = tstring(buf) + _T(":") + titleName;

		// next loop or exit
		if (!i_hwnd)
			break;
		i_hwnd = GetParent(i_hwnd);
		isTheFirstTime = false;
	}
}

/// update show
static void updateShow(HWND i_hwnd, NotifyShow::Show i_show)
{
	bool isMDI = false;

	if (!i_hwnd)
		return;

	LONG_PTR style = GetWindowLongPtr(i_hwnd, GWL_STYLE);
	if (!(style & WS_MAXIMIZEBOX) && !(style & WS_MAXIMIZEBOX))
		return; // ignore window that has neither maximize or minimize button

	if (style & WS_CHILD)
	{
		LONG_PTR exStyle = GetWindowLongPtr(i_hwnd, GWL_EXSTYLE);
		if (exStyle & WS_EX_MDICHILD)
		{
			isMDI = true;
		}
		else
			return; // ignore non-MDI child window case
	}

	notifyShow(i_show, isMDI);
}

/// notify WM_Targetted
static void notifyName(HWND i_hwnd, Notify::Type i_type = Notify::Type_name)
{
	tstringi className;
	tstring titleName;
	getClassNameTitleName(i_hwnd, g.m_isInMenu, &className, &titleName);

	NotifySetFocus *nfc = new NotifySetFocus;
	nfc->m_type = i_type;
	nfc->m_threadId = GetCurrentThreadId();
	nfc->m_hwnd = static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(i_hwnd));
	tcslcpy(nfc->m_className, className.c_str(), NUMBER_OF(nfc->m_className));
	tcslcpy(nfc->m_titleName, titleName.c_str(), NUMBER_OF(nfc->m_titleName));

	notify(nfc, sizeof(*nfc));
	delete nfc;
}

/// notify WM_SETFOCUS
static void notifySetFocus(bool i_doesForce = false)
{
	HWND hwnd = GetFocus();

	if (i_doesForce || hwnd != g.m_hwndFocus)
	{
		g.m_hwndFocus = hwnd;
		notifyName(hwnd, Notify::Type_setFocus);
	}
}

/// notify sync
static void notifySync()
{
	Notify n;
	n.m_type = Notify::Type_sync;
	notify(&n, sizeof(n));
}

/// notify DLL_THREAD_DETACH
static void notifyThreadDetach()
{
	NotifyThreadDetach ntd;
	ntd.m_type = Notify::Type_threadDetach;
	ntd.m_threadId = GetCurrentThreadId();
	notify(&ntd, sizeof(ntd));
}

/// notify WM_COMMAND, WM_SYSCOMMAND
static void notifyCommand(HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam)
{
	bool m_doesNotifyCommand;

	if (g.hookDataDll == NULL)
		mapHookData();

	if (g.hookDataDll != NULL)
		m_doesNotifyCommand = g.hookDataDll->m_doesNotifyCommand;
	else
		m_doesNotifyCommand = false;

	if (m_doesNotifyCommand)
	{
		NotifyCommand ntc;
		ntc.m_type = Notify::Type_command;
		ntc.m_hwnd = static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(i_hwnd));
		ntc.m_message = i_message;
		ntc.m_wParam = i_wParam;
		ntc.m_lParam = i_lParam;
		notify(&ntc, sizeof(ntc));
	}
}

/// notify show of current window
static void notifyShow(NotifyShow::Show i_show, bool i_isMDI)
{
	NotifyShow ns;
	ns.m_type = Notify::Type_show;
	ns.m_show = i_show;
	ns.m_isMDI = i_isMDI;
	notify(&ns, sizeof(ns));
}

/// notify log
static void notifyLog(_TCHAR *i_msg)
{
	NotifyLog nl;
	nl.m_type = Notify::Type_log;
	tcslcpy(nl.m_msg, i_msg, NUMBER_OF(nl.m_msg));
	notify(&nl, sizeof(nl));
}

/// &Recenter
static void funcRecenter(HWND i_hwnd)
{
	_TCHAR buf[MAX(GANA_MAX_PATH, GANA_MAX_ATOM_LENGTH)];
	GetClassName(i_hwnd, buf, NUMBER_OF(buf));
	bool isEdit;
	if (_tcsicmp(buf, _T("Edit")) == 0)
		isEdit = true;
	else if (_tcsnicmp(buf, _T("RichEdit"), 8) == 0)
		isEdit = false;
	else
		return; // this function only works for Edit control

	LONG_PTR style = GetWindowLongPtr(i_hwnd, GWL_STYLE);
	if (!(style & ES_MULTILINE))
		return; // this function only works for multi line Edit control

	RECT rc;
	GetClientRect(i_hwnd, &rc);
	POINTL p = {(rc.right + rc.left) / 2, (rc.top + rc.bottom) / 2};
	int line;
	if (isEdit)
	{
		line = static_cast<int>(SendMessage(i_hwnd, EM_CHARFROMPOS, 0, MAKELPARAM(p.x, p.y)));
		line = HIWORD(line);
	}
	else
	{
		LONG_PTR ci = SendMessage(i_hwnd, EM_CHARFROMPOS, 0, (LPARAM)&p);
		line = static_cast<int>(SendMessage(i_hwnd, EM_EXLINEFROMCHAR, 0, ci));
	}
	int caretLine = static_cast<int>(SendMessage(i_hwnd, EM_LINEFROMCHAR, -1, 0));
	SendMessage(i_hwnd, EM_LINESCROLL, 0, caretLine - line);
}

// &SetImeConvStatus
static void funcSetImeConvStatus(HWND i_hwnd, int i_status)
{
	HIMC hIMC;
	DWORD dwConv, dwSent;

	hIMC = ImmGetContext(i_hwnd);
	if (hIMC == INVALID_HANDLE_VALUE)
		return;

	if (ImmGetOpenStatus(hIMC))
	{
		ImmGetConversionStatus(hIMC, &dwConv, &dwSent);
		//ImmSetConversionStatus(hIMC, 0, 0);
		//dwConv |= ((DWORD)i_status) & 0x000f;
		dwConv = (DWORD)i_status;
		ImmSetConversionStatus(hIMC, dwConv, dwSent);
	}
	ImmReleaseContext(i_hwnd, hIMC);
}

// &SetImeStatus
static void funcSetImeStatus(HWND i_hwnd, int i_status)
{
	HIMC hIMC;
	BOOL bSetStatus;
	BOOL bStatus;

	hIMC = ImmGetContext(i_hwnd);
	if (hIMC == INVALID_HANDLE_VALUE)
		return;

	if (i_status == 0)
		bSetStatus = FALSE;

	if (i_status == 1)
		bSetStatus = TRUE;

	if (i_status < 0)
	{
		// toggle: Get Current Status
		i_status = ImmGetOpenStatus(hIMC);

		if (i_status != 0)
		{
			bSetStatus = FALSE; // Set Close
		}
		else
		{
			bSetStatus = TRUE; // Set Open
		}
	}

	bStatus = ImmSetOpenStatus(hIMC, bSetStatus);
	ImmReleaseContext(i_hwnd, hIMC);
}

// &SetImeString
static void funcSetImeString(HWND i_hwnd, int i_size)
{
#if defined(_WINNT)
	_TCHAR *buf = new _TCHAR(i_size);
	DWORD len = 0;
	_TCHAR ImeDesc[GANA_MAX_ATOM_LENGTH];
	UINT ImeDescLen;
	DWORD error;
	DWORD denom = 1;
	HANDLE hPipe = CreateFile(addSessionId(HOOK_PIPE_NAME).c_str(), GENERIC_READ,
							  FILE_SHARE_READ, (SECURITY_ATTRIBUTES *)NULL,
							  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);
	error = ReadFile(hPipe, buf, i_size, &len, NULL);
	CloseHandle(hPipe);

	ImeDescLen = ImmGetDescription(GetKeyboardLayout(0),
								   ImeDesc, sizeof(ImeDesc));
	if (_tcsncmp(ImeDesc, _T("SKKIME"), ImeDescLen) > 0)
		denom = sizeof(_TCHAR);

	HIMC hIMC = ImmGetContext(i_hwnd);
	if (hIMC == INVALID_HANDLE_VALUE)
		return;

	int status = ImmGetOpenStatus(hIMC);

#if 0
	if(isXP() == FALSE)
	{
		// Vista以降
		DWORD nSize = ImmGetCompositionString(hIMC, GCS_COMPSTR, NULL, 0);
		if(nSize)
		{
			_TCHAR *buf2 = new _TCHAR(nSize);
			ImmGetCompositionString(hIMC, GCS_COMPSTR, buf2, nSize);
			ImmSetCompositionString(hIMC, SCS_SETSTR, buf2, nSize, NULL, 0);
			ImmSetCompositionString(hIMC, SCS_SETSTR, buf, len / denom, NULL, 0);
			delete buf;
			delete buf2;
		}
	}
	else
#endif
	{
		ImmSetCompositionString(hIMC, SCS_SETSTR, buf, len / denom, NULL, 0);
		delete buf;
	}
	ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
	if (!status)
		ImmSetOpenStatus(hIMC, status);
	ImmReleaseContext(i_hwnd, hIMC);
#endif // _WINNT
}

/// notify lock state
/*DllExport*/ void notifyLockState(int i_cause)
{
	NotifyLockState n;

	n.m_type = Notify::Type_lockState;
	n.m_isNumLockToggled = !!(GetKeyState(VK_NUMLOCK) & 1);
	n.m_isCapsLockToggled = !!(GetKeyState(VK_CAPITAL) & 1);
	n.m_isScrollLockToggled = !!(GetKeyState(VK_SCROLL) & 1);

	if (i_cause == 8)
		g.m_isKanaLock = !!(GetKeyState(VK_KANA) & 1);

	n.m_isImeLockToggled = g.m_isImeLock;
	n.m_isKanaLockToggled = g.m_isKanaLock;
	n.m_isImeCompToggled = g.m_isImeCompositioning;
	n.m_isCandidateWindow = g.m_isCandidateWindow;
	n.m_debugParam = i_cause;
	notify(&n, sizeof(n));
}

DllExport void notifyLockState()
{
	notifyLockState(9);
}

/// hook of GetMessage
LRESULT CALLBACK getMessageProc(int i_nCode, WPARAM i_wParam, LPARAM i_lParam)
{
	if (!g.m_isMaped)
		mapHookData();

	if (i_nCode < 0 || !m_isHooked || g.hookDataDll == NULL)
		goto through;

	bool m_correctKanaLockHandling = FALSE;

	if ((i_nCode == HC_ACTION) && (i_wParam & PM_REMOVE))
	{
		m_correctKanaLockHandling = g.hookDataDll->m_correctKanaLockHandling;

		MSG &msg = *reinterpret_cast<MSG *>(i_lParam);

		switch (msg.message)
		{
		case WM_COMMAND:
		case WM_SYSCOMMAND:
			notifyCommand(msg.hwnd, msg.message, msg.wParam, msg.lParam);
			break;
		case WM_KEYUP:
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		{
			if (HIMC hIMC = ImmGetContext(msg.hwnd))
			{
				bool prev = g.m_isImeLock;
				g.m_isImeLock = !!ImmGetOpenStatus(hIMC);
				ImmReleaseContext(msg.hwnd, hIMC);
				if (prev != g.m_isImeLock)
				{
					notifyLockState(1);
				}
			}

			int nVirtKey = (int)msg.wParam;

			if (nVirtKey == VK_CAPITAL ||
				nVirtKey == VK_NUMLOCK ||
				nVirtKey == VK_KANA ||
				nVirtKey == VK_SCROLL)
			{
				if (m_correctKanaLockHandling)
				{
					notifyLockState(2); // KL- enable時
				}
				else
				{
					notifyLockState(8); // KL- disable時
				}
			}
			//notifyLockState(1);		ここにnotifyを入れるとキー入力のたびに処理が入るので、キー押しっぱなしを引き起こす。
			break;
		}
		case WM_IME_STARTCOMPOSITION:
			g.m_isImeCompositioning = true;
			notifyLockState(3);
			break;
		case WM_IME_ENDCOMPOSITION:
			g.m_isImeCompositioning = false;
			notifyLockState(4);
			break;
		default:
			if (msg.message == g.m_WM_NODOKA_MESSAGE)
			{
				switch (msg.wParam)
				{
				case NodokaMessage_notifyName:
					notifyName(msg.hwnd);
					break;
				case NodokaMessage_funcRecenter:
					funcRecenter(msg.hwnd);
					break;
				case NodokaMessage_funcSetImeStatus:
					funcSetImeStatus(msg.hwnd, static_cast<int>(msg.lParam));
					break;
				case NodokaMessage_funcSetImeString:
					funcSetImeString(msg.hwnd, static_cast<int>(msg.lParam));
					break;
				case NodokaMessage_funcSetImeConvStatus:
					funcSetImeConvStatus(msg.hwnd, static_cast<int>(msg.lParam));
					break;
				}
			}
			break;
		}
	}
through:
	return CallNextHookEx(m_hHookGetMessage, i_nCode, i_wParam, i_lParam);
}

/// hook of GetMessage for Sync
LRESULT CALLBACK getSyncProc(int i_nCode, WPARAM i_wParam, LPARAM i_lParam)
{
	if (!g.m_isMaped)
		mapHookData();

	if (i_nCode < 0 || !m_isHooked || g.hookDataDll == NULL)
		goto through;

	USHORT m_syncKey;
	bool m_syncKeyIsExtended;

	m_syncKey = g.hookDataDll->m_syncKey;
	m_syncKeyIsExtended = g.hookDataDll->m_syncKeyIsExtended;

	MSG &msg = (*(MSG *)i_lParam);

	//if(i_nCode == HC_ACTION && i_wParam == PM_REMOVE)		// CbC to AbC and &Sync TIMEOUT.
	{
		if (msg.message == WM_KEYUP)
		{
			USHORT scanCode = (USHORT)((msg.lParam >> 16) & 0xff);
			bool isExtended = !!(msg.lParam & (1 << 24));
			if (scanCode == m_syncKey && isExtended == m_syncKeyIsExtended)
				notifySync();
		}
	}
through:
	return CallNextHookEx(m_hHookGetSync, i_nCode, i_wParam, i_lParam);
}

/// hook of GetMessage for WM_INPUT(rawinput) Trial
LRESULT CALLBACK getRawInput(int i_nCode, WPARAM i_wParam, LPARAM i_lParam)
{
	if (!g.m_isMaped)
		mapHookData();

	if (i_nCode < 0 || !m_isHooked || g.hookDataDll == NULL)
		goto through;

	MSG &msg = (*(MSG *)i_lParam);

	KBDLLHOOKSTRUCT Kbll;
	LPKBDLLHOOKSTRUCT pKbll = &Kbll;
	LPARAM lParam = reinterpret_cast<LPARAM>(pKbll);

	UINT dwSize = 40;
	static BYTE lpb[40];
	unsigned int result;

	HMODULE dll = LoadLibrary(TEXT("user32.dll"));
	static FUNCTYPE4 myGetRawInputData = (FUNCTYPE4)GetProcAddress(dll, "GetRawInputData");

	if ((i_nCode == HC_ACTION) && (i_wParam & PM_REMOVE) && (myGetRawInputData != NULL))
	{
		if (msg.message == WM_INPUT)
		{
			// lParamがNULLならSendInputの出力なので抜ける。実際はNULLにはならない。
			if (msg.lParam == NULL)
				goto through;

			//PeekMessage(&msg, NULL, WM_INPUT, WM_INPUT, PM_REMOVE);	// REMOVEしてもアプリには送られてしまう。
			//GetMessage(&msg, NULL, WM_INPUT, WM_INPUT);				// Getしてしまうとengineでは処理されなくなる。

			// RAWINTPUTの取得
			if (myGetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) > 0)
			{
				RAWINPUT *raw = (RAWINPUT *)lpb;
				if (raw->header.dwType == RIM_TYPEKEYBOARD)
				{
					if (raw->data.keyboard.ExtraInformation & 0x100) // SendInput()でEngineから送られたデータを抑止する。
						return 1;

					// RAWKEYBOARDからKBDLLHOOKSTRUCTを組み立てる
					Kbll.vkCode = raw->data.keyboard.VKey;
					Kbll.scanCode = raw->data.keyboard.MakeCode;
					Kbll.flags = raw->data.keyboard.Flags;
					Kbll.time = 0;
					Kbll.dwExtraInfo = NULL;

					// engineにデータを渡す
					if (g.m_keyboardDetour && g.m_engine && g.hookDataDll->m_keyboard_hook == 2)
					{
						result = g.m_keyboardDetour(g.m_engine, NULL, lParam);
						if (result)
						{
							//GetMessage(&msg, NULL, WM_INPUT, WM_INPUT); // ここでGetしても2個になる。入れ替えは出来る。
							return 1;
						}
					}
				}
			}
		}
	}
through:
	return CallNextHookEx(m_hHookKeyboardProc, i_nCode, i_wParam, i_lParam);
}

/// hook of SendMessage
LRESULT CALLBACK callWndProc(int i_nCode, WPARAM i_wParam, LPARAM i_lParam)
{
	if (!g.m_isMaped)
		mapHookData();

	if (i_nCode < 0 || !m_isHooked || g.hookDataDll == NULL)
		goto through;

	CWPSTRUCT &cwps = *(CWPSTRUCT *)i_lParam;

	bool m_CaretBlinkTime = FALSE;
	DWORD m_BlinkTimeOn, m_BlinkTimeOff = 0;

	if (i_nCode == HC_ACTION)
	{
		m_CaretBlinkTime = g.hookDataDll->m_CaretBlinkTime;
		m_BlinkTimeOn = g.hookDataDll->m_BlinkTimeOn;
		m_BlinkTimeOff = g.hookDataDll->m_BlinkTimeOff;

		switch (cwps.message)
		{
		case WM_ACTIVATEAPP:
		case WM_NCACTIVATE:
			if (i_wParam)
			{
				notifySetFocus();
			}
			break;
		case WM_SYSCOMMAND:
			switch (cwps.wParam)
			{
			case SC_MAXIMIZE:
			case SC_MAXIMIZE2:
				updateShow(cwps.hwnd, NotifyShow::Show_Maximized);
				break;
			case SC_MINIMIZE:
			case SC_MINIMIZE2:
				updateShow(cwps.hwnd, NotifyShow::Show_Minimized);
				break;
			case SC_RESTORE:
			case SC_RESTORE2:
				updateShow(cwps.hwnd, NotifyShow::Show_Normal);
				break;
			default:
				break;
			}
			/* through below */
		case WM_COMMAND:
			notifyCommand(cwps.hwnd, cwps.message, cwps.wParam, cwps.lParam);
			break;
		case WM_SIZE:
			switch (cwps.wParam)
			{
			case SIZE_MAXIMIZED:
				updateShow(cwps.hwnd, NotifyShow::Show_Maximized);
				break;
			case SIZE_MINIMIZED:
				updateShow(cwps.hwnd, NotifyShow::Show_Minimized);
				break;
			case SIZE_RESTORED:
				updateShow(cwps.hwnd, NotifyShow::Show_Normal);
				break;
			default:
				break;
			}
			break;
		case WM_MOUSEACTIVATE:
			notifySetFocus();
			break;
		case WM_ACTIVATE:
			if (LOWORD(cwps.wParam) != WA_INACTIVE)
			{
				notifySetFocus();
				if (HIWORD(cwps.wParam)) // check minimized flag
				{
					// minimized flag on
					notifyShow(NotifyShow::Show_Minimized, false);
					//notifyShow(NotifyShow::Show_Normal, true);
				}
			}
			break;
		case WM_ENTERMENULOOP:
			g.m_isInMenu = true;
			notifySetFocus(true);
			break;
		case WM_EXITMENULOOP:
			g.m_isInMenu = false;
			notifySetFocus(true);
			break;
		case WM_SETFOCUS:
			g.m_isInMenu = false;
			notifySetFocus();
			notifyLockState(5);
			break;
		case WM_IME_STARTCOMPOSITION:
			g.m_isImeCompositioning = true;
			notifyLockState(6);
			break;
		case WM_IME_ENDCOMPOSITION:
			g.m_isImeCompositioning = false;
			notifyLockState(7);
			break;

		case WM_IME_NOTIFY:
			HIMC hIMC = ImmGetContext(cwps.hwnd);
			if (cwps.wParam == IMN_SETOPENSTATUS)
				if (hIMC != NULL)
				{
					g.m_isImeLock = !!ImmGetOpenStatus(hIMC);
					if (m_CaretBlinkTime)
						if (g.m_isImeLock)
						{
							SetCaretBlinkTime(m_BlinkTimeOn);
						}
						else
						{
							SetCaretBlinkTime(m_BlinkTimeOff);
						}
					notifyLockState(2);
				}

			if (hIMC != NULL)
				ImmReleaseContext(cwps.hwnd, hIMC);

			if (cwps.wParam == IMN_OPENCANDIDATE)
			{
				g.m_isCandidateWindow = true;
				notifyLockState(5);
			}
			if (cwps.wParam == IMN_CLOSECANDIDATE)
			{
				g.m_isCandidateWindow = false;
				notifyLockState(5);
			}
			break;
		}
	}
through:
	return CallNextHookEx(m_hHookCallWndProc, i_nCode, i_wParam, i_lParam);
}

/// install hooks
DllExport bool installHooks()
{
	bool iHookError = false;

	m_hHookGetMessage = SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)getMessageProc, g.m_hInstDLL, 0);
	if (m_hHookGetMessage == NULL)
		iHookError = true;

	m_hHookCallWndProc = SetWindowsHookEx(WH_CALLWNDPROC, (HOOKPROC)callWndProc, g.m_hInstDLL, 0);
	if (m_hHookCallWndProc == NULL)
		iHookError = true;

	m_hHookGetSync = SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)getSyncProc, g.m_hInstDLL, 0);
	if (m_hHookGetSync == NULL)
		iHookError = true;

	m_isHooked = !iHookError;

	// UIAccess=true 環境では DLL_PROCESS_ATTACH 時点でウィンドウが未作成のため
	// ChangeWindowMessageFilterEx が失敗している。フック設定完了後（ウィンドウ作成済み）
	// に再度呼び直すことで m_hwndTaskTray が正しく設定された状態で適用する。
	EnableChangeWindowMessageFilter();

	return iHookError;
}

/// uninstall hooks
DllExport bool uninstallHooks()
{
	m_isHooked = false;

	if (m_hHookGetMessage)
	{
		UnhookWindowsHookEx(m_hHookGetMessage);
		DisableChangeWindowMessageFilter();
	}
	m_hHookGetMessage = NULL;

	if (m_hHookCallWndProc)
		UnhookWindowsHookEx(m_hHookCallWndProc);
	m_hHookCallWndProc = NULL;

	if (m_hHookGetSync)
		UnhookWindowsHookEx(m_hHookGetSync);
	m_hHookGetSync = NULL;

	return true;
}

static LRESULT CALLBACK lowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	MSLLHOOKSTRUCT *pMsll = (MSLLHOOKSTRUCT *)lParam;

	if (!g.m_isMaped)
		mapHookData();

	if (g.hookDataDll == NULL || nCode < 0 || nCode == HC_NOREMOVE)
		goto through;

	if (pMsll->flags & LLMHF_INJECTED) // KB2973201 LLMHF_LOWER_IL_INJECTED as LLMHF_INJECTED
		goto through;

	if (g.m_mouseDetour && g.m_engine)
	{
		unsigned int result;
		result = static_cast<unsigned int>(g.m_mouseDetour(g.m_engine, wParam, lParam));
		if (result)
		{
			return 1;
		}
	}

through:
	return CallNextHookEx(m_hHookMouseProc, nCode, wParam, lParam);
}

static LRESULT CALLBACK lowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	KBDLLHOOKSTRUCT *pKbll = (KBDLLHOOKSTRUCT *)lParam;

	NODOKA_TRACE(_T("lowLevelKeyboardProc: called\n"));

	if (!g.m_isMaped)
		mapHookData();

	if (g.hookDataDll == NULL || nCode != HC_ACTION)
	{
		NODOKA_TRACE(_T("lowLevelKeyboardProc: hookDataDll==NULL or nCode!=HC_ACTION, goto through\n"));
		goto through;
	}

	// lParam & 0x40000000 チェックは WH_KEYBOARD 用（lParam がメッセージ値）のコードを誤って
	// WH_KEYBOARD_LL に流用したもの。LL フックでは lParam は KBDLLHOOKSTRUCT* のポインタ値
	// であり bit30 チェックは無意味（誤動作の原因）なので削除。

	// 注入キーを素通りさせる:
	// - LLKHF_INJECTED: 通常の SendInput 注入（他プロセスや低整合性プロセスから）
	// - NODOKA_SELF_INJECT_EXTRA_INFO: nodoka 自身が LL フックモードで注入したキー
	//   （uiAccess=true 時、自プロセスの uiAccess ウィンドウへの注入で
	//     LLKHF_INJECTED が設定されない場合の回避策）
	if ((pKbll->flags & LLKHF_INJECTED) ||
	    (pKbll->dwExtraInfo == NODOKA_SELF_INJECT_EXTRA_INFO))
	{
		NODOKA_TRACE(_T("lowLevelKeyboardProc: INJECTED or self-inject, goto through\n"));
		goto through;
	}

	if (g.m_keyboardDetour && g.m_engine && g.hookDataDll->m_keyboard_hook == 1)
	{
		unsigned int result;

		// Windows 8 Alt-Tab, Win-X W.A.
		if (g.hookDataDll->m_win8wa == 1)
		{
			if ((pKbll->vkCode == VK_TAB) && (pKbll->flags & LLKHF_ALTDOWN))
			{
				goto through;
			}
			if ((pKbll->vkCode == 0x58) && (GetAsyncKeyState(VK_LWIN) || GetAsyncKeyState(VK_RWIN)))
			{
				goto through;
			}
		}

		result = g.m_keyboardDetour(g.m_engine, wParam, lParam);
		if (result)
		{
			return 1;
		}
	}
	else
	{
		NODOKA_TRACE(_T("lowLevelKeyboardProc: detour conditions failed: detour=%p engine=%p hook=%d\n"),
			g.m_keyboardDetour, g.m_engine,
			g.hookDataDll ? (int)g.hookDataDll->m_keyboard_hook : -1);
	}
through:
	return CallNextHookEx(m_hHookKeyboardProc, nCode, wParam, lParam);
}

/// install keyboard hook
DllExport int installKeyboardHook(INPUT_DETOUR i_keyboardDetour, Engine *i_engine, bool i_install)
{
	if (!g.m_isMaped)
		mapHookData();

	if (g.hookDataDll->m_keyboard_hook == 0)
		return 0;

	if (g.hookDataDll->m_device == true)
		return 0;

	int m_mode = 0;
	if (i_install == true)
	{
		if (g.hookDataDll->m_keyboard_hook == 1)
			m_mode = 1;
		if (g.hookDataDll->m_keyboard_hook == 2)
			m_mode = 2;
	}

	switch (m_mode)
	{
	case 2: // install rawinput
		g.m_keyboardDetour = i_keyboardDetour;
		g.m_engine = i_engine;
		m_hHookKeyboardProc = SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)getRawInput, g.m_hInstDLL, 0);
		//DBG_PRINT(_T("install rawinput hook. %d"), m_hHookKeyboardProc);
		break;
	case 1: // install LL Hook
		g.m_keyboardDetour = i_keyboardDetour;
		g.m_engine = i_engine;
		m_hHookKeyboardProc = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)lowLevelKeyboardProc, g.m_hInstDLL, 0);
		//DBG_PRINT(_T("install LL hook. %d"), m_hHookKeyboardProc);
		break;
	case 0: // uninstall LL Hook
		if (m_hHookKeyboardProc)
			UnhookWindowsHookEx(m_hHookKeyboardProc);
		m_hHookKeyboardProc = NULL;
		break;
	default:
		break;
	}
	return 0;
}

/// WH_KEYBOARD（スレッドローカル）フックコールバック
/// ログウィンドウにフォーカスがあるとき WH_KEYBOARD_LL が呼ばれない問題（uiAccess=true の
/// 同プロセスウィンドウに対して OS が LL フックを省略する動作）を補完する。
/// nodoka.exe メインスレッドにインストールし、同プロセスの uiAccess ウィンドウへの
/// キー入力も確実にインターセプトする。
/// WH_GETMESSAGE スレッドローカルフックコールバック
/// ログウィンドウ等の同プロセス uiAccess ウィンドウへのキー配送を制御する。
/// WH_KEYBOARD と異なり WH_GETMESSAGE は MSG.message = WM_NULL で物理キーを
/// 実際に抑制できるため、WH_KEYBOARD_LL と同等の動作（物理キー抑制＋再注入）を実現できる。
static LRESULT CALLBACK keyboardLocalProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode != HC_ACTION)
		return CallNextHookEx(m_hHookLocalKeyboardProc, nCode, wParam, lParam);

	// PM_NOREMOVE（PeekMessage のキューを消費しない呼び出し）はスキップ
	if (wParam != PM_REMOVE)
		return CallNextHookEx(m_hHookLocalKeyboardProc, nCode, wParam, lParam);

	MSG *msg = reinterpret_cast<MSG *>(lParam);
	if (msg->message != WM_KEYDOWN && msg->message != WM_KEYUP &&
	    msg->message != WM_SYSKEYDOWN && msg->message != WM_SYSKEYUP)
		return CallNextHookEx(m_hHookLocalKeyboardProc, nCode, wParam, lParam);

	// 自己注入キー（nodoka が SendInput で再注入したキー）は素通り
	if (GetMessageExtraInfo() == NODOKA_SELF_INJECT_EXTRA_INFO)
	{
		DBG_PRINT(_T("keyboardLocalProc: self-inject, passing through\n"));
		return CallNextHookEx(m_hHookLocalKeyboardProc, nCode, wParam, lParam);
	}

	if (!g.m_isMaped)
		mapHookData();

	if (g.hookDataDll == NULL || g.m_keyboardDetour == NULL || g.m_engine == NULL)
		return CallNextHookEx(m_hHookLocalKeyboardProc, nCode, wParam, lParam);

	// LL フックモード以外は素通り
	if (g.hookDataDll->m_keyboard_hook != 1)
		return CallNextHookEx(m_hHookLocalKeyboardProc, nCode, wParam, lParam);

	// MSG.lParam から KBDLLHOOKSTRUCT を構築してキーボードデトゥアーを呼び出す
	// MSG.lParam bits 16-23: OEM スキャンコード
	// MSG.lParam bit  24   : 拡張キーフラグ
	// MSG.lParam bit  29   : コンテキストコード（ALT 押下中 = 1、WM_SYSKEYDOWN/UP 用）
	// UP/DOWN はメッセージ種別で判定する（lParam bit 31 より確実）
	KBDLLHOOKSTRUCT kbdll = {};
	kbdll.vkCode   = (DWORD)msg->wParam;
	kbdll.scanCode = (msg->lParam >> 16) & 0xFF;
	kbdll.flags    = 0;
	if (msg->lParam & (1UL << 24)) kbdll.flags |= LLKHF_EXTENDED;
	if (msg->lParam & (1UL << 29)) kbdll.flags |= LLKHF_ALTDOWN;
	if (msg->message == WM_KEYUP || msg->message == WM_SYSKEYUP)
		kbdll.flags |= LLKHF_UP;
	kbdll.time        = msg->time;
	kbdll.dwExtraInfo = 0;

	DBG_PRINT(_T("keyboardLocalProc: vk=0x%x sc=0x%x flags=0x%x\n"),
	          kbdll.vkCode, kbdll.scanCode, kbdll.flags);

	g.m_keyboardDetour(g.m_engine, (WPARAM)msg->wParam, (LPARAM)&kbdll);

	// 物理キーを抑制する: WH_KEYBOARD の return 1 と異なり WH_GETMESSAGE では
	// MSG.message = WM_NULL に書き換えることで実際にウィンドウへの配送をキャンセルできる。
	// nodoka がキーボードハンドラ経由で SendInput 再注入したキーだけがウィンドウに届く。
	msg->message = WM_NULL;
	return CallNextHookEx(m_hHookLocalKeyboardProc, nCode, wParam, lParam);
}

/// スレッドローカル WH_GETMESSAGE フックのインストール／アンインストール
/// WH_KEYBOARD ではなく WH_GETMESSAGE を使うことで物理キーを MSG.message=WM_NULL で
/// 実際に抑制できる（WH_KEYBOARD の return 1 は WH_KEYBOARD_LL と異なり無視される）。
DllExport void installLocalKeyboardHook(DWORD i_threadId, bool i_install)
{
	if (i_install)
	{
		if (m_hHookLocalKeyboardProc == NULL)
		{
			m_hHookLocalKeyboardProc = SetWindowsHookEx(
				WH_GETMESSAGE, (HOOKPROC)keyboardLocalProc, g.m_hInstDLL, i_threadId);
			DBG_PRINT(_T("installLocalKeyboardHook: thread=%u handle=%p\n"),
			          i_threadId, (void*)m_hHookLocalKeyboardProc);
		}
	}
	else
	{
		if (m_hHookLocalKeyboardProc)
		{
			UnhookWindowsHookEx(m_hHookLocalKeyboardProc);
			m_hHookLocalKeyboardProc = NULL;
			NODOKA_TRACE(_T("installLocalKeyboardHook: uninstalled\n"));
		}
	}
}

/// install mouse hook
DllExport int installMouseHook(INPUT_DETOUR i_mouseDetour, Engine *i_engine, bool i_install)
{
	if (g.hookDataDll->m_mouse_hook != 1)
	{
		return 0;
	}
	if (i_install)
	{
		if (!g.m_isMaped)
			mapHookData();

		g.m_mouseDetour = i_mouseDetour;
		g.m_engine = i_engine;
		g.hookDataDll->m_mouseHookType = MouseHookType_None;
		m_hHookMouseProc =
			SetWindowsHookEx(WH_MOUSE_LL, (HOOKPROC)lowLevelMouseProc, g.m_hInstDLL, 0);
	}
	else
	{
		if (m_hHookMouseProc)
			UnhookWindowsHookEx(m_hHookMouseProc);
		m_hHookMouseProc = NULL;
	}
	return 0;
}
