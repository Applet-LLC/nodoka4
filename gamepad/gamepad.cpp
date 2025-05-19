#define STRICT
#define DIRECTINPUT_VERSION 0x0800
#define _CRT_SECURE_NO_DEPRECATE
#ifndef _WIN32_DCOM
#define _WIN32_DCOM
#endif

//#define LOG
#define MAX_GPBUTTON 16
#define BASEKEY 0x20

#include <windows.h>
#include <commdlg.h>
#include <XInput.h> // XInput API
#include <basetsd.h>
#include <process.h>
#include <tchar.h>

#include "..\nodoka\driver.h"

#include <dinput.h>
#include <dinputd.h>
#include <assert.h>
#include <oleauto.h>
#include <shellapi.h>

#pragma warning(disable : 4996) // disable deprecated warning
#include <strsafe.h>
#pragma warning(default : 4996)

static HANDLE s_instance;
static UINT s_engineThreadId;

HWND hDlg;
static HMODULE dll = NULL;
static HANDLE s_notifyEvent;
static int s_terminated;
static HANDLE s_loopThread;
static unsigned int s_loopThreadId;
static BOOL bGamePadReady = FALSE;
static BOOL bPad[12] = {FALSE};
static BOOL blX1 = FALSE;
static BOOL blX2 = FALSE;
static BOOL blY1 = FALSE;
static BOOL blY2 = FALSE;
static BOOL blZ1 = FALSE;
static BOOL blZ2 = FALSE;
static BOOL blRx1 = FALSE;
static BOOL blRx2 = FALSE;
static BOOL blRy1 = FALSE;
static BOOL blRy2 = FALSE;
static BOOL blRz1 = FALSE;
static BOOL blRz2 = FALSE;
static BOOL bSlider0 = FALSE;
static BOOL bSlider1 = FALSE;
static BOOL bUP = FALSE;
static BOOL bUP_RIGHT = FALSE;
static BOOL bRIGHT = FALSE;
static BOOL bDOWN_RIGHT = FALSE;
static BOOL bDOWN = FALSE;
static BOOL bDOWN_LEFT = FALSE;
static BOOL bLEFT = FALSE;
static BOOL bUP_LEFT = FALSE;
static int bRepPad[22] = {0};

static BOOL bButton[MAX_GPBUTTON] = {FALSE};
static int bRepeat[MAX_GPBUTTON] = {0};
HWND i_hwnd;

// パラメータ
static volatile int maxVALUE = 10000;			  // 最大値
static volatile int thVALUE = 5000;				  // 閾値
static volatile int deadzoneVALUE = 2500;		  // デッドゾーンの範囲
static volatile int REPEAT_TIMES_1 = 20;		  // キー入力後リピートするまでの間隔
static volatile int REPEAT_TIMES_2 = 10;		  // 2個目以降のリピート間隔
static volatile int WAIT = 10;					  // ms
static volatile unsigned int FLAGPAD = 0xffff;	// Repeat flag for PAD
static volatile unsigned int FLAGHAT = 0xffff;	// Repeat flag for HAT
static volatile unsigned int FLAGBUTTON = 0xffff; // Repeat flag for BUTTON

//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
//INT_PTR CALLBACK MainDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam );
BOOL CALLBACK EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE *pdidoi, VOID *pContext);
BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE *pdidInstance, VOID *pContext);
HRESULT InitDirectInput(HWND hDlg);
VOID FreeDirectInput();
HRESULT UpdateInputState(HWND hDlg);

// Stuff to filter out XInput devices
#include <wbemidl.h>
HRESULT SetupForIsXInputDevice();
bool IsXInputDevice(const GUID *pGuidProductFromDirectInput);
void CleanupForIsXInputDevice();

struct XINPUT_DEVICE_NODE
{
	DWORD dwVidPid;
	XINPUT_DEVICE_NODE *pNext;
};

struct DI_ENUM_CONTEXT
{
	DIJOYCONFIG *pPreferredJoyCfg;
	bool bPreferredJoyCfgValid;
};

bool g_bFilterOutXinputDevices = false;
XINPUT_DEVICE_NODE *g_pXInputDeviceList = NULL;

//-----------------------------------------------------------------------------
// Defines, constants, and global variables
//-----------------------------------------------------------------------------
#define SAFE_DELETE(p)  \
	{                   \
		if (p)          \
		{               \
			delete (p); \
			(p) = NULL; \
		}               \
	}
#define SAFE_RELEASE(p)     \
	{                       \
		if (p)              \
		{                   \
			(p)->Release(); \
			(p) = NULL;     \
		}                   \
	}

LPDIRECTINPUT8 g_pDI = NULL;
LPDIRECTINPUTDEVICE8 g_pJoystick = NULL;

static void changeEvent(int i_up, LPARAM lParam)
{
#ifdef LOG
	TCHAR buffer[1024];
	swprintf_s(buffer, sizeof(buffer), L"changeEvent : %x : %x\n", i_up, lParam);
	OutputDebugString(buffer);
#endif
	PostThreadMessage(s_engineThreadId, WM_APP + 202, i_up ? 0 : 1, BASEKEY + lParam);
}

static bool reg_read(int *o_value, int i_defaultValue, LPCWSTR szName)
{
	HKEY hkey;

	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\appletkan\\nodoka"), 0, KEY_READ, &hkey))
	{
		DWORD type = REG_DWORD;
		DWORD size = sizeof(*o_value);
		LONG r = RegQueryValueEx(hkey, szName, NULL, &type, (BYTE *)o_value, &size);
		RegCloseKey(hkey);
		if (r == ERROR_SUCCESS)
			return true;
	}
	*o_value = i_defaultValue;
	return false;
}

static unsigned int WINAPI loop(void *dummy)
{
	TCHAR buffer[1024];

	if (hDlg = FindWindow(_T("nodokaTasktray"), NULL))
		while (s_terminated == 0)
		{
			if (!bGamePadReady)
				if (SUCCEEDED(InitDirectInput(hDlg)))
				{
#ifdef LOG
					OutputDebugString(L"find gamepad");
#endif
					bGamePadReady = TRUE;
					if (g_pJoystick)
						g_pJoystick->Acquire();

					// すべての入力フラグをクリア
					blX1 = FALSE;
					blX2 = FALSE;
					blY1 = FALSE;
					blY2 = FALSE;
					blZ1 = FALSE;
					blZ2 = FALSE;
					blRx1 = FALSE;
					blRx2 = FALSE;
					blRy1 = FALSE;
					blRy2 = FALSE;
					blRz1 = FALSE;
					blRz2 = FALSE;
					bSlider0 = FALSE;
					bSlider1 = FALSE;
					bUP = FALSE;
					bUP_RIGHT = FALSE;
					bRIGHT = FALSE;
					bDOWN_RIGHT = FALSE;
					bDOWN = FALSE;
					bDOWN_LEFT = FALSE;
					bLEFT = FALSE;
					bUP_LEFT = FALSE;
					for (int i = 0; i < MAX_GPBUTTON; i++)
						bButton[i] = FALSE;
				}
				else
				{
					FreeDirectInput(); // InitDirectInput()失敗したので、FreeDirectInput()を呼ぶ。これが無いと、毎秒200KBずつぐらいメモリを消費する。
					continue;
				}
			//#ifdef LOG
			swprintf_s(buffer, sizeof(buffer), L"gamepad parm : %d %d %d %d %d %d\n", maxVALUE, thVALUE, deadzoneVALUE, REPEAT_TIMES_1, REPEAT_TIMES_2, WAIT);
			OutputDebugString(buffer);
			//#endif

			if (FAILED(UpdateInputState(hDlg)))
			{
#ifdef LOG
				OutputDebugString(L"lost gamepad");
#endif
				bGamePadReady = FALSE;
				FreeDirectInput();
#if 0 // 現状、ゲームパッドがロストすると、InitDirectInput()のpJoyConfig->GetConfigで、gamepad.dllが落ちてしまうので、loopを抜けて、設定ファイルの手動のReloadが必要。 
					Sleep(5000);
					continue;
#else
				goto loop_exit;
#endif
			}
			Sleep(WAIT);
		}
loop_exit:
	_endthreadex(0);
	return 0;
}

bool WINAPI ts4nodokaInit(UINT i_engineThreadId)
{
	i_hwnd = FindWindow(_T("nodokaTasktray"), NULL);
	s_engineThreadId = i_engineThreadId;

	reg_read((int *)&maxVALUE, 10000, L"m_maxVALUE");
	reg_read((int *)&thVALUE, 5000, L"m_thVALUE");
	reg_read((int *)&deadzoneVALUE, 2500, L"m_deadzoneVALUE");
	reg_read((int *)&REPEAT_TIMES_1, 20, L"m_REPEAT_TIMES_1");
	reg_read((int *)&REPEAT_TIMES_2, 3, L"m_REPEAT_TIMES_2");
	reg_read((int *)&WAIT, 10, L"m_WAIT");
	reg_read((int *)&FLAGPAD, 0xffff, L"m_REPEAT_FLAG_PAD");
	reg_read((int *)&FLAGHAT, 0xffff, L"m_REPEAT_FLAG_HAT");
	reg_read((int *)&FLAGBUTTON, 0xffff, L"m_REPEAT_FLAG_BUTTON");

	s_notifyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (s_notifyEvent == NULL)
	{
		goto error_on_init;
	}

	s_loopThread =
		(HANDLE)_beginthreadex(NULL, 0, loop, NULL, 0, &s_loopThreadId);
	if (s_loopThread == 0)
	{
		goto error_on_init;
	}

	s_terminated = 0;
	SetEvent(s_notifyEvent);

	return true;

error_on_init:
	if (s_notifyEvent)
	{
		CloseHandle(s_notifyEvent);
	}

	return true;
}

bool WINAPI ts4nodokaTerm()
{
	s_terminated = 1;

	if (s_loopThread)
	{
		SetEvent(s_notifyEvent);
		WaitForSingleObject(s_loopThread, INFINITE);
		CloseHandle(s_loopThread);
	}

	if (bGamePadReady)
	{
		FreeDirectInput();
		bGamePadReady = FALSE;
	}

	return true;
}

BOOL WINAPI DllMain(HANDLE module, DWORD reason, LPVOID reserve)
{
	s_instance = (HINSTANCE)module;
	return TRUE;
}

//-----------------------------------------------------------------------------
// Name: InitDirectInput()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
HRESULT InitDirectInput(HWND hDlg)
{
	HRESULT hr;

	// Register with the DirectInput subsystem and get a pointer
	// to a IDirectInput interface we can use.
	// Create a DInput object
	if (FAILED(hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION,
									   IID_IDirectInput8, (VOID **)&g_pDI, NULL)))
		return hr;

	if (g_bFilterOutXinputDevices)
		SetupForIsXInputDevice();

	DIJOYCONFIG PreferredJoyCfg = {0};
	DI_ENUM_CONTEXT enumContext;
	enumContext.pPreferredJoyCfg = &PreferredJoyCfg;
	enumContext.bPreferredJoyCfgValid = false;

	IDirectInputJoyConfig8 *pJoyConfig = NULL;
	if (FAILED(hr = g_pDI->QueryInterface(IID_IDirectInputJoyConfig8, (void **)&pJoyConfig)))
		return hr;

	PreferredJoyCfg.dwSize = sizeof(PreferredJoyCfg);
	if (SUCCEEDED(pJoyConfig->GetConfig(0, &PreferredJoyCfg, DIJC_GUIDINSTANCE))) // This function is expected to fail if no joystick is attached
		enumContext.bPreferredJoyCfgValid = true;
	SAFE_RELEASE(pJoyConfig);

	// Look for a simple joystick we can use for this sample program.
	if (FAILED(hr = g_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL,
									   EnumJoysticksCallback,
									   &enumContext, DIEDFL_ATTACHEDONLY)))
		return hr;

	if (g_bFilterOutXinputDevices)
		CleanupForIsXInputDevice();

	// Make sure we got a joystick
	if (NULL == g_pJoystick)
		return E_FAIL;

	// Set the data format to "simple joystick" - a predefined data format
	//
	// A data format specifies which controls on a device we are interested in,
	// and how they should be reported. This tells DInput that we will be
	// passing a DIJOYSTATE2 structure to IDirectInputDevice::GetDeviceState().
	if (FAILED(hr = g_pJoystick->SetDataFormat(&c_dfDIJoystick2)))
		return hr;

	// Set the cooperative level to let DInput know how this device should
	// interact with the system and with other DInput applications.
	if (FAILED(hr = g_pJoystick->SetCooperativeLevel(hDlg, DISCL_EXCLUSIVE |
															   DISCL_BACKGROUND)))
		return hr;

	// Enumerate the joystick objects. The callback function enabled user
	// interface elements for objects that are found, and sets the min/max
	// values property for discovered axes.
	if (FAILED(hr = g_pJoystick->EnumObjects(EnumObjectsCallback,
											 (VOID *)hDlg, DIDFT_ALL)))
		return hr;

	return S_OK;
}

//-----------------------------------------------------------------------------
// Enum each PNP device using WMI and check each device ID to see if it contains
// "IG_" (ex. "VID_045E&PID_028E&IG_00").  If it does, then it痴 an XInput device
// Unfortunately this information can not be found by just using DirectInput.
// Checking against a VID/PID of 0x028E/0x045E won't find 3rd party or future
// XInput devices.
//
// This function stores the list of xinput devices in a linked list
// at g_pXInputDeviceList, and IsXInputDevice() searchs that linked list
//-----------------------------------------------------------------------------
HRESULT SetupForIsXInputDevice()
{
	IWbemServices *pIWbemServices = NULL;
	IEnumWbemClassObject *pEnumDevices = NULL;
	IWbemLocator *pIWbemLocator = NULL;
	IWbemClassObject *pDevices[20] = {0};
	BSTR bstrDeviceID = NULL;
	BSTR bstrClassName = NULL;
	BSTR bstrNamespace = NULL;
	DWORD uReturned = 0;
	bool bCleanupCOM = false;
	UINT iDevice = 0;
	VARIANT var;
	HRESULT hr;

	// CoInit if needed
	hr = CoInitialize(NULL);
	bCleanupCOM = SUCCEEDED(hr);

	// Create WMI
	hr = CoCreateInstance(__uuidof(WbemLocator),
						  NULL,
						  CLSCTX_INPROC_SERVER,
						  __uuidof(IWbemLocator),
						  (LPVOID *)&pIWbemLocator);
	if (FAILED(hr) || pIWbemLocator == NULL)
		goto LCleanup;

	// Create BSTRs for WMI
	bstrNamespace = SysAllocString(L"\\\\.\\root\\cimv2");
	if (bstrNamespace == NULL)
		goto LCleanup;
	bstrDeviceID = SysAllocString(L"DeviceID");
	if (bstrDeviceID == NULL)
		goto LCleanup;
	bstrClassName = SysAllocString(L"Win32_PNPEntity");
	if (bstrClassName == NULL)
		goto LCleanup;

	// Connect to WMI
	hr = pIWbemLocator->ConnectServer(bstrNamespace, NULL, NULL, 0L,
									  0L, NULL, NULL, &pIWbemServices);
	if (FAILED(hr) || pIWbemServices == NULL)
		goto LCleanup;

	// Switch security level to IMPERSONATE
	CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
					  RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, 0);

	// Get list of Win32_PNPEntity devices
	hr = pIWbemServices->CreateInstanceEnum(bstrClassName, 0, NULL, &pEnumDevices);
	if (FAILED(hr) || pEnumDevices == NULL)
		goto LCleanup;

	// Loop over all devices
	for (;;)
	{
		// Get 20 at a time
		hr = pEnumDevices->Next(10000, 20, pDevices, &uReturned);
		if (FAILED(hr))
			goto LCleanup;
		if (uReturned == 0)
			break;

		for (iDevice = 0; iDevice < uReturned; iDevice++)
		{
			// For each device, get its device ID
			hr = pDevices[iDevice]->Get(bstrDeviceID, 0L, &var, NULL, NULL);
			if (SUCCEEDED(hr) && var.vt == VT_BSTR && var.bstrVal != NULL)
			{
				// Check if the device ID contains "IG_".  If it does, then it痴 an XInput device
				// Unfortunately this information can not be found by just using DirectInput
				if (wcsstr(var.bstrVal, L"IG_"))
				{
					// If it does, then get the VID/PID from var.bstrVal
					DWORD dwPid = 0, dwVid = 0;
					WCHAR *strVid = wcsstr(var.bstrVal, L"VID_");
					if (strVid && swscanf(strVid, L"VID_%4X", &dwVid) != 1)
						dwVid = 0;
					WCHAR *strPid = wcsstr(var.bstrVal, L"PID_");
					if (strPid && swscanf(strPid, L"PID_%4X", &dwPid) != 1)
						dwPid = 0;

					DWORD dwVidPid = MAKELONG(dwVid, dwPid);

					// Add the VID/PID to a linked list
					XINPUT_DEVICE_NODE *pNewNode = new XINPUT_DEVICE_NODE;
					if (pNewNode)
					{
						pNewNode->dwVidPid = dwVidPid;
						pNewNode->pNext = g_pXInputDeviceList;
						g_pXInputDeviceList = pNewNode;
					}
				}
			}
			SAFE_RELEASE(pDevices[iDevice]);
		}
	}

LCleanup:
	if (bstrNamespace)
		SysFreeString(bstrNamespace);
	if (bstrDeviceID)
		SysFreeString(bstrDeviceID);
	if (bstrClassName)
		SysFreeString(bstrClassName);
	for (iDevice = 0; iDevice < 20; iDevice++)
		SAFE_RELEASE(pDevices[iDevice]);
	SAFE_RELEASE(pEnumDevices);
	SAFE_RELEASE(pIWbemLocator);
	SAFE_RELEASE(pIWbemServices);

	return hr;
}

//-----------------------------------------------------------------------------
// Returns true if the DirectInput device is also an XInput device.
// Call SetupForIsXInputDevice() before, and CleanupForIsXInputDevice() after
//-----------------------------------------------------------------------------
bool IsXInputDevice(const GUID *pGuidProductFromDirectInput)
{
	// Check each xinput device to see if this device's vid/pid matches
	XINPUT_DEVICE_NODE *pNode = g_pXInputDeviceList;
	while (pNode)
	{
		if (pNode->dwVidPid == pGuidProductFromDirectInput->Data1)
			return true;
		pNode = pNode->pNext;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Cleanup needed for IsXInputDevice()
//-----------------------------------------------------------------------------
void CleanupForIsXInputDevice()
{
	// Cleanup linked list
	XINPUT_DEVICE_NODE *pNode = g_pXInputDeviceList;
	while (pNode)
	{
		XINPUT_DEVICE_NODE *pDelete = pNode;
		pNode = pNode->pNext;
		SAFE_DELETE(pDelete);
	}
}

//-----------------------------------------------------------------------------
// Name: EnumJoysticksCallback()
// Desc: Called once for each enumerated joystick. If we find one, create a
//       device interface on it so we can play with it.
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE *pdidInstance,
									VOID *pContext)
{
	DI_ENUM_CONTEXT *pEnumContext = (DI_ENUM_CONTEXT *)pContext;
	HRESULT hr;

	if (g_bFilterOutXinputDevices && IsXInputDevice(&pdidInstance->guidProduct))
		return DIENUM_CONTINUE;

	// Skip anything other than the perferred joystick device as defined by the control panel.
	// Instead you could store all the enumerated joysticks and let the user pick.
	if (pEnumContext->bPreferredJoyCfgValid &&
		!IsEqualGUID(pdidInstance->guidInstance, pEnumContext->pPreferredJoyCfg->guidInstance))
		return DIENUM_CONTINUE;

	// Obtain an interface to the enumerated joystick.
	hr = g_pDI->CreateDevice(pdidInstance->guidInstance, &g_pJoystick, NULL);

	// If it failed, then we can't use this joystick. (Maybe the user unplugged
	// it while we were in the middle of enumerating it.)
	if (FAILED(hr))
		return DIENUM_CONTINUE;

	// Stop enumeration. Note: we're just taking the first joystick we get. You
	// could store all the enumerated joysticks and let the user pick.
	return DIENUM_STOP;
}

//-----------------------------------------------------------------------------
// Name: EnumObjectsCallback()
// Desc: Callback function for enumerating objects (axes, buttons, POVs) on a
//       joystick. This function enables user interface elements for objects
//       that are found to exist, and scales axes min/max values.
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE *pdidoi,
								  VOID *pContext)
{
	HWND hDlg = (HWND)pContext;

	static int nSliderCount = 0; // Number of returned slider controls
	static int nPOVCount = 0;	// Number of returned POV controls

	// For axes that are returned, set the DIPROP_RANGE property for the
	// enumerated axis in order to scale min/max values.
	if (pdidoi->dwType & DIDFT_AXIS)
	{
		DIPROPRANGE diprg;
		diprg.diph.dwSize = sizeof(DIPROPRANGE);
		diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		diprg.diph.dwHow = DIPH_BYID;
		diprg.diph.dwObj = pdidoi->dwType; // Specify the enumerated axis
		diprg.lMin = -maxVALUE;
		diprg.lMax = +maxVALUE;

		// Set the range for the axis
		if (FAILED(g_pJoystick->SetProperty(DIPROP_RANGE, &diprg.diph)))
			return DIENUM_STOP;

		DIPROPDWORD dipdw;
		dipdw.diph.dwSize = sizeof(dipdw);
		dipdw.diph.dwHeaderSize = sizeof(dipdw.diph);
		dipdw.diph.dwHow = DIPH_BYID;
		dipdw.diph.dwObj = pdidoi->dwType;
		dipdw.dwData = deadzoneVALUE;
		// Set the dead zone
		if (FAILED(g_pJoystick->SetProperty(DIPROP_DEADZONE, &dipdw.diph)))
			return DIENUM_STOP;
	}

	if (pdidoi->guidType == GUID_XAxis)
		bPad[0] = TRUE;
	if (pdidoi->guidType == GUID_YAxis)
		bPad[1] = TRUE;
	if (pdidoi->guidType == GUID_ZAxis)
		bPad[2] = TRUE;
	if (pdidoi->guidType == GUID_RxAxis)
		bPad[3] = TRUE;
	if (pdidoi->guidType == GUID_RyAxis)
		bPad[4] = TRUE;
	if (pdidoi->guidType == GUID_RzAxis)
		bPad[5] = TRUE;
	if (pdidoi->guidType == GUID_Slider)
	{
		switch (nSliderCount++)
		{
		case 0:
			bPad[6] = TRUE;
			break;

		case 1:
			bPad[7] = TRUE;
			break;
		}
	}
	if (pdidoi->guidType == GUID_POV)
	{
		switch (nPOVCount++)
		{
		case 0:
			bPad[8] = TRUE;
			break;

		case 1:
			bPad[9] = TRUE;
			break;

		case 2:
			bPad[10] = TRUE;
			break;

		case 3:
			bPad[11] = TRUE;
			break;
		}
	}

	return DIENUM_CONTINUE;
}

//-----------------------------------------------------------------------------
// Name: UpdateInputState()
// Desc: Get the input device's state and display it.
//-----------------------------------------------------------------------------
HRESULT UpdateInputState(HWND hDlg)
{
	HRESULT hr;
	TCHAR strText[512] = {0}; // Device state text
	DIJOYSTATE2 js;			  // DInput joystick state

	if (NULL == g_pJoystick)
	{
		return S_OK;
	}

	// Poll the device to read the current state
	hr = g_pJoystick->Poll();
	if (FAILED(hr))
	{
		// DInput is telling us that the input stream has been
		// interrupted. We aren't tracking any state between polls, so
		// we don't have any special reset that needs to be done. We
		// just re-acquire and try again.
		hr = g_pJoystick->Acquire();
		while (hr == DIERR_INPUTLOST)
			hr = g_pJoystick->Acquire();

		// hr may be DIERR_OTHERAPPHASPRIO or other errors.  This
		// may occur when the app is minimized or in the process of
		// switching, so just try again later
		return S_OK;
	}

	// Get the input's device state
	if (FAILED(hr = g_pJoystick->GetDeviceState(sizeof(DIJOYSTATE2), &js)))
		return hr; // The device should have been acquired during the Poll()

		// Send joystick state to Nodoka
#ifdef LOG
	TCHAR buffer[1024];
	swprintf_s(buffer, sizeof(buffer), L"js.lX : %d\n", js.lX);
	OutputDebugString(buffer);
	swprintf_s(buffer, sizeof(buffer), L"js.lY : %d\n", js.lY);
	OutputDebugString(buffer);
	swprintf_s(buffer, sizeof(buffer), L"js.lZ : %d\n", js.lZ);
	OutputDebugString(buffer);
	swprintf_s(buffer, sizeof(buffer), L"js.lRx : %d\n", js.lRx);
	OutputDebugString(buffer);
	swprintf_s(buffer, sizeof(buffer), L"js.lRy : %d\n", js.lRy);
	OutputDebugString(buffer);
	swprintf_s(buffer, sizeof(buffer), L"js.lRz : %d\n", js.lRz);
	OutputDebugString(buffer);
	swprintf_s(buffer, sizeof(buffer), L"js.rglSlider[0] : %d\n", js.rglSlider[0]);
	OutputDebugString(buffer);
	swprintf_s(buffer, sizeof(buffer), L"js.rglSlider[1] : %d\n", js.rglSlider[1]);
	OutputDebugString(buffer);
	swprintf_s(buffer, sizeof(buffer), L"js.rgdwPOV[0] : %d\n", js.rgdwPOV[0]);
	OutputDebugString(buffer);
	for (int i = 0; i < MAX_GPBUTTON; i++)
	{
		swprintf_s(buffer, sizeof(buffer), L"js.rgbButtons[i] : %x\n", js.rgbButtons[i] & 0x80);
		OutputDebugString(buffer);
	}
#endif

	if (bPad[0])
	{
		if (blX1 == FALSE && js.lX >= thVALUE) // down
		{
			blX1 = TRUE;
			bRepPad[0] = REPEAT_TIMES_1; // set repeat time 1
			changeEvent(0, 0);
		}
		else if (blX1 == TRUE && js.lX < thVALUE) // up
		{
			blX1 = FALSE;
			bRepPad[0] = REPEAT_TIMES_1; // reset repeat time 1
			changeEvent(1, 0);
		}
		else if (blX1 == TRUE && js.lX >= thVALUE && (FLAGPAD & 0x0001)) // repeat
		{
			if (bRepPad[0] > 0) // count down
				bRepPad[0]--;
			else // time up
			{
				bRepPad[0] = REPEAT_TIMES_2; // set repeat time 2
				changeEvent(0, 0);
			}
		}

		if (blX2 == FALSE && js.lX <= (thVALUE * (-1)))
		{
			blX2 = TRUE;
			bRepPad[1] = REPEAT_TIMES_1;
			changeEvent(0, 1);
		}
		else if (blX2 == TRUE && js.lX > (thVALUE * (-1)))
		{
			blX2 = FALSE;
			bRepPad[1] = REPEAT_TIMES_1;
			changeEvent(1, 1);
		}
		else if (blX2 == TRUE && js.lX <= (thVALUE * (-1)) && (FLAGPAD & 0x0002))
		{
			if (bRepPad[1] > 0)
				bRepPad[1]--;
			else
			{
				bRepPad[1] = REPEAT_TIMES_2;
				changeEvent(0, 1);
			}
		}
	}

	if (bPad[1])
	{
		if (blY1 == FALSE && js.lY >= thVALUE)
		{
			blY1 = TRUE;
			bRepPad[2] = REPEAT_TIMES_1;
			changeEvent(0, 2);
		}
		else if (blY1 == TRUE && js.lY < thVALUE)
		{
			blY1 = FALSE;
			bRepPad[2] = REPEAT_TIMES_1;
			changeEvent(1, 2);
		}
		else if (blY1 == TRUE && js.lY >= thVALUE && (FLAGPAD & 0x0004))
		{
			if (bRepPad[2] > 0)
				bRepPad[2]--;
			else
			{
				bRepPad[2] = REPEAT_TIMES_2;
				changeEvent(0, 2);
			}
		}
		if (blY2 == FALSE && js.lY <= (thVALUE * (-1)))
		{
			blY2 = TRUE;
			bRepPad[3] = REPEAT_TIMES_1;
			changeEvent(0, 3);
		}
		else if (blY2 == TRUE && js.lY > (thVALUE * (-1)))
		{
			blY2 = FALSE;
			bRepPad[3] = REPEAT_TIMES_1;
			changeEvent(1, 3);
		}
		else if (blY2 == TRUE && js.lY <= (thVALUE * (-1)) && (FLAGPAD & 0x0008))
		{
			if (bRepPad[3] > 0)
				bRepPad[3]--;
			else
			{
				bRepPad[3] = REPEAT_TIMES_2;
				changeEvent(0, 3);
			}
		}
	}

	if (bPad[2])
	{
		if (blZ1 == FALSE && js.lZ >= thVALUE)
		{
			blZ1 = TRUE;
			bRepPad[4] = REPEAT_TIMES_1;
			changeEvent(0, 4);
		}
		else if (blZ1 == TRUE && js.lZ < thVALUE)
		{
			blZ1 = FALSE;
			bRepPad[4] = REPEAT_TIMES_1;
			changeEvent(1, 4);
		}
		else if (blZ1 == TRUE && js.lZ >= thVALUE && (FLAGPAD & 0x0010))
		{
			if (bRepPad[4] > 0)
				bRepPad[4]--;
			else
			{
				bRepPad[4] = REPEAT_TIMES_2;
				changeEvent(0, 4);
			}
		}

		if (blZ2 == FALSE && js.lZ <= (thVALUE * (-1)))
		{
			blZ2 = TRUE;
			bRepPad[5] = REPEAT_TIMES_1;
			changeEvent(0, 5);
		}
		else if (blZ2 == TRUE && js.lZ > (thVALUE * (-1)))
		{
			blZ2 = FALSE;
			bRepPad[5] = REPEAT_TIMES_1;
			changeEvent(1, 5);
		}
		else if (blZ2 == TRUE && js.lZ <= (thVALUE * (-1)) && (FLAGPAD & 0x0020))
		{
			if (bRepPad[5] > 0)
				bRepPad[5]--;
			else
			{
				bRepPad[5] = REPEAT_TIMES_2;
				changeEvent(0, 5);
			}
		}
	}

	if (bPad[3])
	{
		if (blRx1 == FALSE && js.lRx >= thVALUE)
		{
			blRx1 = TRUE;
			bRepPad[6] = REPEAT_TIMES_1;
			changeEvent(0, 6);
		}
		else if (blRx1 == TRUE && js.lRx < thVALUE)
		{
			blRx1 = FALSE;
			bRepPad[6] = REPEAT_TIMES_1;
			changeEvent(1, 6);
		}
		else if (blRx1 == TRUE && js.lRx >= thVALUE && (FLAGPAD & 0x0040))
		{
			if (bRepPad[6] > 0)
				bRepPad[6]--;
			else
			{
				bRepPad[6] = REPEAT_TIMES_2;
				changeEvent(0, 6);
			}
		}

		if (blRx2 == FALSE && js.lRx <= (thVALUE * (-1)))
		{
			blRx2 = TRUE;
			bRepPad[7] = REPEAT_TIMES_1;
			changeEvent(0, 7);
		}
		else if (blRx2 == TRUE && js.lRx > (thVALUE * (-1)))
		{
			blRx2 = FALSE;
			bRepPad[7] = REPEAT_TIMES_1;
			changeEvent(1, 7);
		}
		else if (blRx2 == TRUE && js.lRx <= (thVALUE * (-1)) && (FLAGPAD & 0x0080))
		{
			if (bRepPad[7] > 0)
				bRepPad[7]--;
			else
			{
				bRepPad[7] = REPEAT_TIMES_2;
				changeEvent(0, 7);
			}
		}
	}

	if (bPad[4])
	{
		if (blRy1 == FALSE && js.lRy >= thVALUE)
		{
			blRy1 = TRUE;
			bRepPad[8] = REPEAT_TIMES_1;
			changeEvent(0, 8);
		}
		else if (blRy1 == TRUE && js.lRy < thVALUE)
		{
			blRy1 = FALSE;
			bRepPad[8] = REPEAT_TIMES_1;
			changeEvent(1, 8);
		}
		else if (blRy1 == TRUE && js.lRy >= thVALUE && (FLAGPAD & 0x0100))
		{
			if (bRepPad[8] > 0)
				bRepPad[8]--;
			else
			{
				bRepPad[8] = REPEAT_TIMES_2;
				changeEvent(0, 8);
			}
		}

		if (blRy2 == FALSE && js.lRy <= (thVALUE * (-1)))
		{
			blRy2 = TRUE;
			bRepPad[9] = REPEAT_TIMES_1;
			changeEvent(0, 9);
		}
		else if (blRy2 == TRUE && js.lRy > (thVALUE * (-1)))
		{
			blRy2 = FALSE;
			bRepPad[9] = REPEAT_TIMES_1;
			changeEvent(1, 9);
		}
		else if (blRy2 == TRUE && js.lRy <= (thVALUE * (-1)) && (FLAGPAD & 0x0200))
		{
			if (bRepPad[9] > 0)
				bRepPad[9]--;
			else
			{
				bRepPad[9] = REPEAT_TIMES_2;
				changeEvent(0, 9);
			}
		}
	}

	if (bPad[5])
	{
		if (blRz1 == FALSE && js.lRz >= thVALUE)
		{
			blRz1 = TRUE;
			bRepPad[10] = REPEAT_TIMES_1;
			changeEvent(0, 10);
		}
		else if (blRz1 == TRUE && js.lRz < thVALUE)
		{
			blRz1 = FALSE;
			bRepPad[10] = REPEAT_TIMES_1;
			changeEvent(1, 10);
		}
		else if (blRz1 == TRUE && js.lRz >= thVALUE && (FLAGPAD & 0x0400))
		{
			if (bRepPad[10] > 0)
				bRepPad[10]--;
			else
			{
				bRepPad[10] = REPEAT_TIMES_2;
				changeEvent(0, 10);
			}
		}

		if (blRz2 == FALSE && js.lRz <= (thVALUE * (-1)))
		{
			blRz2 = TRUE;
			bRepPad[11] = REPEAT_TIMES_1;
			changeEvent(0, 11);
		}
		else if (blRz2 == TRUE && js.lRz > (thVALUE * (-1)))
		{
			blRz2 = FALSE;
			bRepPad[11] = REPEAT_TIMES_1;
			changeEvent(1, 11);
		}
		else if (blRz2 == TRUE && js.lRz <= (thVALUE * (-1)) && (FLAGPAD & 0x0800))
		{
			if (bRepPad[11] > 0)
				bRepPad[11]--;
			else
			{
				bRepPad[11] = REPEAT_TIMES_2;
				changeEvent(0, 11);
			}
		}
	}

	// Slider controls
	if (bPad[6])
	{
		if (bSlider0 == FALSE && js.rglSlider[0] >= thVALUE)
		{
			bSlider0 = TRUE;
			bRepPad[12] = REPEAT_TIMES_1;
			changeEvent(0, 12);
		}
		else if (bSlider0 == TRUE && js.rglSlider[0] <= (thVALUE * (-1)))
		{
			bSlider0 = FALSE;
			bRepPad[12] = REPEAT_TIMES_1;
			changeEvent(1, 12);
		}
		else if (bSlider0 == TRUE && js.rglSlider[0] >= thVALUE && (FLAGPAD & 0x1000))
		{
			if (bRepPad[12] > 0)
				bRepPad[12]--;
			else
			{
				bRepPad[12] = REPEAT_TIMES_2;
				changeEvent(0, 12);
			}
		}
	}

	if (bPad[7])
	{
		if (bSlider1 == FALSE && js.rglSlider[1] >= thVALUE)
		{
			bSlider1 = TRUE;
			bRepPad[13] = REPEAT_TIMES_1;
			changeEvent(0, 13);
		}
		else if (bSlider1 == TRUE && js.rglSlider[1] <= (thVALUE * (-1)))
		{
			bSlider1 = TRUE;
			bRepPad[13] = REPEAT_TIMES_1;
			changeEvent(1, 13);
		}
		else if (bSlider1 == TRUE && js.rglSlider[1] >= thVALUE && (FLAGPAD & 0x2000))
		{
			if (bRepPad[13] > 0)
				bRepPad[13]--;
			else
			{
				bRepPad[13] = REPEAT_TIMES_2;
				changeEvent(0, 13);
			}
		}
	}

	// Hat button
	if (bPad[8])
	{
		// UP = 0
		if (bUP == FALSE && (js.rgdwPOV[0] == 0))
		{
			bUP = TRUE;
			bRepPad[14] = REPEAT_TIMES_1;
			changeEvent(0, 14);
		}
		else if (bUP == TRUE && (js.rgdwPOV[0] != 0))
		{
			bUP = FALSE;
			bRepPad[14] = REPEAT_TIMES_1;
			changeEvent(1, 14);
		}
		else if (bUP == TRUE && (js.rgdwPOV[0] == 0) && (FLAGHAT & 0x0001))
		{
			if (bRepPad[14] > 0)
				bRepPad[14]--;
			else
			{
				bRepPad[14] = REPEAT_TIMES_2;
				changeEvent(0, 14);
			}
		}

		// UP & RIGHT = 45

		if (bUP_RIGHT == FALSE && (js.rgdwPOV[0] == 4500))
		{
			bUP_RIGHT = TRUE;
			bRepPad[15] = REPEAT_TIMES_1;
			changeEvent(0, 15);
		}
		else if (bUP_RIGHT == TRUE && (js.rgdwPOV[0] != 4500))
		{
			bUP_RIGHT = FALSE;
			bRepPad[15] = REPEAT_TIMES_1;
			changeEvent(1, 15);
		}
		else if (bUP_RIGHT == TRUE && (js.rgdwPOV[0] == 4500) && (FLAGHAT & 0x0002))
		{
			if (bRepPad[15] > 0)
				bRepPad[15]--;
			else
			{
				bRepPad[15] = REPEAT_TIMES_2;
				changeEvent(0, 15);
			}
		}

		// RIGHT = 90
		if (bRIGHT == FALSE && (js.rgdwPOV[0] == 9000))
		{
			bRIGHT = TRUE;
			bRepPad[16] = REPEAT_TIMES_1;
			changeEvent(0, 16);
		}
		else if (bRIGHT == TRUE && (js.rgdwPOV[0] != 9000))
		{
			bRIGHT = FALSE;
			bRepPad[16] = REPEAT_TIMES_1;
			changeEvent(1, 16);
		}
		else if (bRIGHT == TRUE && (js.rgdwPOV[0] == 9000) && (FLAGHAT & 0x0004))
		{
			if (bRepPad[16] > 0)
				bRepPad[16]--;
			else
			{
				bRepPad[16] = REPEAT_TIMES_2;
				changeEvent(0, 16);
			}
		}

		// DOWN & RIGHT = 135
		if (bDOWN_RIGHT == FALSE && (js.rgdwPOV[0] == 13500))
		{
			bDOWN_RIGHT = TRUE;
			bRepPad[17] = REPEAT_TIMES_1;
			changeEvent(0, 17);
		}
		else if (bDOWN_RIGHT == TRUE && (js.rgdwPOV[0] != 13500))
		{
			bDOWN_RIGHT = FALSE;
			bRepPad[17] = REPEAT_TIMES_1;
			changeEvent(1, 17);
		}
		else if (bDOWN_RIGHT == TRUE && (js.rgdwPOV[0] == 13500) && (FLAGHAT & 0x0008))
		{
			if (bRepPad[17] > 0)
				bRepPad[17]--;
			else
			{
				bRepPad[17] = REPEAT_TIMES_2;
				changeEvent(0, 17);
			}
		}

		// DOWN = 180
		if (bDOWN == FALSE && (js.rgdwPOV[0] == 18000))
		{
			bDOWN = TRUE;
			bRepPad[18] = REPEAT_TIMES_1;
			changeEvent(0, 18);
		}
		else if (bDOWN == TRUE && (js.rgdwPOV[0] != 18000))
		{
			bDOWN = FALSE;
			bRepPad[18] = REPEAT_TIMES_1;
			changeEvent(1, 18);
		}
		else if (bDOWN == TRUE && (js.rgdwPOV[0] == 18000) && (FLAGHAT & 0x0010))
		{
			if (bRepPad[18] > 0)
				bRepPad[18]--;
			else
			{
				bRepPad[18] = REPEAT_TIMES_2;
				changeEvent(0, 18);
			}
		}

		// DOWN & LEFT = 225
		if (bDOWN_LEFT == FALSE && (js.rgdwPOV[0] == 22500))
		{
			bDOWN_LEFT = TRUE;
			bRepPad[19] = REPEAT_TIMES_1;
			changeEvent(0, 19);
		}
		else if (bDOWN_LEFT == TRUE && (js.rgdwPOV[0] != 22500))
		{
			bDOWN_LEFT = FALSE;
			bRepPad[19] = REPEAT_TIMES_1;
			changeEvent(1, 19);
		}
		else if (bDOWN_LEFT == TRUE && (js.rgdwPOV[0] == 22500) && (FLAGHAT & 0x0020))
		{
			if (bRepPad[19] > 0)
				bRepPad[19]--;
			else
			{
				bRepPad[19] = REPEAT_TIMES_2;
				changeEvent(0, 19);
			}
		}

		// LEFT = 270
		if (bLEFT == FALSE && (js.rgdwPOV[0] == 27000))
		{
			bLEFT = TRUE;
			bRepPad[20] = REPEAT_TIMES_1;
			changeEvent(0, 20);
		}
		else if (bLEFT == TRUE && (js.rgdwPOV[0] != 27000))
		{
			bLEFT = FALSE;
			bRepPad[20] = REPEAT_TIMES_1;
			changeEvent(1, 20);
		}
		else if (bLEFT == TRUE && (js.rgdwPOV[0] == 27000) && (FLAGHAT & 0x0040))
		{
			if (bRepPad[20] > 0)
				bRepPad[20]--;
			else
			{
				bRepPad[20] = REPEAT_TIMES_2;
				changeEvent(0, 20);
			}
		}

		// UP & LEFT = 315
		if (bUP_LEFT == FALSE && (js.rgdwPOV[0] == 31500))
		{
			bUP_LEFT = TRUE;
			bRepPad[21] = REPEAT_TIMES_1;
			changeEvent(0, 21);
		}
		else if (bUP_LEFT == TRUE && (js.rgdwPOV[0] != 31500))
		{
			bUP_LEFT = FALSE;
			bRepPad[21] = REPEAT_TIMES_1;
			changeEvent(1, 21);
		}
		else if (bUP_LEFT == TRUE && (js.rgdwPOV[0] == 31500) && (FLAGHAT & 0x0080))
		{
			if (bRepPad[21] > 0)
				bRepPad[21]--;
			else
			{
				bRepPad[21] = REPEAT_TIMES_2;
				changeEvent(0, 21);
			}
		}
	}

	// buttons
	unsigned int m_bit = 1;
	for (int i = 0; i < MAX_GPBUTTON; i++)
	{
		if (bButton[i] == FALSE && js.rgbButtons[i] & 0x80)
		{
			bButton[i] = TRUE;
			bRepeat[i] = REPEAT_TIMES_1;
			changeEvent(0, 32 + i);
		}
		else if (bButton[i] && !(js.rgbButtons[i] & 0x80))
		{
			bButton[i] = FALSE;
			bRepeat[i] = REPEAT_TIMES_1;
			changeEvent(1, 32 + i);
		}
		else if (bButton[i] == TRUE && js.rgbButtons[i] & 0x80 && (FLAGBUTTON & m_bit))
		{
			if (bRepeat[i] > 0)
			{
				bRepeat[i]--;
			}
			else
			{
				bRepeat[i] = REPEAT_TIMES_2;
				changeEvent(0, 32 + i);
			}
		}
		m_bit = m_bit << 1;
	}

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: FreeDirectInput()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
VOID FreeDirectInput()
{
	// Unacquire the device one last time just in case
	// the app tried to exit while the device is still acquired.
	if (g_pJoystick)
		g_pJoystick->Unacquire();

	// Release any DirectInput objects.
	SAFE_RELEASE(g_pJoystick);
	SAFE_RELEASE(g_pDI);
}
