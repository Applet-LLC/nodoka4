#include <windows.h>
#include <process.h>
#include <tchar.h>
#include "..\nodoka\driver.h"
//#include "../registry.h"

#ifdef STS4NODOKA
#include "SynKit.h"
#pragma comment(lib, "SynCom.lib")
#endif /* STS4NODOKA */

#ifdef CTS4NODOKA
#include "Touchpad.h"
#pragma comment(lib, "TouchPad.lib")
#endif /* CTS4NODOKA */

static HANDLE s_instance;
static UINT s_engineThreadId;

#ifdef STS4NODOKA
static ISynAPI *s_synAPI;
static ISynDevice *s_synDevice;
static HANDLE s_notifyEvent;

static int s_terminated;
static HANDLE s_loopThread;
static unsigned int s_loopThreadId;
#endif /* STS4NODOKA */

#ifdef CTS4NODOKA
static HTOUCHPAD s_hTP[16];
static int s_devNum;
#endif /* CTS4NODOKA */

#ifdef ATS4NODOKA
typedef BOOL(__stdcall *FUNCTYPE)(HWND, UINT, UINT, UINT, UINT);
HWND i_hwnd;
static HMODULE dll = NULL;
static HANDLE s_notifyEvent;
static int s_terminated;
static HANDLE s_loopThread;
static unsigned int s_loopThreadId;
#endif /* ATS4NODOKA */

static void changeTouch(int i_isBreak, LPARAM lParam)
{
	static int pBreak = 0;
	static LPARAM plParam = 0;

	lParam = lParam & 0xffff;

	if (pBreak != i_isBreak || plParam != lParam)
	{
		pBreak = i_isBreak;
		plParam = lParam;
		PostThreadMessage(s_engineThreadId, WM_APP + 201, i_isBreak ? 0 : 1, lParam);
	}
}

static void postEvent(WPARAM wParam, LPARAM lParam)
{
	PostThreadMessage(s_engineThreadId, WM_APP + 201, wParam, lParam);
}

static bool reg_read(int *o_value)
{
	HKEY hkey;
	int i_defaultValue = 3200;
	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\appletkan\\nodoka"), 0, KEY_READ, &hkey))
	{
		DWORD type = REG_DWORD;
		DWORD size = sizeof(*o_value);
		LONG r = RegQueryValueEx(hkey, _T("CenterVal"), NULL, &type, (BYTE *)o_value, &size);
		RegCloseKey(hkey);
		if (r == ERROR_SUCCESS)
			return true;
	}
	*o_value = i_defaultValue;
	return false;
}

#ifdef STS4NODOKA
static unsigned int WINAPI loop(void *dummy)
{
	HRESULT result;
	SynPacket packet;
	int isTouched = 0;
	LPARAM lParam;
	long pX = 0, pY = 0;
	long cX = 0, cY = 0;
	int LtoRed = 0;
	int RtoLed = 0;

	int m_CenterVal;
	reg_read(&m_CenterVal);

	while (s_terminated == 0)
	{
		WaitForSingleObject(s_notifyEvent, INFINITE);
		if (s_terminated)
		{
			break;
		}

		for (;;)
		{
			long value;

			result = s_synAPI->GetEventParameter(&value);
			if (result != SYN_OK)
			{
				break;
			}
			if (value == SE_Configuration_Changed)
			{
				s_synDevice->SetEventNotification(s_notifyEvent);
			}
		}

		for (;;)
		{
			result = s_synDevice->LoadPacket(packet);
			if (result == SYNE_FAIL)
			{
				break;
			}
			cX = packet.X();
			cY = packet.Y();

			if ((packet.FingerState() & SF_FingerTouch))
			{
				isTouched = 0;
			}
			else
			{
				isTouched = 1;
			}

			if (isTouched)
			{
				lParam = cY << 16 | cX;
				changeTouch(1, lParam);

				if (pX < m_CenterVal)
				{
					if ((cX >= m_CenterVal) && (LtoRed == 0)) // L -> R
					{
						changeTouch(1, pY << 16 | pX); // L up
						changeTouch(0, cY << 16 | cX); // R down
						RtoLed = 0;
						LtoRed = 1;
					}
				}
				else
				{
					if ((cX < m_CenterVal) & (RtoLed == 0)) // L <- R
					{
						changeTouch(1, pY << 16 | pX); // R up
						changeTouch(0, cY << 16 | cX); // L down
						RtoLed = 1;
						LtoRed = 0;
					}
				}
			}
			else
			{
				lParam = cY << 16 | cX;
				changeTouch(0, lParam);
				pX = cX;
				pY = cY;
				LtoRed = 0;
				RtoLed = 0;
			}
		}
	}
	_endthreadex(0);
	return 0;
}
#endif /* STS4NODOKA */
#ifdef CTS4NODOKA
static void CALLBACK TouchpadFunc(HTOUCHPAD hTP, LPFEEDHDR lpFeedHdr, LPARAM i_lParam)
{
	LPRAWFEED lpRawFeed;
	static int isTouched = 0;
	static WPARAM s_wParam;
	static LPARAM s_lParam;
	WPARAM wParam;
	LPARAM lParam;

	lpRawFeed = (LPRAWFEED)(lpFeedHdr + 1);
#if 1
	wParam = lpRawFeed->wPressure;
	lParam = lpRawFeed->x << 16 | lpRawFeed->y;
	if (wParam != s_wParam || lParam != s_lParam)
	{
		postEvent(wParam, lParam);
		s_wParam = wParam;
		s_lParam = lParam;
	}
#else
	if (isTouched)
	{
		if (!lpRawFeed->wPressure)
		{
			changeTouch(1);
			isTouched = 0;
		}
	}
	else
	{
		if (lpRawFeed->wPressure)
		{
			changeTouch(0);
			isTouched = 1;
		}
	}
#endif
	EnableWindowsCursor(hTP, TRUE);
}

static BOOL CALLBACK DevicesFunc(LPGENERICDEVICE device, LPARAM lParam)
{
	HTOUCHPAD hTP = NULL;
	BOOL ret = FALSE;

	s_hTP[s_devNum] = GetPad(device->devicePort);
	CreateCallback(s_hTP[s_devNum], TouchpadFunc,
				   TPF_RAW | TPF_POSTMESSAGE, NULL);
	StartFeed(s_hTP[s_devNum]);
	++s_devNum;
	return TRUE;
}
#endif /* CTS4NODOKA */

bool WINAPI ts4nodokaInit(UINT i_engineThreadId)
{
	s_engineThreadId = i_engineThreadId;

#ifdef STS4NODOKA
	HRESULT result;
	long hdl;

	s_synAPI = NULL;
	s_synDevice = NULL;
	s_notifyEvent = NULL;

	s_terminated = 0;

	result = SynCreateAPI(&s_synAPI);
	if (result != SYN_OK)
	{
		goto error_on_init;
	}

	hdl = -1;
	result = s_synAPI->FindDevice(SE_ConnectionAny, SE_DeviceTouchPad, &hdl);
	if (result != SYN_OK)
	{
		goto error_on_init;
	}

	result = s_synAPI->CreateDevice(hdl, &s_synDevice);
	if (result != SYN_OK)
	{
		goto error_on_init;
	}

	s_notifyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (s_notifyEvent == NULL)
	{
		goto error_on_init;
	}

	s_synAPI->SetEventNotification(s_notifyEvent);
	s_synDevice->SetEventNotification(s_notifyEvent);

	s_loopThread =
		(HANDLE)_beginthreadex(NULL, 0, loop, NULL, 0, &s_loopThreadId);
	if (s_loopThread == 0)
	{
		goto error_on_init;
	}

	return true;

error_on_init:
	if (s_notifyEvent)
	{
		CloseHandle(s_notifyEvent);
	}

	if (s_synDevice)
	{
		s_synDevice->Release();
	}

	if (s_synAPI)
	{
		s_synAPI->Release();
	}

	return false;
#endif /* STS4NODOKA */
#ifdef CTS4NODOKA
	// enumerate devices
	EnumDevices(DevicesFunc, NULL);
	return true;
#endif /* CTS4NODOKA */
#ifdef ATS4NODOKA
	// Vxdif.dll init window message
	dll = LoadLibrary(_T("Vxdif.dll"));
	if (dll == NULL)
		return false;

	i_hwnd = FindWindow(_T("nodokaTasktray"), NULL);

	if (dll != NULL)
	{
		FUNCTYPE IOCTL_SetMouseMonitor = (FUNCTYPE)GetProcAddress(dll, "IOCTL_SetMouseMonitor");
		if (IOCTL_SetMouseMonitor != NULL)
			IOCTL_SetMouseMonitor(i_hwnd, 1, 0, 0x0BCD, NULL);
	}
	// loop start
	/*
	s_notifyEvent = NULL;
	s_terminated = 0;
	s_notifyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	s_loopThread = (HANDLE)_beginthreadex(NULL, 0, loop, NULL, 0, &s_loopThreadId);
	SetEvent(s_notifyEvent);
	*/

	return true;
#endif /* ATS4NODOKA */
}

bool WINAPI ts4nodokaTerm()
{
#ifdef STS4NODOKA
	s_terminated = 1;

	if (s_loopThread)
	{
		SetEvent(s_notifyEvent);
		WaitForSingleObject(s_loopThread, INFINITE);
		CloseHandle(s_loopThread);
	}

	if (s_notifyEvent)
	{
		CloseHandle(s_notifyEvent);
	}

	if (s_synDevice)
	{
		s_synDevice->Release();
	}

	if (s_synAPI)
	{
		s_synAPI->Release();
	}

	return true; // dll unload
#endif			 /* STS4NODOKA */
#ifdef CTS4NODOKA
	for (int i = 0; i < s_devNum; i++)
	{
		StopFeed(s_hTP[i]);
	}
	return false; // dll not unload
#endif			  /* CTS4NODOKA */
#ifdef ATS4NODOKA
	/*
	s_terminated = 1;

	if (s_loopThread) {
		SetEvent(s_notifyEvent);
		WaitForSingleObject(s_loopThread, INFINITE);
		CloseHandle(s_loopThread);
	}

	if (s_notifyEvent) {
		CloseHandle(s_notifyEvent);
	}
*/
	if (dll != NULL)
		FreeLibrary(dll);
	return true; // dll unload
#endif			 /* ATS4NODOKA */
}

BOOL WINAPI DllMain(HANDLE module, DWORD reason, LPVOID reserve)
{
	s_instance = (HINSTANCE)module;
	return TRUE;
}
