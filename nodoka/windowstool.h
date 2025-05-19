//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// windowstool.h

#ifndef _WINDOWSTOOL_H
#define _WINDOWSTOOL_H

#include "stringtool.h"
#include <windows.h>
#include <tchar.h>
#include <stdarg.h>

#define DBG_PRINTF_LENGTH 256

inline void DBG_PRINTF(const _TCHAR *fmt, ...)
{
	_TCHAR buf[DBG_PRINTF_LENGTH];
	va_list ap;
	va_start(ap, fmt);
	_vsntprintf_s(buf, DBG_PRINTF_LENGTH, _TRUNCATE, fmt, ap);
	va_end(ap);
	OutputDebugString(buf);
}

/// instance handle of this application
extern HINSTANCE g_hInst;
extern HWND m_hwndSetting;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// resource

/// load resource string
extern tstring loadString(UINT i_id);

/// load small icon resource (it must be deleted by DestroyIcon())
extern HICON loadSmallIcon(UINT i_id);

///load big icon resource (it must be deleted by DestroyIcon())
extern HICON loadBigIcon(UINT i_id);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// window

/// resize the window (it does not move the window)
extern bool resizeWindow(HWND i_hwnd, int i_w, int i_h, bool i_doRepaint);

/** get rect of the window in client coordinates.
@return rect of the window in client coordinates */
extern bool getChildWindowRect(HWND i_hwnd, RECT *o_rc);
extern bool myGetWindowRect(HWND i_hwnd, RECT *o_rc);

/** set small icon to the specified window.
@return handle of previous icon or NULL */
extern HICON setSmallIcon(HWND i_hwnd, UINT i_id);

/** set big icon to the specified window.
@return handle of previous icon or NULL */
extern HICON setBigIcon(HWND i_hwnd, UINT i_id);

/// remove icon from a window that is set by setSmallIcon
extern void unsetSmallIcon(HWND i_hwnd);

/// remove icon from a window that is set by setBigIcon
extern void unsetBigIcon(HWND i_hwnd);

/// get toplevel (non-child) window
extern HWND getToplevelWindow(HWND i_hwnd, bool *io_isMDI);

/// move window asynchronously
extern void asyncMoveWindow(HWND i_hwnd, int i_x, int i_y);

/// move window asynchronously
extern void asyncMoveWindow(HWND i_hwnd, int i_x, int i_y, int i_w, int i_h);

/// resize asynchronously
extern void asyncResize(HWND i_hwnd, int i_w, int i_h);

/// get dll version
extern DWORD getDllVersion(const _TCHAR *i_dllname);
#define PACKVERSION(major, minor) MAKELONG(minor, major)

// workaround of SetForegroundWindow
extern bool setForegroundWindow(HWND i_hwnd);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// dialog

/// get/set GWL_USERDATA
template <class T>
inline T getUserData(HWND i_hwnd, T *i_wc)
{
	return (*i_wc = reinterpret_cast<T>(GetWindowLongPtr(i_hwnd, GWLP_USERDATA)));
}

///
template <class T>
inline T setUserData(HWND i_hwnd, T i_wc)
{
	SetWindowLongPtr(i_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(i_wc));
	return i_wc;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// RECT

///
inline int rcWidth(const RECT *i_rc) { return i_rc->right - i_rc->left; }

///
inline int rcHeight(const RECT *i_rc) { return i_rc->bottom - i_rc->top; }

///
inline bool isRectInRect(const RECT *i_rcin, const RECT *i_rcout)
{
	return (i_rcout->left <= i_rcin->left &&
			i_rcin->right <= i_rcout->right &&
			i_rcout->top <= i_rcin->top &&
			i_rcin->bottom <= i_rcout->bottom);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// edit control

/// returns bytes of text
extern size_t editGetTextBytes(HWND i_hwnd);

/// delete a line
extern void editDeleteLine(HWND i_hwnd, size_t i_n);

/// insert text at last
extern void editInsertTextAtLast(HWND i_hwnd, const tstring &i_text,
								 size_t i_threshold);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Windows2000/XP specific API

/// SetLayeredWindowAttributes API
typedef BOOL(WINAPI *SetLayeredWindowAttributes_t)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
extern SetLayeredWindowAttributes_t setLayeredWindowAttributes;

/// MonitorFromWindow API
extern HMONITOR(WINAPI *monitorFromWindow)(HWND hwnd, DWORD dwFlags);

/// GetMonitorInfo API
extern BOOL(WINAPI *getMonitorInfo)(HMONITOR hMonitor, LPMONITORINFO lpmi);

/// EnumDisplayMonitors API
extern BOOL(WINAPI *enumDisplayMonitors)(HDC hdc, LPRECT lprcClip, MONITORENUMPROC lpfnEnum, LPARAM dwData);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// WindowsXP specific API

/// WTSRegisterSessionNotification API
typedef BOOL(WINAPI *WTSRegisterSessionNotification_t)(HWND hWnd, DWORD dwFlags);
extern WTSRegisterSessionNotification_t wtsRegisterSessionNotification;

/// WTSUnRegisterSessionNotification API
typedef BOOL(WINAPI *WTSUnRegisterSessionNotification_t)(HWND hWnd);
extern WTSUnRegisterSessionNotification_t wtsUnRegisterSessionNotification;

/// WTSGetActiveConsoleSessionId API
typedef DWORD(WINAPI *WTSGetActiveConsoleSessionId_t)(void);
extern WTSGetActiveConsoleSessionId_t wtsGetActiveConsoleSessionId;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Utility

// PathRemoveFileSpec()
tstring pathRemoveFileSpec(const tstring &i_path);

// _tgetenv() to _tgetenv_s()
wchar_t *GetEnv(const wchar_t *varname);

// IsWow64Message()
BOOL IsWow64MessageLocal();

// check Windows version i_major.i_minor or later
BOOL checkWindowsVersion(DWORD i_major, DWORD i_minor);

// for def option DesktopView
#ifndef LVM_FIRST
#define LVM_FIRST 0x1000 // ListView messages
#endif

#if (_WIN32_WINNT < 0x0501)
#define LV_VIEW_ICON 0x0000
#define LV_VIEW_DETAILS 0x0001
#define LV_VIEW_SMALLICON 0x0002
#define LV_VIEW_LIST 0x0003
#define LV_VIEW_TILE 0x0004

#define LVM_SETVIEW (LVM_FIRST + 142)
#endif

#endif // _WINDOWSTOOL_H
