/*
  天狼のDLLを利用して、IMEの状態を取得するサンプルです。
  実行の前に、このプロジェクトをビルドして生成される getCommonValues.exe と
  同じディレクトリに　sirius_core_x86.dll をコピーしておいて下さい。
  sirius_core_x86.dll は、天狼のインストールディレクトリにあります。
  天狼が起動している場合、天狼を終了してから実行して下さい。
 */

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <Msctf.h>
#include <process.h>

// commonValues.h
#include "..\..\include\sirius\commonValues.h"

typedef SIRIUS_HOOK_API CcommonValues *(*SiriusSetupHookPtr)(DWORD dwMessageId);
typedef SIRIUS_HOOK_API void *(*SiriusReleaseHookPtr)();

static CRITICAL_SECTION cs;
static int polling_status;

void OpenPollingStatus()
{
  InitializeCriticalSection(&cs);
  polling_status = 1;
}

void ClosePollingStatus()
{
  DeleteCriticalSection(&cs);
}

int GetPollingStatus()
{
  int n = 0;
  EnterCriticalSection(&cs);
  n = polling_status;
  LeaveCriticalSection(&cs);
  return n;
}

void SetPollingStatus(int n)
{
  EnterCriticalSection(&cs);
  polling_status = n;
  LeaveCriticalSection(&cs);
}

DWORD WINAPI ui_polling(LPVOID)
{
  ::MessageBox(::GetDesktopWindow(), L"polling now", L"Sirius Hook Test", MB_OK);
  SetPollingStatus(0);
  return 0;
}

int _tmain(int argc, _TCHAR *argv[])
{
  HMODULE hMsctf = NULL;
#ifdef _WIN64
  hMsctf = LoadLibrary(L"sirius_hook_x64.dll");
#else
  hMsctf = LoadLibrary(L"sirius_hook_x86.dll");
#endif
  SiriusSetupHookPtr mySiriusSetupHook = (SiriusSetupHookPtr)GetProcAddress(hMsctf, "SiriusSetupHook");
  SiriusReleaseHookPtr mySiriusReleaseHook = (SiriusReleaseHookPtr)GetProcAddress(hMsctf, "SiriusReleaseHook");
  DWORD wm_sirius_control = RegisterWindowMessage(L"WM_SIRIUS_CONTROL");
  CcommonValues *pCv = mySiriusSetupHook(wm_sirius_control);

  OpenPollingStatus();
  CreateThread(NULL, 0, ui_polling, NULL, 0, NULL);
  while (GetPollingStatus() == 1)
  {
    wprintf(L"m_ishooked = %d\n", pCv->m_isHooked);
    wprintf(L"m_supportTsf is %s\n", pCv->m_supportTsf ? L"enable" : L"disable");
    wprintf(L"IL support %s; -IL=%s\n", pCv->m_supportTsfOpenClose ? L"enable" : L"disable", pCv->m_bImeStatus ? L"ON" : L"OFF");
    wprintf(L"IC support %d; -IC=%d\n", pCv->m_supportTsfComposition, pCv->m_Composition);
    wprintf(L"conv support %s; conv=%4x\n", pCv->m_supportTsfConversion ? L"enable" : L"disable", pCv->m_conversion);
    Sleep(1000);
  }
  ClosePollingStatus();
  mySiriusReleaseHook();

  DWORD_PTR dwResult;
  SendMessageTimeout(HWND_BROADCAST, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 5000, &dwResult);

  return 0;
}
