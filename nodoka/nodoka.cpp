//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// nodoka.cpp

#define APSTUDIO_INVOKED

#include "misc.h"
#include "compiler_specific_func.h"
#include "dlginvestigate.h"
#include "dlglog.h"
#include "dlgsetting.h"
#include "dlgversion.h"
#include "engine.h"
#include "errormessage.h"
#include "focus.h"
#include "function.h"
#include "hook.h"
#include "nodoka.h"
#include "nodokaipc.h"
#include "nodokarc.h"
#include "msgstream.h"
#include "multithread.h"
#include "registry.h"
#include "setting.h"
#include "target.h"
#include "windowstool.h"
#include "rawinput.h"
#include "fixscancodemap.h"
#include <boost/program_options.hpp>
#include <iostream>
#include <process.h>
#include <time.h>
#include <commctrl.h>
#include <wtsapi32.h>
#include <Msctf.h>
#include "..\sirius_sdk\commonValues.h"

/// define
#define ID_MENUITEM_reloadBegin _APS_NEXT_COMMAND_VALUE
typedef SIRIUS_HOOK_API CcommonValues *(*SiriusSetupHookPtr)(DWORD dwMessageId);
typedef SIRIUS_HOOK_API void *(*SiriusReleaseHookPtr)();

typedef UINT(CALLBACK *FUNCTYPE2)(HANDLE, UINT, LPVOID, PUINT);
static FUNCTYPE2 myGetRawInputDeviceInfo = (FUNCTYPE2)GetProcAddress(GetModuleHandle(L"user32.dll"), "GetRawInputDeviceInfoW");

typedef UINT(CALLBACK *FUNCTYPE4)(HANDLE, UINT, LPVOID, PUINT, UINT);
static FUNCTYPE4 myGetRawInputData = (FUNCTYPE4)GetProcAddress(GetModuleHandle(L"user32.dll"), "GetRawInputData");

typedef UINT(CALLBACK *FUNCTYPE5)(WELL_KNOWN_SID_TYPE, PSID, PSID, DWORD *);

/// Prototype
#ifdef _WIN64
void run_nodoka_x86(void);
void exit_nodoka_x86(void);
#endif

void convertRegistry(void);
void SetChangeWindowMessageFilter(HWND m_hwndTaskTray);

/// map hook data
bool mapHookData(void);
void unmapHookData(void);

#pragma comment(linker, "/section:shared,rws")
#pragma data_seg("shared")
HANDLE m_hHookDataExe = NULL; ///
HookData *g_hookDataExe = NULL;
#pragma data_seg()

// for Sirius TSF SDK
HMODULE hMsctf = NULL;
SiriusSetupHookPtr mySiriusSetupHook;
SiriusReleaseHookPtr mySiriusReleaseHook;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Nodoka

///
class Nodoka
{
	HWND m_hwndTaskTray;	/// tasktray window
	HWND m_hwndLog;			/// log dialog
	HWND m_hwndInvestigate; /// investigate dialog
	HWND m_hwndVersion;		/// version dialog

	UINT m_WM_TaskbarRestart;	/** window message sent when
																taskber restarts */
	UINT m_WM_NodokaIPC;		 /** IPC message sent from
															other applications */
	NOTIFYICONDATA m_ni;		 /// taskbar icon data
	HICON m_tasktrayIcon[16];	/// taskbar icon
	bool m_canUseTasktrayBaloon; ///

	tomsgstream m_log; /** log stream (output to log dialog's edit) */

	HMENU m_hMenuTaskTray; /// tasktray menu

	static const DWORD SESSION_LOCKED = 1 << 0;
	static const DWORD SESSION_DISCONNECTED = 1 << 1;
	static const DWORD SESSION_END_QUERIED = 1 << 2;
	DWORD m_sessionState;
	int m_escapeNlsKeys;
	FixScancodeMap m_fixScancodeMap;

	Setting *m_setting;			  /// current setting
	bool m_isSettingDialogOpened; /// is setting dialog opened ?

	Engine m_engine; /// engine

	bool m_usingSN;		/// using WTSRegisterSessionNotification() ?
	time_t m_startTime; /// nodoka started at ...

	enum
	{
		YAMY_TIMER_ESCAPE_NLS_KEYS = 0, ///
	};

	enum
	{
		WM_APP_taskTrayNotify = WM_APP + 101,  ///
		WM_APP_msgStreamNotify = WM_APP + 102, ///
		WM_APP_SendKey = WM_APP + 116,
		WM_APP_escapeNLSKeysFailed = WM_APP + 124, ///
		ID_TaskTrayIcon = 1,					   ///
	};

private:
	// ScancodeMap reload and engine start
	void connect()
	{
		if (!m_sessionState)
		{
			if (m_escapeNlsKeys && m_engine.getIsEnabled())
			{
				m_fixScancodeMap.escape(true);
			}
		}
		if (m_engine.m_keyboard_hook == 0)
		{
			if (!m_engine.resume())
			{
				m_engine.prepairQuit();
				PostMessage(m_hwndTaskTray, WM_CLOSE, 0, 0);
				return;
			}
			m_log << _T("resume engine") << std::endl;
		}
		else
		{
			if (!m_engine.getIsEnabled())
			{
				m_engine.enable(true);
				m_log << _T("resume nodoka") << std::endl;
				showTasktrayIcon();
			}
		}
	}

	// ScancodeMap original and engine pause
	void disconnect()
	{
		if (!m_sessionState)
		{
			if (m_escapeNlsKeys && m_engine.getIsEnabled())
			{
				m_fixScancodeMap.escape(false);
			}
		}
		if (m_engine.m_keyboard_hook == 0)
		{
			m_engine.pause();
			m_log << _T("pause engine") << std::endl;
		}
		else
		{
			if (m_engine.getIsEnabled())
			{
				m_engine.enable(false);
				m_log << _T("pause nodoka") << std::endl;
				showTasktrayIcon();
			}
		}
	}

	/// register class for tasktray
	ATOM Register_tasktray()
	{
		WNDCLASS wc;
		wc.style = 0;
		wc.lpfnWndProc = tasktray_wndProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = sizeof(Nodoka *);
		wc.hInstance = g_hInst;
		wc.hIcon = NULL;
		wc.hCursor = NULL;
		wc.hbrBackground = NULL;
		wc.lpszMenuName = NULL;
		wc.lpszClassName = _T("nodokaTasktray");
		return RegisterClass(&wc);
	}

	/// notify handler
	BOOL notifyHandler(COPYDATASTRUCT *cd)
	{
		switch (cd->dwData)
		{
		case Notify::Type_setFocus:
		case Notify::Type_name:
		{
			NotifySetFocus *n = (NotifySetFocus *)cd->lpData;
			n->m_className[NUMBER_OF(n->m_className) - 1] = _T('\0');
			n->m_titleName[NUMBER_OF(n->m_titleName) - 1] = _T('\0');

			if (n->m_type == Notify::Type_setFocus)
				m_engine.setFocus(reinterpret_cast<HWND>(n->m_hwnd), n->m_threadId,
								  n->m_className, n->m_titleName, false);

			{
				Acquire a(&m_log, 1);
				m_log << _T("HWND:\t") << std::hex
					  << n->m_hwnd
					  << std::dec << std::endl;
				m_log << _T("THREADID:") << static_cast<int>(n->m_threadId)
					  << std::endl;
			}
			Acquire a(&m_log, (n->m_type == Notify::Type_name) ? 0 : 1);
			m_log << _T("CLASS:\t") << n->m_className << std::endl;
			m_log << _T("TITLE:\t") << n->m_titleName << std::endl;

			bool isMDI = true;
			HWND hwnd = getToplevelWindow(reinterpret_cast<HWND>(n->m_hwnd), &isMDI);
			RECT rc;
			if (isMDI)
			{
				getChildWindowRect(hwnd, &rc);
				m_log << _T("MDI Window Position/Size: (")
					  << rc.left << _T(", ") << rc.top << _T(") / (")
					  << rcWidth(&rc) << _T("x") << rcHeight(&rc) << _T(")")
					  << std::endl;
				hwnd = getToplevelWindow(reinterpret_cast<HWND>(n->m_hwnd), NULL);
			}

			GetWindowRect(hwnd, &rc);
			m_log << _T("Toplevel Window Position/Size: (")
				  << rc.left << _T(", ") << rc.top << _T(") / (")
				  << rcWidth(&rc) << _T("x") << rcHeight(&rc) << _T(")")
				  << std::endl;

			SystemParametersInfo(SPI_GETWORKAREA, 0, (void *)&rc, FALSE);
			m_log << _T("Desktop Window Position/Size: (")
				  << rc.left << _T(", ") << rc.top << _T(") / (")
				  << rcWidth(&rc) << _T("x") << rcHeight(&rc) << _T(")")
				  << std::endl;

			m_log << std::endl;
			break;
		}
		case Notify::Type_sync:
		{
			m_engine.syncNotify();
			break;
		}
			/* tasktray_wndProc()での処理に移動。
				case Notify::Type_lockState:
					{
					NotifyLockState *n = (NotifyLockState *)cd->lpData;
					m_engine.setLockState(n->m_isNumLockToggled,
						n->m_isCapsLockToggled,
						n->m_isScrollLockToggled,
						n->m_isKanaLockToggled,
						n->m_isImeLockToggled,
						n->m_isImeCompToggled);
					break;
					}

				case Notify::Type_threadDetach:
					{
					NotifyThreadDetach *n = (NotifyThreadDetach *)cd->lpData;
					m_engine.threadDetachNotify(n->m_threadId);
					break;
					}
*/
		case Notify::Type_command:
		{
			NotifyCommand *n = (NotifyCommand *)cd->lpData;
			;
#ifdef _WIN64
			if (IsWow64MessageLocal())
				NotifyCommand86 *n = (NotifyCommand86 *)cd->lpData;
#endif
			m_engine.commandNotify(reinterpret_cast<HWND>(n->m_hwnd), n->m_message, n->m_wParam, n->m_lParam);
			break;
		}

		case Notify::Type_show:
		{
			NotifyShow *n = (NotifyShow *)cd->lpData;
			switch (n->m_show)
			{
			case NotifyShow::Show_Maximized:
				m_engine.setShow(true, false, n->m_isMDI);
				break;
			case NotifyShow::Show_Minimized:
				m_engine.setShow(false, true, n->m_isMDI);
				break;
			case NotifyShow::Show_Normal:
			default:
				m_engine.setShow(false, false, n->m_isMDI);
				break;
			}
			break;
		}

		case Notify::Type_log:
		{
			Acquire a(&m_log, 1);
			NotifyLog *n = (NotifyLog *)cd->lpData;
			m_log << _T("hook log: ") << n->m_msg << std::endl;
			break;
		}
		}
		return true;
	}

	/// window procedure for tasktray
	static LRESULT CALLBACK tasktray_wndProc(HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam)
	{
		Nodoka *This = reinterpret_cast<Nodoka *>(GetWindowLongPtr(i_hwnd, 0));

		if (!This)
			switch (i_message)
			{
			case WM_CREATE:
				This = reinterpret_cast<Nodoka *>(
					reinterpret_cast<CREATESTRUCT *>(i_lParam)->lpCreateParams);
				This->m_fixScancodeMap.init(i_hwnd, WM_APP_escapeNLSKeysFailed);
				if (This->m_escapeNlsKeys)
				{
					This->m_fixScancodeMap.escape(true);
				}
				SetWindowLongPtr(i_hwnd, 0, (LONG_PTR)This);
				return 0;
			}
		else
			switch (i_message)
			{
			case WM_COPYDATA:
			{
				COPYDATASTRUCT *cd;
				cd = reinterpret_cast<COPYDATASTRUCT *>(i_lParam);
				return This->notifyHandler(cd);
			}
			case WM_APP_NotifyThreadDetach: // WM_APP + 120
			{
				This->m_engine.threadDetachNotify((DWORD)i_wParam);
				return TRUE;
			}
			/*
					case WM_APP_NotifySync:			// WM_APP + 121
						{
						This->m_engine.syncNotify();
						return TRUE;
						}
					*/

			// nodoka dllから NotifyLockStateでIMM系ステート取得
			case WM_APP_NotifyLockState: // WM_APP + 122
			{
				DWORD dwLock = (DWORD)i_wParam;
				;
				This->m_engine.setLockState(
					(bool)((dwLock & 0x01) == 0x01), // m_isNumLockToggled
					(bool)((dwLock & 0x02) == 0x02), // m_isCapsLockToggled
					(bool)((dwLock & 0x04) == 0x04), // m_isScrollLockToggled
					(bool)((dwLock & 0x08) == 0x08), // m_isKanaLockToggled
					(bool)((dwLock & 0x10) == 0x10), // m_isImeLockToggled
					(bool)((dwLock & 0x20) == 0x20), // m_isImeCompToggled
					(bool)((dwLock & 0x40) == 0x40)  // m_isCandidateWindow
				);

				return TRUE;
			}
			// sirius_hook_xxx.dllから来たTSF系ステート取得
			case WM_APP_NotifyTSF: // WM_APP + 123
			{
				if (i_wParam == 1) // from TsfCompartmnet
				{
					This->m_engine.setLockState2();
				}
				if (i_wParam == 2) // from TsfCompostion
				{
					if (i_lParam == 0)
						This->m_engine.setLockState2B(false);
					if (i_lParam != 0)
						This->m_engine.setLockState2B(true);
				}
				return TRUE;
			}
			case WM_INPUT:
			{
				if (This->m_setting)
					if (This->m_setting->m_UseUnitID == 1)
						if (myGetRawInputData != NULL)
						{
							UINT dwSize = 40;
							static BYTE lpb[40];

							if (myGetRawInputData((HRAWINPUT)i_lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) > 0)
							{
								RAWINPUT *raw = (RAWINPUT *)lpb;
								if (raw->header.dwType == RIM_TYPEKEYBOARD)
								{
									HANDLE tmpHDevice = raw->header.hDevice;
									UINT k = 0;
									UINT UnitID = 0;
									while (k < 8)
									{ // キーボードは8個までサポート
										if (tmpHDevice == This->m_setting->m_keyboard_table[k].hDevice)
										{
											UnitID = This->m_setting->m_keyboard_table[k].UnitID;
											This->m_engine.setLockState3(UnitID);
											break;
										}
										k++;
									}
									if (k == 8)
									{
										This->m_engine.setLockState3(0);
									}
								}
							}
						}
				return FALSE;
			}
			case WM_INPUT_DEVICE_CHANGE:
			{
				if (This->m_setting)
					if (This->m_setting->m_UseUnitID == 1)
						if (i_wParam == 1) //GIDC_ARRIVAL　1
						{
							if (myGetRawInputDeviceInfo != NULL)
							{
								UINT dwSize = sizeof(RID_DEVICE_INFO);
								RID_DEVICE_INFO devinfo = {dwSize};

								if (myGetRawInputDeviceInfo((HANDLE)i_lParam, RIDI_DEVICEINFO, &devinfo, &dwSize) > 0)
								{
									if (devinfo.dwType == RIM_TYPEKEYBOARD)
									{
										This->m_log << _T("Keyboard device change") << std::endl;
										// 現状 reloadさせると再度WM_INPUT_DEVICE_CHANGEが発生し無限ループに陥るのでやめる。
										//This->load();
									}
								}
							}
						}
				return FALSE;
			}
			case WM_QUERYENDSESSION:
				if (!This->m_sessionState)
				{
					if (This->m_escapeNlsKeys && This->m_engine.getIsEnabled())
					{
						This->m_fixScancodeMap.escape(false);
					}
				}
				This->m_sessionState |= Nodoka::SESSION_END_QUERIED;
				This->m_engine.prepairQuit();
				PostMessage(i_hwnd, WM_CLOSE, 0, 0);
				return TRUE;

			/*
						restore NLS keys when any bits of m_sessionState is on
						and
						escape NLS keys when all bits of m_sessionState cleared
					*/
			case WM_WTSSESSION_CHANGE:
			{
				const char *m = "";
				switch (i_wParam)
				{
				case WTS_CONSOLE_CONNECT:
					m = "WTS_CONSOLE_CONNECT";
					This->m_sessionState &= ~Nodoka::SESSION_DISCONNECTED;
					This->connect();
					break;
				case WTS_CONSOLE_DISCONNECT:
					m = "WTS_CONSOLE_DISCONNECT";
					This->disconnect();
					This->m_sessionState |= Nodoka::SESSION_DISCONNECTED;
					break;
				case WTS_REMOTE_CONNECT:
					m = "WTS_REMOTE_CONNECT";
					This->m_sessionState &= ~Nodoka::SESSION_DISCONNECTED;
					This->connect();
					if (This->m_engine.m_keyboard_hook == 0)
					{
						This->m_log << _T("Can not use Remote Desktop. You can rerun nodoka -k") << std::endl;
					}
					break;
				case WTS_REMOTE_DISCONNECT:
					m = "WTS_REMOTE_DISCONNECT";
					This->disconnect();
					This->m_sessionState |= Nodoka::SESSION_DISCONNECTED;
					if (This->m_engine.m_keyboard_hook == 0)
					{
						This->m_log << _T("Can not use Remote Desktop. You can rerun nodoka -k") << std::endl;
					}
					break;
				case WTS_SESSION_LOGON:
					m = "WTS_SESSION_LOGON";
					break;
				case WTS_SESSION_LOGOFF:
					m = "WTS_SESSION_LOGOFF";
					break;
				case WTS_SESSION_LOCK:
					m = "WTS_SESSION_LOCK";
					//This->disconnect();
					//This->m_sessionState |= Nodoka::SESSION_LOCKED;
					break;
				case WTS_SESSION_UNLOCK:
					m = "WTS_SESSION_UNLOCK";
					//This->m_sessionState &= ~Nodoka::SESSION_LOCKED;
					//This->connect();
					break;
				case WTS_SESSION_REMOTE_CONTROL:
					//m = "WTS_SESSION_REMOTE_CONTROL";
					break;
				}
				This->m_log << _T("WM_WTSESSION_CHANGE(")
							<< i_wParam << ", " << i_lParam << "): "
							<< m << std::endl;
				return TRUE;
			}
			case WM_APP_msgStreamNotify: // WM_APP + 102
			{
				tomsgstream::StreamBuf *log =
					reinterpret_cast<tomsgstream::StreamBuf *>(i_lParam);
				const tstring &str = log->acquireString();
				editInsertTextAtLast(GetDlgItem(This->m_hwndLog, IDC_EDIT_log),
									 str, 65000);
				log->releaseString();
				return 0;
			}

			case WM_APP_taskTrayNotify: // WM_APP + 101
			{
				if (i_wParam == ID_TaskTrayIcon)
					switch (i_lParam)
					{
					case WM_RBUTTONUP:
					{
						POINT p;
						CHECK_TRUE(GetCursorPos(&p));
						SetForegroundWindow(i_hwnd);
						HMENU hMenuSub = GetSubMenu(This->m_hMenuTaskTray, 0);
						if (This->m_engine.getIsEnabled())
							CheckMenuItem(hMenuSub, ID_MENUITEM_disable,
										  MF_UNCHECKED | MF_BYCOMMAND);
						else
							CheckMenuItem(hMenuSub, ID_MENUITEM_disable,
										  MF_CHECKED | MF_BYCOMMAND);
						CHECK_TRUE(SetMenuDefaultItem(hMenuSub,
													  ID_MENUITEM_investigate, FALSE));

						// create reload menu
						HMENU hMenuSubSub = GetSubMenu(hMenuSub, 1);
						Registry reg(NODOKA_REGISTRY_ROOT);
						int nodokaIndex;
						reg.read(_T(".nodokaIndex"), &nodokaIndex, 0);
						while (DeleteMenu(hMenuSubSub, 0, MF_BYPOSITION))
							;
						tregex getName(_T("^([^;]*);"));
						for (int index = 0;; index++)
						{
							_TCHAR buf[100];
							_sntprintf_s(buf, NUMBER_OF(buf), _TRUNCATE, _T(".nodoka%d"), index);
							tstringi dot_nodoka;
							if (!reg.read(buf, &dot_nodoka))
								break;
							tsmatch what;
							if (boost::regex_search(dot_nodoka, what, getName))
							{
								MENUITEMINFO mii;
								std::memset(&mii, 0, sizeof(mii));
								mii.cbSize = sizeof(mii);
								mii.fMask = MIIM_ID | MIIM_STATE | MIIM_TYPE;
								mii.fType = MFT_STRING;
								mii.fState =
									MFS_ENABLED | ((nodokaIndex == index) ? MFS_CHECKED : 0);
								mii.wID = ID_MENUITEM_reloadBegin + index;
								tstringi name(what.str(1));
								mii.dwTypeData = const_cast<_TCHAR *>(name.c_str());
								mii.cch = (UINT)name.size();

								InsertMenuItem(hMenuSubSub, index, TRUE, &mii);
							}
						}

						// show popup menu
						TrackPopupMenu(hMenuSub, TPM_LEFTALIGN,
									   p.x, p.y, 0, i_hwnd, NULL);
						// TrackPopupMenu may fail (ERROR_POPUP_ALREADY_ACTIVE)
						PostMessage(i_hwnd, WM_NULL, NULL, NULL);
						break;
					}
					case WM_LBUTTONDOWN:
						SendMessage(i_hwnd, WM_COMMAND,
									MAKELONG(ID_MENUITEM_log, 0), 0);
						break;

					case WM_LBUTTONDBLCLK:
						SendMessage(i_hwnd, WM_COMMAND,
									MAKELONG(ID_MENUITEM_investigate, 0), 0);
						break;

					case WM_MBUTTONDOWN:
						SendMessage(i_hwnd, WM_COMMAND,
									MAKELONG(ID_MENUITEM_setting, 0), 0);
						break;
					}
				return 0;
			}
			case WM_APP_escapeNLSKeysFailed: // WM_APP + 124
				if (i_lParam)
				{
					int ret;
					This->m_log << _T("escape NLS keys done code=") << i_wParam << std::endl;
					switch (i_wParam)
					{
					case YAMY_SUCCESS:
					case YAMY_ERROR_RETRY_INJECTION_SUCCESS:
						// escape NLS keys success
						break;
					case YAMY_ERROR_TIMEOUT_INJECTION:
						ret = This->errorDialogWithCode(IDS_escapeNlsKeysRetry, i_wParam, MB_RETRYCANCEL | MB_ICONSTOP);
						if (ret == IDRETRY)
						{
							This->m_fixScancodeMap.escape(true);
						}
						break;
					default:
						This->errorDialogWithCode(IDS_escapeNlsKeysFailed, i_wParam, MB_OK);
						break;
					}
				}
				else
				{
					This->m_log << _T("restore NLS keys done with code=") << i_wParam << std::endl;
				}
				return 0;
				break;

			case WM_COMMAND:
			{
				int notify_code = HIWORD(i_wParam);
				int id = LOWORD(i_wParam);
				if (notify_code == 0) // menu
					switch (id)
					{
					default:
						if (ID_MENUITEM_reloadBegin <= id)
						{
							Registry reg(NODOKA_REGISTRY_ROOT);
							reg.write(_T(".nodokaIndex"), id - ID_MENUITEM_reloadBegin);
							This->load();
						}
						break;
					case ID_MENUITEM_reload:
						This->load();
						break;
					case ID_MENUITEM_investigate:
					{
						ShowWindow(This->m_hwndLog, SW_SHOW);
						ShowWindow(This->m_hwndInvestigate, SW_SHOW);

						RECT rc1, rc2;
						GetWindowRect(This->m_hwndInvestigate, &rc1);
						GetWindowRect(This->m_hwndLog, &rc2);

						MoveWindow(This->m_hwndLog, rc1.left, rc1.bottom,
								   rcWidth(&rc1), rcHeight(&rc2), TRUE);

						SetForegroundWindow(This->m_hwndLog);
						SetForegroundWindow(This->m_hwndInvestigate);
						break;
					}
					case ID_MENUITEM_setting:
						if (!This->m_isSettingDialogOpened)
						{
							This->m_isSettingDialogOpened = true;
							if (DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_setting),
										  NULL, dlgSetting_dlgProc))
								This->load();
							This->m_isSettingDialogOpened = false;
						}
						else
						{
							if (m_hwndSetting != NULL)
								SetForegroundWindow(m_hwndSetting);
						}
						break;
					case ID_MENUITEM_log:
						ShowWindow(This->m_hwndLog, SW_SHOW);
						SetForegroundWindow(This->m_hwndLog);
						break;
					case ID_MENUITEM_version:
						ShowWindow(This->m_hwndVersion, SW_SHOW);
						SetForegroundWindow(This->m_hwndVersion);
						break;
					case ID_MENUITEM_help:
					{
						_TCHAR buf[GANA_MAX_PATH];
						CHECK_TRUE(GetModuleFileName(g_hInst, buf, NUMBER_OF(buf)));
						tstringi helpFilename = pathRemoveFileSpec(buf);
						helpFilename += _T("\\");
						tstringi helpFilename2 = helpFilename + _T("\\") + loadString(IDS_helpFilename2);
						helpFilename += loadString(IDS_helpFilename);
						if (ERROR_FILE_NOT_FOUND == (LONG)ShellExecute(NULL, _T("open"), helpFilename.c_str(), NULL, NULL, SW_SHOWNORMAL))
						{
							ShellExecute(NULL, _T("open"), helpFilename2.c_str(), NULL, NULL, SW_SHOWNORMAL);
						}
						break;
					}
					case ID_MENUITEM_disable:
						This->m_engine.enable(!This->m_engine.getIsEnabled());
						This->showTasktrayIcon();

						if (This->m_engine.getIsEnabled())
						{
							if (This->m_escapeNlsKeys)
							{
								This->m_fixScancodeMap.escape(true);
							}
							This->m_log << _T("resume nodoka") << std::endl;
						}
						else
						{
							if (This->m_escapeNlsKeys)
							{
								This->m_fixScancodeMap.escape(false);
							}
							This->m_log << _T("pause nodoka") << std::endl;
						}
						break;
					case ID_MENUITEM_quit:
						This->m_engine.prepairQuit();
						PostMessage(i_hwnd, WM_CLOSE, 0, 0);
						break;
					}
				return 0;
			}

			case WM_APP_engineNotify: // WM_APP + 110
			{
				switch (i_wParam)
				{
				case EngineNotify_shellExecute:
					This->m_engine.shellExecute();
					break;
				case EngineNotify_loadSetting:
					This->load();
					break;
				case EngineNotify_helpMessage:
					This->showHelpMessage(false);
					if (i_lParam)
						This->showHelpMessage(true);
					break;
				case EngineNotify_showDlg:
				{
					// show investigate/log window
					int sw = (int)(i_lParam & ~NodokaDialogType_mask);
					HWND hwnd = NULL;
					switch (static_cast<NodokaDialogType>(
						i_lParam & NodokaDialogType_mask))
					{
					case NodokaDialogType_investigate:
						hwnd = This->m_hwndInvestigate;
						break;
					case NodokaDialogType_log:
						hwnd = This->m_hwndLog;
						break;
					}
					if (hwnd)
					{
						ShowWindow(hwnd, sw);
						switch (sw)
						{
						case SW_SHOWNORMAL:
						case SW_SHOWMAXIMIZED:
						case SW_SHOW:
						case SW_RESTORE:
						case SW_SHOWDEFAULT:
							SetForegroundWindow(hwnd);
							break;
						}
					}
					break;
				}
				case EngineNotify_setForegroundWindow:
					// FIXME: completely useless. why ?
					setForegroundWindow(reinterpret_cast<HWND>(i_lParam));
					{
						Acquire a(&This->m_log, 1);
						This->m_log << _T("setForegroundWindow(0x")
									<< std::hex << i_lParam << std::dec << _T(")")
									<< std::endl;
					}
					break;
				case EngineNotify_clearLog:
					SendMessage(This->m_hwndLog, WM_COMMAND, MAKELONG(IDC_BUTTON_clearLog, 0), 0);
					break;
				case EngineNotify_changeicon:
					This->showTasktrayIcon();
					break;
				default:
					break;
				}
				return 0;
			}

			case WM_APP_dlglogNotify: // WM_APP + 115
			{
				switch (i_wParam)
				{
				case DlgLogNotify_logCleared:
					This->showBanner(true);
					break;
				case DlgLogNotify_reload:
					This->load();
					break;
				default:
					break;
				}
				return 0;
			}

			case WM_APP_SendKey: // WM_APP + 116
			{
				int i_flag = 0;
				USHORT u_MakeCode = (USHORT)i_wParam;

				if (u_MakeCode > 256)
				{
					i_flag = 1;
					u_MakeCode -= 256;
				}

				This->m_engine.SendtoKeyboardHandler(1, i_flag, u_MakeCode);
				return 0;
			}

			case WM_DESTROY:
				if (This->m_usingSN)
				{
					wtsUnRegisterSessionNotification(i_hwnd);
					This->m_usingSN = false;
				}

				if (!This->m_sessionState)
				{
					if (This->m_escapeNlsKeys && This->m_engine.getIsEnabled())
					{
						This->m_fixScancodeMap.escape(false);
					}
				}

				PostQuitMessage(0);
				return 0;

			case WM_TIMER:
			{
#ifdef SAMPLE_REL
				tstring text = loadString(IDS_nodokaSeeYou);
				tstring title = loadString(IDS_nodoka);
				PostMessage(i_hwnd, WM_CLOSE, 0, 0);
				MessageBox((HWND)NULL, text.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
#endif
				break;
			}
			case WM_USER + 1997: // Alps TouchPad Message
			{
				/*
						Acquire a(&This->m_log, 1);
						This->m_log << _T("touchpad: ") << i_wParam
									<< _T(".") << (i_lParam & 0xffff)
									<< _T(".") << (i_lParam >> 16 & 0xffff)
									<< std::endl;
						*/
				static WPARAM p_WParam;
				static LPARAM p_LParam;
				WPARAM WParam;
				LPARAM LParam;

				LParam = ((i_lParam & 0xffff) << 16) + ((i_lParam >> 16) & 0xffff);
				WParam = (i_wParam & 0x0002) >> 1;

				if (p_WParam != WParam || p_LParam != LParam)
				{
					PostThreadMessage(This->m_engine.m_threadId, WM_APP + 201, WParam, LParam);
					p_WParam = WParam;
					p_LParam = LParam;
				}
				break;
			}
			default:
				if (i_message == This->m_WM_TaskbarRestart)
				{
					if (i_wParam != 8)
						This->m_engine.setIconColorNumber((int)i_wParam);
					if (This->showTasktrayIcon(true))
					{
						Acquire a(&This->m_log, 0);
						This->m_log << _T("Tasktray icon is updated.") << std::endl;
					}
					else
					{
						Acquire a(&This->m_log, 1);
						This->m_log << _T("Tasktray icon already exists.") << std::endl;
					}
					return 0;
				}
				else if (i_message == This->m_WM_NodokaIPC)
				{
					switch (static_cast<NodokaIPCCommand>(i_wParam))
					{
					case NodokaIPCCommand_Enable:
						This->m_engine.enable(!!i_lParam);
						if (This->m_escapeNlsKeys)
						{
							if (This->m_engine.getIsEnabled())
							{
								This->m_fixScancodeMap.escape(true);
							}
							else
							{
								This->m_fixScancodeMap.escape(false);
							}
						}

						This->showTasktrayIcon();
						if (i_lParam)
						{
							Acquire a(&This->m_log, 1);
							This->m_log << _T("Enabled by another application.")
										<< std::endl;
						}
						else
						{
							Acquire a(&This->m_log, 1);
							This->m_log << _T("Disabled by another application.")
										<< std::endl;
						}
						break;
					}
				}
			}
		return DefWindowProc(i_hwnd, i_message, i_wParam, i_lParam);
	}

	/// load setting
	void load()
	{
		HCURSOR hcursor, horg_cursor;
		hcursor = (HCURSOR)LoadImage(NULL, IDC_WAIT, IMAGE_CURSOR, 0, 0, LR_SHARED);
		horg_cursor = SetCursor(hcursor); // Busy Cursor
		Setting *newSetting = new Setting;

		// set symbol
		for (int i = 1; i < __argc; ++i)
		{
			if (__targv[i][0] == _T('-') && __targv[i][1] == _T('D'))
				newSetting->m_symbols.insert(__targv[i] + 2);
		}

		if (!SettingLoader(&m_log, &m_log).load(newSetting))
		{
			ShowWindow(m_hwndLog, SW_SHOW);
			SetForegroundWindow(m_hwndLog);
			delete newSetting;
			Acquire a(&m_log, 0);
			m_log << _T("error: failed to load.") << std::endl;
			SetCursor(horg_cursor); // Original Cursor
			return;
		}

		while (!m_engine.setSetting(newSetting))
			Sleep(1000);
		delete m_setting;
		m_setting = newSetting;

		SetCursor(horg_cursor); // Original Cursor
		m_log << _T("successfully loaded.") << std::endl;
	}

	// show message (a baloon from the task tray icon)
	void showHelpMessage(bool i_doesShow = true)
	{
		if (m_canUseTasktrayBaloon)
		{
			if (i_doesShow)
			{
				tstring helpMessage, helpTitle;
				m_engine.getHelpMessages(&helpMessage, &helpTitle);
				tcslcpy(m_ni.szInfo, helpMessage.c_str(), NUMBER_OF(m_ni.szInfo));
				tcslcpy(m_ni.szInfoTitle, helpTitle.c_str(),
						NUMBER_OF(m_ni.szInfoTitle));
				m_ni.dwInfoFlags = NIIF_INFO;
			}
			else
				m_ni.szInfo[0] = m_ni.szInfoTitle[0] = _T('\0');
			CHECK_TRUE(Shell_NotifyIcon(NIM_MODIFY, &m_ni));
		}
	}

	// change the task tray icon
	bool showTasktrayIcon(bool i_doesAdd = false)
	{
		int IconNumber;
		tstring text;

		tstring title = loadString(IDS_nodoka);

		IconNumber = (m_engine.getIsEnabled() ? 1 : 0) + 2 * (m_engine.getIconColorNumber());
		m_ni.hIcon = m_tasktrayIcon[IconNumber];
		m_ni.szInfo[0] = m_ni.szInfoTitle[0] = _T('\0');

		if (i_doesAdd)
		{
			/* Vistaでは、すぐ消えるが、XPでは残ったままとなるので、バルーンヘルプは表示しない。
				tstring title = loadString(IDS_nodoka) + _T(" ") + _T(VERSION);
				tcslcpy(m_ni.szInfo, title.c_str(), NUMBER_OF(m_ni.szInfo));
				m_ni.szInfoTitle[0] = _T('\0');

				m_ni.uTimeout = 5000;			// 5秒で「のどか」表示終了
				*/

			// http://support.microsoft.com/kb/418138/JA/

			int guard = 100;
			while (0 < guard)
			{
				if (!Shell_NotifyIcon(NIM_ADD, &m_ni))
				{ // 登録を試みる。
					if (!Shell_NotifyIcon(NIM_MODIFY, &m_ni))
						if (GetLastError() == ERROR_TIMEOUT) // TIME OUT ?
							Sleep(2500);					 // とりあえず待つ。
				}
				else
				{
					break; // while loopを抜ける。
				}
				guard--;
			}

			if (Shell_NotifyIcon(NIM_MODIFY, &m_ni)) // ループ終了後の状況確認。
			{
				//m_ni.uVersion = NOTIFYICON_VERSION_4;				// 通知領域に残るようになる?
				//Shell_NotifyIcon(NIM_SETVERSION, &m_ni);
				return true;
			}
			else
			{
				DWORD dwLastError = GetLastError();
				if (dwLastError == ERROR_TIMEOUT)
				{ // TIME OUT ?
					text = loadString(IDS_errorTaskTrayTimeout);
					//MessageBox((HWND)NULL, text.c_str(), title.c_str(), MB_OK | MB_ICONSTOP);
					return false;
				}
				else
				{ // TIMEOUT以外
					// ここに到達しても、実際には登録されているケースや
					// エラーダイアログを出すと固まることがあったので取りやめ。
					LPTSTR lpBufferLastError1;
					FormatMessage(
						FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
						NULL,
						dwLastError,
						LANG_USER_DEFAULT,
						(LPTSTR)&lpBufferLastError1,
						0,
						NULL);
					text = loadString(IDS_errorTaskTray) + lpBufferLastError1;
					//MessageBox((HWND)NULL, text.c_str(), title.c_str(), MB_OK | MB_ICONSTOP);
					LocalFree(lpBufferLastError1);
					return false;
				}
			}
		}
		else
		{
			return !!Shell_NotifyIcon(NIM_MODIFY, &m_ni);
		}
	}
#if 0
		// Set TasktrayIcon loop
		static void WINAPI SetTasktrayIconloop(void *dummy)
		{
			while(true){
				DWORD dwRet = WaitForMultipleObjects(2, s_SetExitEvent, FALSE, INFINITE);
				if(dwRet - WAIT_OBJECT_0 == 1)	// exit
					break;
				if(dwRet - WAIT_OBJECT_0 == 0)	// start
				{
					// load TaskTray

					// load TaskTray sucsess
						break;

					dwRet = WaitForSingleObject(s_SetExitEvent[1], 200);	// exit
					if(dwRet == WAIT_TIMEOUT)	// time out

					if(dwRet == WAIT_OBJECT_0)	// get exit
						break;
				}
			}


		}
#endif
	void showBanner(bool i_isCleared)
	{
		time_t now;
		time(&now);

		_TCHAR starttimebuf[1024];
		_TCHAR timebuf[1024];

		struct tm localtime_now;
		struct tm localtime_startTime;
		localtime_s(&localtime_now, &now);
		localtime_s(&localtime_startTime, &m_startTime);
		_tcsftime(timebuf, NUMBER_OF(timebuf), _T("%#c"), &localtime_now);
		_tcsftime(starttimebuf, NUMBER_OF(starttimebuf), _T("%#c"), &localtime_startTime);

		Acquire a(&m_log, 0);
		m_log << _T("------------------------------------------------------------") << std::endl;
		m_log << loadString(IDS_nodoka) << _T(" ") _T(VERSION);
#if 0
#ifndef NDEBUG
			m_log << _T(" (DEBUG)");
#endif
#ifdef _UNICODE
			m_log << _T(" (UNICODE)");
#endif
#endif
#ifdef _WIN64
		m_log << _T(" for x64");
#else
		m_log << _T(" for x86");
#endif
		m_log << std::endl;
		m_log << _T("  built by ")
			  << _T(LOGNAME) << _T("@") << toLower(_T(COMPUTERNAME))
			  << _T(" (") << _T(__DATE__) << _T(" ")
			  << _T(__TIME__) << _T(", ")
			  << getCompilerVersionString() << _T(")") << std::endl;
		_TCHAR modulebuf[1024];
		CHECK_TRUE(GetModuleFileName(g_hInst, modulebuf,
									 NUMBER_OF(modulebuf)));
		m_log << _T("  started at ") << starttimebuf << std::endl;
		m_log << _T("  ") << modulebuf << std::endl;

		// check remote desktop
		DWORD sessionId;
		if (!ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) || wtsGetActiveConsoleSessionId() != sessionId)
		{
			m_log << _T("  detect Remote Desktop") << std::endl;
		}

		if (m_engine.m_keyboard_hook == 0)
			m_log << _T("  use Keyboard filter driver.") << std::endl;
		if (m_engine.m_keyboard_hook == 1 && m_engine.m_win8wa == 0)
			m_log << _T("  use Keyboard LL Hook") << std::endl;
		if (m_engine.m_keyboard_hook == 1 && m_engine.m_win8wa == 1)
			m_log << _T("  use Keyboard LL Hook with Win8 WA") << std::endl;

		if (m_engine.m_keyboard_hook == 2)
			m_log << _T("  use Keyboard RawInput Hook") << std::endl;

		if (m_engine.m_mouse_hook == 1)
			m_log << _T("  use Mouse LL Hook") << std::endl;

#ifdef FOR_LIMIT
		m_log << _T("  Limit keyboard macro.") << std::endl;
#endif
		m_log << _T("------------------------------------------------------------") << std::endl;

		if (i_isCleared)
		{
			m_log << _T("log was cleared at ") << timebuf << std::endl;
		}
		else
		{
			m_log << _T("log begins at ") << timebuf << std::endl;
		}
	}

	int errorDialogWithCode(UINT ids, int code, UINT style = MB_OK | MB_ICONSTOP)
	{
		_TCHAR title[1024];
		_TCHAR text[1024];

		_sntprintf_s(title, NUMBER_OF(title), _TRUNCATE, loadString(IDS_nodoka).c_str());
		_sntprintf_s(text, NUMBER_OF(text), _TRUNCATE, loadString(ids).c_str(), code);
		return MessageBox((HWND)NULL, text, title, style);
	}

public:
	///
	Nodoka(int icon_color, int keyboard_hook, int mouse_hook, int iPause, int iLog, int iDLog, int i_escapeNlsKeys, int win8wa)
		: m_hwndTaskTray(NULL),
		  m_hwndLog(NULL),
		  m_WM_TaskbarRestart(RegisterWindowMessage(_T("TaskbarCreated"))),
		  m_WM_NodokaIPC(RegisterWindowMessage(WM_NodokaIPC_NAME)),
		  m_canUseTasktrayBaloon(PACKVERSION(5, 0) <= getDllVersion(_T("shlwapi.dll"))),
		  m_log(WM_APP_msgStreamNotify),
		  m_setting(NULL),
		  m_escapeNlsKeys(i_escapeNlsKeys),
		  m_isSettingDialogOpened(false),
		  m_sessionState(0),
		  m_engine(m_log, keyboard_hook, mouse_hook, win8wa)
	{
		time(&m_startTime);

		CHECK_TRUE(Register_focus());
		CHECK_TRUE(Register_target());
		CHECK_TRUE(Register_tasktray());

		// create windows, dialogs
		tstringi title = loadString(IDS_nodoka) + _T(" ") + _T(VERSION);
		m_hwndTaskTray = CreateWindow(_T("nodokaTasktray"), title.c_str(),
									  WS_OVERLAPPEDWINDOW,
									  CW_USEDEFAULT, CW_USEDEFAULT,
									  CW_USEDEFAULT, CW_USEDEFAULT,
									  NULL, NULL, g_hInst, this);
		CHECK_TRUE(m_hwndTaskTray);

		HWND tmpNodokaTasktray = m_hwndTaskTray;
		int tmp_count = 0;

		while (tmpNodokaTasktray != FindWindow(L"nodokaTasktray", NULL))
		{
			Sleep(100);
			tmp_count++;
			DBG_PRINT((L"tmp_count = %d"), tmp_count);

			if (tmp_count > 50)
				break;
		}

		g_hookDataExe->m_hwndTaskTray = (DWORD)m_hwndTaskTray;

		// Set ChangeWindowMessageFilter
		// manifestや署名により のどかよりも高い権限にメッセージを送れるため、フィルタの変更は本来不要だがフィルタの変更は実施する。
		// なお7以降では、SetChangeWindowMessageFilterEx()を使って、送り先のウィンドウハンドルが必要となる。

		if (m_hwndTaskTray != NULL)
			SetChangeWindowMessageFilter(m_hwndTaskTray);

		// hook and set window handle of tasktray
		CHECK_FALSE(installHooks());

		// load sirius_hook dll
#ifdef _WIN64
		hMsctf = LoadLibraryA("sirius_hook_for_nodoka_x64.dll");
#else
		hMsctf = LoadLibraryA("sirius_hook_for_nodoka_x86.dll");
#endif
		mySiriusSetupHook = (SiriusSetupHookPtr)GetProcAddress(hMsctf, "SiriusSetupHook");
		mySiriusReleaseHook = (SiriusReleaseHookPtr)GetProcAddress(hMsctf, "SiriusReleaseHook");

		DWORD wm_sirius_control = RegisterWindowMessage(L"WM_SIRIUS_CONTROL");
		g_hookDataExe->pCv = NULL;
		if (mySiriusSetupHook != NULL)
		{
			g_hookDataExe->pCv = mySiriusSetupHook(wm_sirius_control);
		}

#ifdef _WIN64
		// load x86 heler&dll, installHooks()
		run_nodoka_x86();
#endif

		// change dir
		HomeDirectories pathes;
		getHomeDirectories(&pathes);
		for (HomeDirectories::iterator i = pathes.begin(); i != pathes.end(); ++i)
			if (SetCurrentDirectory(i->c_str()))
				break;

		if (wtsRegisterSessionNotification(m_hwndTaskTray, NOTIFY_FOR_THIS_SESSION) != 0)
			m_usingSN = true;
		else
			m_usingSN = false;

		DlgLogData dld;
		dld.m_log = &m_log;
		dld.m_hwndTaskTray = m_hwndTaskTray;
		m_hwndLog =
			CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_log), NULL,
							  dlgLog_dlgProc, (LPARAM)&dld);
		CHECK_TRUE(m_hwndLog);

		DlgInvestigateData did;
		did.m_engine = &m_engine;
		did.m_hwndLog = m_hwndLog;
		m_hwndInvestigate =
			CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_investigate), NULL,
							  dlgInvestigate_dlgProc, (LPARAM)&did);
		CHECK_TRUE(m_hwndInvestigate);

		m_hwndVersion =
			CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_version),
							  NULL, dlgVersion_dlgProc,
							  (LPARAM)m_engine.getNodokadVersion().c_str());
		CHECK_TRUE(m_hwndVersion);

		// 高DPI対応
		HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
		SendMessage(m_hwndTaskTray, WM_SETFONT, (WPARAM)font, 0);
		SendMessage(m_hwndLog, WM_SETFONT, (WPARAM)font, 0);
		SendMessage(m_hwndInvestigate, WM_SETFONT, (WPARAM)font, 0);
		SendMessage(m_hwndVersion, WM_SETFONT, (WPARAM)font, 0);

		// attach log
		SendMessage(GetDlgItem(m_hwndLog, IDC_EDIT_log), EM_SETLIMITTEXT, 0, 0);
		m_log.attach(m_hwndTaskTray);

		//internal error: m_currentKeymap == NULL's workaround
		HWND hwndFore = GetDesktopWindow();
		SetForegroundWindow(hwndFore);

		// start keyboard handler thread
		m_engine.setAssociatedWndow(m_hwndTaskTray);
		m_engine.start();

		// show tasktray icon
		m_tasktrayIcon[0] = loadSmallIcon(IDI_ICON_nodoka_disabled);
		m_tasktrayIcon[1] = loadSmallIcon(IDI_ICON_nodoka);
		m_tasktrayIcon[2] = loadSmallIcon(IDI_ICON_nodoka1_disabled);
		m_tasktrayIcon[3] = loadSmallIcon(IDI_ICON_nodoka1);
		m_tasktrayIcon[4] = loadSmallIcon(IDI_ICON_nodoka2_disabled);
		m_tasktrayIcon[5] = loadSmallIcon(IDI_ICON_nodoka2);
		m_tasktrayIcon[6] = loadSmallIcon(IDI_ICON_nodoka3_disabled);
		m_tasktrayIcon[7] = loadSmallIcon(IDI_ICON_nodoka3);
		m_tasktrayIcon[8] = loadSmallIcon(IDI_ICON_nodoka4_disabled);
		m_tasktrayIcon[9] = loadSmallIcon(IDI_ICON_nodoka4);
		m_tasktrayIcon[10] = loadSmallIcon(IDI_ICON_nodoka5_disabled);
		m_tasktrayIcon[11] = loadSmallIcon(IDI_ICON_nodoka5);
		m_tasktrayIcon[12] = loadSmallIcon(IDI_ICON_nodoka6_disabled);
		m_tasktrayIcon[13] = loadSmallIcon(IDI_ICON_nodoka6);
		m_tasktrayIcon[14] = loadSmallIcon(IDI_ICON_nodoka7_disabled);
		m_tasktrayIcon[15] = loadSmallIcon(IDI_ICON_nodoka7);
		std::memset(&m_ni, 0, sizeof(m_ni));
		m_ni.uID = ID_TaskTrayIcon;
		m_ni.hWnd = m_hwndTaskTray;
		m_ni.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;

		int IconNumber = 1 + 2 * icon_color;
		m_engine.setIconColorNumber(icon_color);

		m_ni.hIcon = m_tasktrayIcon[IconNumber];
		m_ni.uCallbackMessage = WM_APP_taskTrayNotify;
		tstring tip = loadString(IDS_nodoka) + _T(" ") + _T(VERSION);
		tcslcpy(m_ni.szTip, tip.c_str(), NUMBER_OF(m_ni.szTip));
		if (m_canUseTasktrayBaloon)
		{
			m_ni.cbSize = sizeof(m_ni);
			m_ni.uFlags |= NIF_INFO;
		}
		else
			m_ni.cbSize = NOTIFYICONDATA_V1_SIZE;
		showTasktrayIcon(true);

		// create menu
		m_hMenuTaskTray = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_MENU_tasktray));
		ASSERT(m_hMenuTaskTray);

		// set pause mode
		if (iPause == 1)
			PostMessage(m_hwndTaskTray, WM_COMMAND, MAKELONG(ID_MENUITEM_disable, 0), 0);
		// set log mode
		if (iLog == 1)
			PostMessage(m_hwndTaskTray, WM_COMMAND, MAKELONG(ID_MENUITEM_log, 0), 0);
		// set 詳細Log mode
		if (iDLog == 1)
		{
			SendMessage(GetDlgItem(m_hwndLog, IDC_CHECK_detail), BM_SETCHECK, BST_CHECKED, 0);
			m_log.setDebugLevel(1);
		}
#ifdef SAMPLE_REL
		SetTimer(m_hwndTaskTray, 1, SAMPLE_TIME * 60 * 1000, NULL);
#endif
		// set initial lock state
		notifyLockState();
	}

	///

	~Nodoka()
	{
		// first, detach log from edit control to avoid deadlock
		m_log.detach();

#ifdef _WIN64
		// unload x86 heler&dll, uninstallHoooks()
		exit_nodoka_x86();
#endif

		// unload Sirius
		if (g_hookDataExe->pCv != NULL)
			g_hookDataExe->pCv = NULL;
		if (mySiriusReleaseHook != NULL)
			mySiriusReleaseHook();
		if (hMsctf != NULL)
			FreeLibrary(hMsctf);

		// stop hook for notify from nodoka.dll
		CHECK_FALSE(uninstallHooks());

#ifdef SAMPLE_REL
		KillTimer(m_hwndTaskTray, 1);
#endif
		// destroy windows
		CHECK_TRUE(DestroyWindow(m_hwndVersion));
		CHECK_TRUE(DestroyWindow(m_hwndInvestigate));
		CHECK_TRUE(DestroyWindow(m_hwndLog));
		CHECK_TRUE(DestroyWindow(m_hwndTaskTray));

		// destroy menu
		DestroyMenu(m_hMenuTaskTray);

		// delete tasktray icon
		CHECK_TRUE(Shell_NotifyIcon(NIM_DELETE, &m_ni));
		CHECK_TRUE(DestroyIcon(m_tasktrayIcon[15]));
		CHECK_TRUE(DestroyIcon(m_tasktrayIcon[14]));
		CHECK_TRUE(DestroyIcon(m_tasktrayIcon[13]));
		CHECK_TRUE(DestroyIcon(m_tasktrayIcon[12]));
		CHECK_TRUE(DestroyIcon(m_tasktrayIcon[11]));
		CHECK_TRUE(DestroyIcon(m_tasktrayIcon[10]));
		CHECK_TRUE(DestroyIcon(m_tasktrayIcon[9]));
		CHECK_TRUE(DestroyIcon(m_tasktrayIcon[8]));
		CHECK_TRUE(DestroyIcon(m_tasktrayIcon[7]));
		CHECK_TRUE(DestroyIcon(m_tasktrayIcon[6]));
		CHECK_TRUE(DestroyIcon(m_tasktrayIcon[5]));
		CHECK_TRUE(DestroyIcon(m_tasktrayIcon[4]));
		CHECK_TRUE(DestroyIcon(m_tasktrayIcon[3]));
		CHECK_TRUE(DestroyIcon(m_tasktrayIcon[2]));
		CHECK_TRUE(DestroyIcon(m_tasktrayIcon[1]));
		CHECK_TRUE(DestroyIcon(m_tasktrayIcon[0]));

		// stop keyboard handler thread
		m_engine.stop();

		if (!(m_sessionState & SESSION_END_QUERIED))
		{
			DWORD_PTR result;
			SendMessageTimeout(HWND_BROADCAST, WM_NULL, 0, 0, 0, 5000, &result);
		}

		// remove setting;
		delete m_setting;
	}

	/// message loop
	WPARAM messageLoop()
	{
		showBanner(false);
		load();

		MSG msg;
		while (0 < GetMessage(&msg, NULL, 0, 0))
		{
			if (IsDialogMessage(m_hwndLog, &msg))
				continue;
			if (IsDialogMessage(m_hwndInvestigate, &msg))
				continue;
			if (IsDialogMessage(m_hwndVersion, &msg))
				continue;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		return msg.wParam;
	}
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Functions

/// convert registry
void convertRegistry()
{
	Registry reg(NODOKA_REGISTRY_ROOT);
	tstringi dot_nodoka;
	bool doesAdd = false;
	DWORD index;
	tstringi dir, layout;

	Registry commonreg(HKEY_LOCAL_MACHINE, _T("Software\\appletkan\\nodoka"));

	if (reg.read(_T(".nodoka"), &dot_nodoka))
	{
		reg.write(_T(".nodoka0"), _T(";") + dot_nodoka + _T(";"));
		reg.remove(_T(".nodoka"));
		doesAdd = true;
		index = 0;
	}
	else if (!reg.read(_T(".nodoka0"), &dot_nodoka))
	{
		commonreg.read(_T("layout"), &layout);
		if (layout == _T("109"))
			reg.write(_T(".nodoka0"), loadString(IDS_readFromHomeDirectory) + _T(";") + _T(";-DNODOKA"));
		else if (layout == _T("104"))
			reg.write(_T(".nodoka0"), loadString(IDS_readFromHomeDirectory) + _T(";") + _T(";-DUSE104") + _T(";-DNODOKA"));
		else
			reg.write(_T(".nodoka0"), loadString(IDS_readFromHomeDirectory) + _T(";") + _T(";-DNODOKA"));
		doesAdd = true;
		index = 3; // default set as not emacs
	}
	if (doesAdd)
	{
		if (commonreg.read(_T("dir"), &dir) &&
			commonreg.read(_T("layout"), &layout))
		{
			tstringi tmp = _T(";") + dir + _T("\\dot.nodoka");
			if (layout == _T("109"))
			{
				reg.write(_T(".nodoka1"), loadString(IDS_109Emacs) + tmp + _T(";-DUSE109")
																		   _T(";-DUSEdefault"));
				reg.write(_T(".nodoka2"), loadString(IDS_104on109Emacs) + tmp + _T(";-DUSE109")
																				_T(";-DUSEdefault")
																				_T(";-DUSE104on109"));
				reg.write(_T(".nodoka3"), loadString(IDS_109) + tmp + _T(";-DUSE109"));
				reg.write(_T(".nodoka4"), loadString(IDS_104on109) + tmp + _T(";-DUSE109")
																		   _T(";-DUSE104on109"));
			}
			else
			{
				reg.write(_T(".nodoka1"), loadString(IDS_104Emacs) + tmp + _T(";-DUSE104")
																		   _T(";-DUSEdefault"));
				reg.write(_T(".nodoka2"), loadString(IDS_109on104Emacs) + tmp + _T(";-DUSE104")
																				_T(";-DUSEdefault")
																				_T(";-DUSE109on104"));
				reg.write(_T(".nodoka3"), loadString(IDS_104) + tmp + _T(";-DUSE104"));
				reg.write(_T(".nodoka4"), loadString(IDS_109on104) + tmp + _T(";-DUSE104")
																		   _T(";-DUSE109on104"));
			}
			reg.write(_T(".nodokaIndex"), index);
		}
	}
}

void RemoveOldRegistry()
{
	Registry reg(NODOKA_REGISTRY_ROOT2);
	bool doesExit = false;

	doesExit = reg.doesExist();

	if (doesExit)
	{
		reg.remove(_T("m_doesNotifyCommand"));
		reg.remove(_T("m_correctKanaLockHandling"));
		reg.remove(_T("m_CaretBlinkTime"));
		reg.remove(_T("m_BlinkTimeOn"));
		reg.remove(_T("m_BlinkTimeOff"));
		reg.remove(_T("m_syncKey"));
		reg.remove(_T("m_syncKeyIsExtended"));
		Registry::remove(NODOKA_REGISTRY_ROOT2);
		Registry::remove(NODOKA_REGISTRY_ROOT3);
	}
}

#ifdef _WIN64
// nodoka x86 dll load, hook
void run_nodoka_x86()
{
	SHELLEXECUTEINFO shExecInfo;

	shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);

	shExecInfo.fMask = SEE_MASK_FLAG_NO_UI;
	shExecInfo.hwnd = NULL;
	shExecInfo.lpVerb = L"open";
	shExecInfo.lpFile = L"nodoka_helper.exe";
	shExecInfo.lpParameters = NULL;
	shExecInfo.lpDirectory = NULL;
	shExecInfo.nShow = SW_HIDE;
	shExecInfo.hInstApp = NULL;

	ShellExecuteEx(&shExecInfo);
}

void exit_nodoka_x86()
{
	HWND hWnd = FindWindow(L"nodoka_helper", NULL);
	SendMessage(hWnd, WM_CLOSE, 0, 0);
}
#endif

// ChangeWindowMessageFilter helper
void myChangeWindowMessageFilter(HWND m_hwndTaskTray, UINT message, DWORD flag)
{
	HMODULE dll = LoadLibrary(TEXT("user32.dll"));
	static FUNCTYPE ChangeWindowMessageFilter = (FUNCTYPE)GetProcAddress(dll, "ChangeWindowMessageFilter");
	static FUNCTYPE7 ChangeWindowMessageFilterEx = (FUNCTYPE7)GetProcAddress(dll, "ChangeWindowMessageFilterEx");

	DWORD flag7 = MSGFLT_RESET;

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
		ChangeWindowMessageFilterEx(m_hwndTaskTray, message, flag7, 0);
	}
	else if (ChangeWindowMessageFilter != NULL) // Vista
	{
		ChangeWindowMessageFilter(message, flag);
	}
}

/// SetChangeWindowMessageFilter()
void SetChangeWindowMessageFilter(HWND m_hwndTaskTray)
{
	UINT WM_NODOKA_MESSAGE = RegisterWindowMessage(addSessionId(WM_NODOKA_MESSAGE_NAME).c_str());

	if (m_hwndTaskTray != NULL)
	{
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_APP + 101, MSGFLT_ADD); // WM_APP_taskTrayNotify
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_APP + 102, MSGFLT_ADD); // WM_APP_msgStreamNotify
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_APP + 103, MSGFLT_ADD); // WM_APP_notifyFocus
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_APP + 104, MSGFLT_ADD); // WM_APP_notifyVKey
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_APP + 105, MSGFLT_ADD); // WM_APP_targetNotify
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_APP + 110, MSGFLT_ADD); // WM_APP_engineNotify
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_APP + 115, MSGFLT_ADD); // WM_APP_dlglogNotify
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_APP + 116, MSGFLT_ADD); // WM_APP_SendKey
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_APP + 120, MSGFLT_ADD); // WM_APP_NotifyThreadDetach
		//ChangeWindowMessageFilter(m_hwndTaskTray, WM_APP + 121, MSGFLT_ADD);	// WM_APP_NotifySync	not use.
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_APP + 122, MSGFLT_ADD);			  // WM_APP_NotifyLockState
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_APP + 123, MSGFLT_ADD);			  // WM_APP_NotifyTSF
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_APP + 124, MSGFLT_ADD);			  // WM_APP_escapeNLSKeysFailed
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_APP + 201, MSGFLT_ADD);			  // for Touchpad
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_APP + 202, MSGFLT_ADD);			  // for gamepad
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_APP + 203, MSGFLT_ADD);			  // for mouse
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_NODOKA_MESSAGE, MSGFLT_ADD);		  // for Touchpad
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_CREATE, MSGFLT_ADD);				  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_DESTROY, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_MOVE, MSGFLT_ADD);				  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_SIZE, MSGFLT_ADD);				  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_ACTIVATE, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_SETFOCUS, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_KILLFOCUS, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_PAINT, MSGFLT_ADD);				  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_CLOSE, MSGFLT_ADD);				  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_QUERYENDSESSION, MSGFLT_ADD);	  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_ACTIVATEAPP, MSGFLT_ADD);		  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_MOUSEACTIVATE, MSGFLT_ADD);		  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_COPYDATA, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_NOTIFY, MSGFLT_ADD);				  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_SETICON, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_NCDESTROY, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_NCHITTEST, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_NCACTIVATE, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_GETDLGCODE, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_INPUT, MSGFLT_ADD);				  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_INPUT_DEVICE_CHANGE, MSGFLT_ADD);  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_KEYDOWN, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_KEYUP, MSGFLT_ADD);				  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_CHAR, MSGFLT_ADD);				  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_DEADCHAR, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_SYSKEYDOWN, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_SYSKEYUP, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_IME_STARTCOMPOSITION, MSGFLT_ADD); //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_IME_ENDCOMPOSITION, MSGFLT_ADD);   //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_INITDIALOG, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_COMMAND, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_SYSCOMMAND, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_TIMER, MSGFLT_ADD);				  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_MOUSEMOVE, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_LBUTTONDOWN, MSGFLT_ADD);		  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_LBUTTONUP, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_ENTERMENULOOP, MSGFLT_ADD);		  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_EXITMENULOOP, MSGFLT_ADD);		  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_SIZING, MSGFLT_ADD);				  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_IME_NOTIFY, MSGFLT_ADD);			  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_WTSSESSION_CHANGE, MSGFLT_ADD);	//
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_LBUTTONDBLCLK, MSGFLT_ADD);		  //
		myChangeWindowMessageFilter(m_hwndTaskTray, WM_MBUTTONDOWN, MSGFLT_ADD);		  //
	}
}

// 整合性レベルを設定する
bool SetIntgritylevel(int i_intGritylevel)
{
	HANDLE hToken;
	TOKEN_MANDATORY_LABEL mandatoryLabel;
	PSID pSid;
	DWORD dwSidSize;
	HMODULE dll = LoadLibrary(TEXT("user32.dll"));
	FUNCTYPE5 CreateWellKnownSid = (FUNCTYPE5)GetProcAddress(LoadLibrary(TEXT("advapi32.dll")), "CreateWellKnownSid");

	if (CreateWellKnownSid == NULL)
		return FALSE;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_DEFAULT, &hToken))
	{
		MessageBox(NULL, TEXT("-g 引数による整合性レベル設定に失敗しました。トークンハンドルが取得できません。"), TEXT("nodoka"), MB_ICONWARNING);
		return FALSE;
	}

	dwSidSize = SECURITY_MAX_SID_SIZE;
	pSid = (PSID)LocalAlloc(LPTR, dwSidSize);

	switch (i_intGritylevel)
	{
	case 0:
		CreateWellKnownSid(WinLowLabelSid, NULL, pSid, &dwSidSize);
		break;
	case 2:
		CreateWellKnownSid(WinHighLabelSid, NULL, pSid, &dwSidSize);
		break;
	default:
		CreateWellKnownSid(WinMediumLabelSid, NULL, pSid, &dwSidSize);
	}

	mandatoryLabel.Label.Attributes = SE_GROUP_INTEGRITY;
	mandatoryLabel.Label.Sid = pSid;

	if (!SetTokenInformation(hToken, TokenIntegrityLevel, &mandatoryLabel, sizeof(TOKEN_MANDATORY_LABEL) + GetLengthSid(pSid)))
	{
		MessageBox(NULL, TEXT("-g 引数による整合性レベルの設定に失敗しました。"), NULL, MB_ICONWARNING);
		LocalFree(pSid);
		CloseHandle(hToken);
		return FALSE;
	}
	else
	{
		LocalFree(pSid);
		CloseHandle(hToken);
		return TRUE;
	}
}

/// map hook data
bool mapHookData()
{
	DWORD dwDesiredAccess = FILE_MAP_READ | FILE_MAP_WRITE;

	SECURITY_DESCRIPTOR SD;
	SECURITY_ATTRIBUTES SA;
	InitializeSecurityDescriptor(&SD, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(&SD, TRUE, NULL, FALSE);
	SA.nLength = sizeof(SECURITY_ATTRIBUTES);
	SA.bInheritHandle = TRUE;
	SA.lpSecurityDescriptor = &SD;

	m_hHookDataExe = CreateFileMapping(INVALID_HANDLE_VALUE, &SA, PAGE_READWRITE, 0, sizeof(HookData), addSessionId(HOOK_DATA_NAME).c_str());

	if (m_hHookDataExe == NULL)
		return false;

	g_hookDataExe = (HookData *)MapViewOfFile(m_hHookDataExe, dwDesiredAccess, 0, 0, sizeof(HookData));
	if (g_hookDataExe == NULL)
	{
		unmapHookData();
		return false;
	}

	return true;
}

/// unmap hook data
void unmapHookData()
{
	if (g_hookDataExe != NULL)
		if (!UnmapViewOfFile(g_hookDataExe))
			return;
	g_hookDataExe = NULL;
	if (m_hHookDataExe != NULL)
		CloseHandle(m_hHookDataExe);
	m_hHookDataExe = NULL;
}

/// main
int WINAPI _tWinMain(HINSTANCE i_hInstance,
					 HINSTANCE i_hPrevInstance,
					 LPTSTR i_lpszCmdLine,
					 int i_nCmdShow)
{
	/// using
	namespace po = boost::program_options;

	g_hInst = i_hInstance;

	// set locale
	CHECK_TRUE(_tsetlocale(LC_ALL, _T("")));

	// 引数処理
	int argc = 0;
	LPWSTR *argv;

	int icon_color = 0;
	int keyboard_hook = 0;
	int mouse_hook = 0;
	int iPause = 0;
	int iRclick = 0;
	int iKey = 0;
	int iMakeCode = 0;
	int iLog = 0;
	int iDLog = 0;
	int iYield = 0;
	int i_escapeNlsKeys = 0;
	int i_win8wa = 0;
	int i_intGritylevel = 1;
	int i_forceDriver = 0;
	int i_forceRun = 0;
	bool m_RDP = false;

	std::string strDefine;
	std::string strKey;

	argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	po::options_description desc("option");

	//TCHAR szErr[1000];
	//wsprintfW(szErr, L"%s is start", argv[0]);
	//MessageBox(NULL, szErr, NULL, MB_OK);

	// define arg
	desc.add_options()("color,c", po::value<int>(), "icon color")  // icon color 変更
		("help,h", "show help")									   // help 表示して終了する 未実装
		("keyboard_hook,k", "use Keyboard LL Hook")				   // -k Keyboard LL Hook mode
		("rawinput_hook,i", "use Keyboard RawInput Hook")		   // -i RawInput Hook mode (test)
		("mouse_hook,m", "use Mouse LL Hook")					   // -m Mouse LL Hook mode
		("Define,D", po::value<std::string>(), "Define")		   // シンボル定義 未実装
		("Key,K", po::value<int>(), "Send Key")					   // -K 任意キー入力 bit8:flag bit7-0:MakeCode
		("pause,p", "pause")									   // -p 一時停止のトグル、実行したら終了する
		("rclick,r", "rclick")									   // -r menuの表示。mは使用済なのでr
		("log,l", "log")										   // -l log出力有効化
		("Log,L", "Log")										   // -L log出力有効化 詳細ON
		("yield,y", "yield")									   // -y menu表示抑止
		("nls,n", "nls")										   // -n escape NLS keys実行
		("win8wa,w", "win8wa")									   // -w Windows 8にて-k使用時にWin-X, Alt-Tabをスルーさせる。
		("scancodemap,s", po::value<std::string>(), "scancodemap") // -s 任意のscancodemap regファイルで設定
		("quit,q", "quit")										   // -q 引数の処理をしたあと終了する
		("intGritylevel,g", po::value<int>(), "IntegrityLevel")	// -g 0,1,2 as low, midium, high
		("forceDriver,f", "forceDriver")						   // -f RDPだと自動的に-kにしているのを止める
		("forceRun,b", "forceRun")								   // -b 二重起動チェックをやらない
		;

	//	po::positional_options_description pos;
	//	pos.add("color", -1);

	// analize command line
	po::variables_map argmap;
	try
	{
		po::store(po::parse_command_line(argc, argv, desc), argmap);
		po::notify(argmap);

		// set arg to variable
		if (argmap.count("Define"))
			strDefine = argmap["Define"].as<std::string>().c_str();

		if (argmap.count("Key"))
		{
			int iKey = argmap["Key"].as<int>();
			if (iKey > 0 && iKey < 512)
				iMakeCode = iKey;
		}

		if (!argmap.count("color"))
		{
			icon_color = 8; // もし color指定がなければ 8
		}
		else
		{
			icon_color = argmap["color"].as<int>(); // あるなら引数をicon color番号として使う。
			if (icon_color < 0 || icon_color > 7)
				icon_color = 0; // しかし値域を外していたら0
		}

		if (!argmap.count("keyboard_hook") && !argmap.count("rawinput_hook"))
		{
			keyboard_hook = 0; // もし keyboard_hook/rawinput_hook指定がなければ 0
		}

		if (argmap.count("keyboard_hook"))
		{
			keyboard_hook = 1; //
		}

		if (argmap.count("rawinput_hook"))
		{
			keyboard_hook = 2; //
		}

		if (!argmap.count("mouse_hook"))
		{
			mouse_hook = 0; // もし mouse_hook指定がなければ 0
		}
		else
		{
			mouse_hook = 1; // 引数が0以外は1
		}

		if (argmap.count("help"))
		{
			//std::cout << opt << std::endl;
		}

		if (argmap.count("pause"))
		{
			iPause = 1;
		}

		if (argmap.count("rclick"))
		{
			iRclick = 1;
		}

		if (argmap.count("log"))
		{
			iLog = 1;
		}

		if (argmap.count("Log"))
		{
			iLog = 1;
			iDLog = 1;
		}

		if (argmap.count("yield"))
		{
			iYield = 1;
		}

		if (argmap.count("nls"))
		{
			i_escapeNlsKeys = 1;
		}

		if (argmap.count("win8wa"))
		{
			i_win8wa = 1;
		}

		if (!argmap.count("intGritylevel"))
		{
			i_intGritylevel = 1; // もし g指定がなければ 1
		}
		else
		{
			i_intGritylevel = argmap["intGritylevel"].as<int>(); // あるなら引数を取得
			if (i_intGritylevel < 0 || i_intGritylevel > 2)
				i_intGritylevel = 1; // しかし値域を外していたら1
		}

		if (argmap.count("forceDriver"))
		{
			i_forceDriver = 1;
			keyboard_hook = 0;
		}

		if (argmap.count("forceRun"))
		{
			i_forceRun = 1;
		}
	}
	catch (std::exception &e)
	{
		std::cout << e.what() << "\n";
	}

	// common controls
	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_LISTVIEW_CLASSES;
	CHECK_TRUE(InitCommonControlsEx(&icc));

	// convert old registry to new registry
	convertRegistry();
	RemoveOldRegistry();

	// is another nodoka running ?
	HANDLE mutex = CreateMutex(
		(SECURITY_ATTRIBUTES *)NULL, TRUE,
		addSessionId(MUTEX_NODOKA_EXCLUSIVE_RUNNING).c_str());
	if (GetLastError() == ERROR_ALREADY_EXISTS && i_forceRun == 0)
	{
		// another nodoka already running

		HWND tmp_hWnd = FindWindow(L"nodokaTasktray", NULL);

		// 2重起動だったら、引数の一部を実行して、最後に設定を出す。
		if (tmp_hWnd)
		{
			UINT WM_TaskbarRestart = RegisterWindowMessage(_T("TaskbarCreated"));
			PostMessage(tmp_hWnd, WM_TaskbarRestart, icon_color, 0);

			// 引数 -p が指定されたら、一時停止をトグルさせる。
			if (iPause == 1)
			{
				PostMessage(tmp_hWnd, WM_COMMAND, MAKELONG(ID_MENUITEM_disable, 0), 0);
			}

			// 引数 -r だったら、アイコンを右クリックしたことにして、メニューを出す。
			if (iRclick == 1)
			{
				PostMessage(tmp_hWnd, WM_APP + 101 /*WM_APP_taskTrayNotify*/, MAKELONG(1 /*ID_TaskTrayIcon*/, 0), MAKELONG(WM_RBUTTONUP, 0));
			}

			// 引数 -K だったら、flagとMakeCodeを送って、キー入力をシミュレートする。
			if (iKey != 0)
			{
				PostMessage(tmp_hWnd, WM_APP + 116 /*WM_APP_SendKey*/, iMakeCode, 0);
			}

			// 引数 -l が指定されたら、ログ出力をトグルさせる。
			if (iLog == 1)
			{
				PostMessage(tmp_hWnd, WM_APP + 101 /*WM_APP_taskTrayNotify*/, MAKELONG(1 /*ID_TaskTrayIcon*/, 0), MAKELONG(WM_LBUTTONDOWN, 0));
			}

			// 設定メニューを出す。ID_MENUITEM_setting
			if (iYield == 0)
			{
				PostMessage(tmp_hWnd, WM_COMMAND, MAKELONG(ID_MENUITEM_setting, 0), 0);
			}
		}
		return 1;
	}
	else
	{
		if (icon_color == 8)
			icon_color = 0; // 2重起動でないときに、8だったらデフォルトカラーに戻す。
	}

	// check remote desktop
	DWORD sessionId;
	if (!ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) || wtsGetActiveConsoleSessionId() != sessionId)
	{
#if 1
		// RDPだったら、自動的にLL HOOKにする。
		// ただし -f が付いていた場合には、何もしない。
		if (i_forceDriver != 1)
		{
			keyboard_hook = 1;
			m_RDP = true;
		}
#else
		tstring text = loadString(IDS_executedInRemoteDesktop);
		tstring title = loadString(IDS_nodoka);
		MessageBox((HWND)NULL, text.c_str(), title.c_str(), MB_OK | MB_ICONSTOP);
		return 1;
#endif
	}

	// 整合性レベルを変更する。
	if (i_intGritylevel == 0 || i_intGritylevel == 2)
	{
		SetIntgritylevel(i_intGritylevel);
	}

	if (!mapHookData())
		MessageBox((HWND)NULL, L"Can not mapHookData!", L"Nodoka", MB_OK | MB_ICONSTOP);

	// set RDP flag
	g_hookDataExe->m_RDP = m_RDP;

	try
	{
		Nodoka(icon_color, keyboard_hook, mouse_hook, iPause, iLog, iDLog, i_escapeNlsKeys, i_win8wa).messageLoop();
	}
	catch (ErrorMessage &i_e)
	{
		tstring title = loadString(IDS_nodoka);
		MessageBox((HWND)NULL, i_e.getMessage().c_str(), title.c_str(), MB_OK | MB_ICONSTOP);
	}

	DWORD_PTR dwResult;
	SendMessageTimeout(HWND_BROADCAST, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 5000, &dwResult);

	unmapHookData();

	CHECK_TRUE(CloseHandle(mutex));
	return 0;
}
