// commonValues.h: CcommonValues クラスのインターフェイス
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_COMMONVALUES_H__E5ADF547_DF32_4834_A7DD_9396EA49100B__INCLUDED_)
#define AFX_COMMONVALUES_H__E5ADF547_DF32_4834_A7DD_9396EA49100B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WM_SIRIUS_PING (WM_APP + 0)
#define WM_SIRIUS_POPFUNCTION (WM_APP + 1)
#define WM_SIRIUS_TASKTRAY (WM_APP + 2)
#define WM_SIRIUS_RELOAD (WM_APP + 3)
#define WM_SIRIUS_CONFIGEDIT (WM_APP + 4)
#define WM_SIRIUS_HELP (WM_APP + 5)
#define WM_SIRIUS_ABOUT (WM_APP + 6)

#define MAX_REPLACE_SIZE 256
#define NATURE_SIZE 256

enum cmd
{
    e_null = 0,
    e_imeClear,
    e_getImeString,
    e_peekMessage,
    e_peekKeyMessage,
    //      e_getFocus,
    e_getImeStatus,
    e_getImeStatusEx,
    e_imeOpen,
    e_getImeStatus2 = 10
};

struct CcommonValues
{
    HWND m_hWndFocus;
#ifndef _WIN64
    char *dmy1;
#endif
    HWND m_hWndFront;
#ifndef _WIN64
    char *dmy2;
#endif
    WCHAR m_strNatureBuffer[NATURE_SIZE];
    WCHAR m_strImeBuffer[MAX_REPLACE_SIZE];
    BOOL m_multipleMessage;
    DWORD m_wm_sirius_controll;
    cmd m_command;
    BOOL m_menuLoop;
    int m_nNaturePos;
    BOOL m_peekMessageResult;

    DWORD m_conversion;
    DWORD m_sentence;
    COMPOSITIONFORM m_composition;
    BOOL m_Composition;
    CANDIDATEFORM m_candidate;
    LOGFONT m_font;
    DWORD m_bImeStatus;
    DWORD test;
    ULONG m_imeStringSync;
    BOOL m_commandComplete;
    BOOL m_bEffective;

    BOOL m_supportTsf;
    BOOL m_supportTsfOpenClose;
    BOOL m_supportTsfConversion;
    BOOL m_supportTsfSentence;
    BOOL m_supportTsfComposition;
    BOOL m_x64;
    WCHAR m_moduleName[MAX_PATH];
    DWORD m_processId;
    DWORD m_threadId;
    bool m_isHooked;
};

#ifdef SIRIUS_HOOK_EXPORTS
#define SIRIUS_HOOK_API __declspec(dllexport)
#else
#define SIRIUS_HOOK_API __declspec(dllimport)
#endif

extern "C"
{
    SIRIUS_HOOK_API void insertNatureBuffer(LPCWSTR str);
    SIRIUS_HOOK_API void deleteNatureBuffer(int count);
    SIRIUS_HOOK_API void clearNatureBuffer();
    SIRIUS_HOOK_API void SiriusReleaseHook();
    SIRIUS_HOOK_API CcommonValues *SiriusSetupHook(DWORD dwMessageId);
    SIRIUS_HOOK_API CcommonValues *SiriusCommonValue();
};

#endif // !defined(AFX_COMMONVALUES_H__E5ADF547_DF32_4834_A7DD_9396EA49100B__INCLUDED_)
