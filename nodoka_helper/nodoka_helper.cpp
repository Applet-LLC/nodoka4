// nodoka_helper.cpp
//

#include "stdafx.h"
#include "nodoka_helper.h"
#include "nodoka.h"
#include "hook.h"
#include "windowstool.h"
//#include <Msctf.h>
#include "..\sirius_sdk\commonValues.h"

#define MAX_LOADSTRING 100

typedef SIRIUS_HOOK_API CcommonValues *(*SiriusSetupHookPtr)(DWORD dwMessageId);
typedef SIRIUS_HOOK_API void *(*SiriusReleaseHookPtr)();

// グローバル変数:
HINSTANCE hInst;					 // 現在のインターフェイス
TCHAR szTitle[MAX_LOADSTRING];		 // タイトル バーのテキスト
TCHAR szWindowClass[MAX_LOADSTRING]; // メイン ウィンドウ クラス名

// for Sirius TSF SDK
HMODULE hMsctf = NULL;
SiriusSetupHookPtr mySiriusSetupHook;
SiriusReleaseHookPtr mySiriusReleaseHook;
CcommonValues *pCv;

// このコード モジュールに含まれる関数の宣言を転送します:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance,
					   HINSTANCE hPrevInstance,
					   LPTSTR lpCmdLine,
					   int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	MSG msg;

	// 二重起動をチェックします。
	HANDLE mutex = CreateMutex((SECURITY_ATTRIBUTES *)NULL, TRUE, MUTEX_NODOKA_HELPER_EXCLUSIVE_RUNNING);
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		return FALSE;
	}

	// グローバル文字列を初期化しています。
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_NODOKA_HELPER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// アプリケーションの初期化を実行します:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	CHECK_FALSE(installHooks());

	// load sirius_hook dll
	hMsctf = LoadLibraryA("sirius_hook_for_nodoka_x86.dll");

	mySiriusSetupHook = (SiriusSetupHookPtr)GetProcAddress(hMsctf, "SiriusSetupHook");
	mySiriusReleaseHook = (SiriusReleaseHookPtr)GetProcAddress(hMsctf, "SiriusReleaseHook");

	DWORD wm_sirius_control = RegisterWindowMessage(L"WM_SIRIUS_CONTROL");

	if (mySiriusSetupHook != NULL)
		pCv = mySiriusSetupHook(wm_sirius_control);

	// メイン メッセージ ループ:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// unload Sirius
	if (mySiriusReleaseHook != NULL)
		mySiriusReleaseHook();
	if (hMsctf != NULL)
		FreeLibrary(hMsctf);

	CHECK_FALSE(uninstallHooks());

	DWORD_PTR dwResult;
	SendMessageTimeout(HWND_BROADCAST, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 5000, &dwResult);

	if (mutex != NULL)
		CHECK_TRUE(CloseHandle(mutex));

	return (int)msg.wParam;
}

//
//  関数: MyRegisterClass()
//
//  目的: ウィンドウ クラスを登録します。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;
	ZeroMemory(&wcex, sizeof(wcex));
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.lpszClassName = szWindowClass;

	return RegisterClassEx(&wcex);
}

//
//   関数: InitInstance(HINSTANCE, int)
//
//   目的: インスタンス ハンドルを保存して、メイン ウィンドウを作成します。
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd;

	hInst = hInstance; // グローバル変数にインスタンス処理を格納します。

	hWnd = CreateWindowEx(0, szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
						  0, 0, 0, 0, NULL, NULL, hInstance, NULL);

	return TRUE;
}

//
//  関数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的:  メイン ウィンドウのメッセージを処理します。
//
//  WM_DESTROY	- 中止メッセージを表示して戻る
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
