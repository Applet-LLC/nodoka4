//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// hook.h

#ifndef _HOOK_H
#define _HOOK_H

#include "misc.h"
#include <tchar.h>
#include "..\sirius_sdk\commonValues.h"
///
#define HOOK_PIPE_NAME _T("\\\\.\\pipe\\appletkan\\nodoka\\{4B22D464-7A4E-494b-982A-C2B2BBAAF9F3}") _T(VERSION)
///
#define WM_NODOKA_MESSAGE_NAME _T("appletkan\\nodoka\\WM_NODOKA_MESSAGE")
#define WM_APP_NotifyThreadDetach WM_APP + 120
#define WM_APP_NotifySync WM_APP + 121
#define WM_APP_NotifyLockState WM_APP + 122
#define WM_APP_NotifyTSF WM_APP + 123

#define HOOK_DATA_NAME _T("{08D6E55C-5103-4e00-8209-A1C4AB13BBEF}") \
					   _T("VERSION")

#define WPARAM64 unsigned __int64
#define LPARAM64 __int64

#define WPARAM86 _W64 unsigned int
#define LPARAM86 _W64 long

#define DEBUG_LOGA(_x_) OutputDebugString _x_;
#define DEBUG_LOG(_x_) ;
//#define DEBUG_LOG(_x_)	OutputDebugString _x_;

#define DBG_PRINT_LENGTH 1440

inline void DBG_PRINT(const _TCHAR *fmt, ...)
{
	_TCHAR buf[DBG_PRINT_LENGTH];
	va_list ap;
	va_start(ap, fmt);
	_vsntprintf_s(buf, DBG_PRINT_LENGTH, _TRUNCATE, fmt, ap);
	va_end(ap);
	OutputDebugString(buf);
}

// for ChangeWindowMessageFilter
typedef BOOL(__stdcall *FUNCTYPE)(UINT, DWORD);
#define MSGFLT_ADD 1
#define MSGFLT_REMOVE 2

typedef BOOL(__stdcall *FUNCTYPE7)(HWND, UINT, DWORD, int);
#define MSGFLT_RESET 0
#define MSGFLT_ALLOW 1

// for myGetRawInputData
typedef UINT(CALLBACK *FUNCTYPE4)(HANDLE, UINT, LPVOID, PUINT, UINT);

// for Remote Desktop or change user
#ifndef WM_WTSSESSION_CHANGE // WinUser.h
#define WM_WTSSESSION_CHANGE 0x02B1
#endif

#ifndef WTS_CONSOLE_CONNECT // WinUser.h
#define WTS_CONSOLE_CONNECT 0x1
#define WTS_CONSOLE_DISCONNECT 0x2
#define WTS_REMOTE_CONNECT 0x3
#define WTS_REMOTE_DISCONNECT 0x4
#define WTS_SESSION_LOGON 0x5
#define WTS_SESSION_LOGOFF 0x6
#define WTS_SESSION_LOCK 0x7
#define WTS_SESSION_UNLOCK 0x8
#define WTS_SESSION_REMOTE_CONTROL 0x9
#endif

#ifndef IMC_GETCONVERSIONMODE
#define IMC_GETCONVERSIONMODE 0x0001
#endif

///
enum NodokaMessage
{
	NodokaMessage_notifyName,
	NodokaMessage_funcRecenter,
	NodokaMessage_funcSetImeStatus,
	NodokaMessage_funcSetImeString,
	NodokaMessage_funcSetImeConvStatus,
};

///
struct Notify
{
	///
	enum Type
	{
		Type_setFocus,	 /// NotifySetFocus
		Type_name,		   /// NotifySetFocus
		Type_lockState,	/// NotifyLockState
		Type_sync,		   /// Notify
		Type_threadDetach, /// NotifyThreadDetach
		Type_command,	  /// notifyCommand
		Type_show,		   /// NotifyShow
		Type_log,		   /// NotifyLog
	};
	Type m_type;		///
	DWORD m_debugParam; /// (for debug)
};

///
struct NotifySetFocus : public Notify
{
	DWORD m_threadId;				   ///
	DWORD m_hwnd;					   /// HWND to DWORD for x64,x86
	_TCHAR m_className[GANA_MAX_PATH]; ///
	_TCHAR m_titleName[GANA_MAX_PATH]; ///
};

///
struct NotifyLockState : public Notify
{
	bool m_isNumLockToggled;	///
	bool m_isCapsLockToggled;   ///
	bool m_isScrollLockToggled; ///
	bool m_isKanaLockToggled;   ///
	bool m_isImeLockToggled;	///
	bool m_isImeCompToggled;	///
	bool m_isCandidateWindow;   ///
};

///
struct NotifyThreadDetach : public Notify
{
	DWORD m_threadId; ///
};

///
struct NotifyCommand : public Notify
{
	DWORD m_hwnd;	/// HWND to DWORD for x86tox64
	UINT m_message;  ///
	WPARAM m_wParam; ///
	LPARAM m_lParam; ///
};

struct NotifyCommand86 : public Notify
{
	DWORD m_hwnd;	  /// HWND to DWORD for x64,x86
	UINT m_message;	///
	WPARAM86 m_wParam; ///
	LPARAM86 m_lParam; ///
};

enum
{
	NOTIFY_MESSAGE_SIZE = sizeof(NotifySetFocus), ///
};

///
struct NotifyShow : public Notify
{
	///
	enum Show
	{
		Show_Normal,
		Show_Maximized,
		Show_Minimized,
	};
	Show m_show;  ///
	bool m_isMDI; ///
};

///
struct NotifyLog : public Notify
{
	_TCHAR m_msg[GANA_MAX_PATH]; ///
};

///
enum MouseHookType
{
	MouseHookType_None = 0,			   /// none
	MouseHookType_Wheel = 1 << 0,	  /// wheel
	MouseHookType_WindowMove = 1 << 1, /// window move
};

class Engine;
typedef unsigned int(WINAPI *INPUT_DETOUR)(Engine *i_engine, WPARAM i_wParam, LPARAM i_lParam);

///
class HookData
{
public:
	USHORT m_syncKey;				///
	bool m_syncKeyIsExtended;		///
	bool m_doesNotifyCommand;		///
	DWORD m_hwndTaskTray;			///
	bool m_correctKanaLockHandling; /// does use KL- ?
	bool m_CaretBlinkTime;
	DWORD m_BlinkTimeOff;
	DWORD m_BlinkTimeOn;
	bool m_device; /// nodokad
	int m_keyboard_hook;
	int m_mouse_hook;
	int m_win8wa;
	MouseHookType m_mouseHookType; ///
	int m_mouseHookParam;		   ///
	DWORD m_hwndMouseHookTarget;   ///
	POINT m_mousePos;			   ///
	DWORD m_WaitKey;			   ///
	bool m_UseTSF;				   ///
	CcommonValues *pCv;			   ///
	bool m_RDP;					   ///
};

///
#define DllExport __declspec(dllexport)
///
#define DllImport __declspec(dllimport)

#ifndef _HOOK_CPP
extern DllImport bool installHooks();
extern DllImport bool uninstallHooks();
extern DllImport int installKeyboardHook(INPUT_DETOUR i_keyboardDetour, Engine *i_engine, bool i_install);
extern DllImport int installMouseHook(INPUT_DETOUR i_mouseDetour, Engine *i_engine, bool i_install);
extern DllImport bool notify(void *data, size_t sizeof_data);
extern DllImport void notifyLockState();
#endif // !_HOOK_CPP

#endif // !_HOOK_H
