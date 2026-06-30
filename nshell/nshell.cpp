// nshell.cpp
// のどかヘルパー: 指定プログラムの実行前後にのどかを終了/再起動する
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>

// nodoka/mayu のメインウィンドウクラス名（nodoka.cpp の "nodokaTasktray" より）
static const WCHAR* NODOKA_WND_CLASS = L"nodokaTasktray";
static const WCHAR* MAYU_WND_CLASS   = L"mayuTasktray";

// 検索対象プロセス名（優先順）
static const WCHAR* NODOKA_EXES[] = {
    L"nodoka.exe", L"nodoka64.exe", L"mayu.exe", nullptr
};

static const DWORD WAIT_MS          = 3000;       // 終了・起動の前後バッファ
static const DWORD NODOKA_EXIT_WAIT = 10000;      // nodoka 終了待ちタイムアウト
static const DWORD CASE2_TIMEOUT_MS = 3 * 60000; // ケース2: ウィンドウ出現待ち上限
static const DWORD CASE2_STABLE_MS  = 5000;      // ケース2: ウィンドウ安定確認時間
static const DWORD CASE2_POLL_MS    = 500;        // ケース2: ポーリング間隔

struct NodokaInfo {
    WCHAR  path[MAX_PATH];
    HANDLE hProcess;
    bool   found;
};

// ─── nodoka/mayu プロセス検出 ────────────────────────────────────────────────

static bool FindNodoka(NodokaInfo& out) {
    out = {};
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    bool found = false;

    if (Process32FirstW(snap, &pe)) {
        do {
            for (int i = 0; NODOKA_EXES[i]; ++i) {
                if (_wcsicmp(pe.szExeFile, NODOKA_EXES[i]) == 0) {
                    HANDLE h = OpenProcess(
                        PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
                        FALSE, pe.th32ProcessID);
                    if (h) {
                        DWORD size = MAX_PATH;
                        if (QueryFullProcessImageNameW(h, 0, out.path, &size)) {
                            out.hProcess = h;
                            found = true;
                        } else {
                            CloseHandle(h);
                        }
                    }
                    break;
                }
            }
        } while (!found && Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    out.found = found;
    return found;
}

// ─── nodoka 終了 ─────────────────────────────────────────────────────────────

static void TerminateNodoka(NodokaInfo& info) {
    HWND hwnd = FindWindowW(NODOKA_WND_CLASS, nullptr);
    if (!hwnd) hwnd = FindWindowW(MAYU_WND_CLASS, nullptr);

    if (hwnd) PostMessage(hwnd, WM_CLOSE, 0, 0);

    if (info.hProcess) {
        WaitForSingleObject(info.hProcess, NODOKA_EXIT_WAIT);
        CloseHandle(info.hProcess);
        info.hProcess = nullptr;
    }
}

// ─── nodoka 再起動 ───────────────────────────────────────────────────────────

static void RestartNodoka(const NodokaInfo& info) {
    if (!info.found || info.path[0] == L'\0') return;
    Sleep(WAIT_MS);
    ShellExecuteW(nullptr, L"open", info.path, nullptr, nullptr, SW_SHOW);
}

// ─── プロセス起動 ────────────────────────────────────────────────────────────

static HANDLE LaunchProcess(const WCHAR* cmdline) {
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    WCHAR buf[32768];
    wcsncpy_s(buf, cmdline, _TRUNCATE);

    if (!CreateProcessW(nullptr, buf, nullptr, nullptr,
                        FALSE, 0, nullptr, nullptr, &si, &pi)) {
        DWORD err = GetLastError();
        WCHAR msg[1100];
        _snwprintf_s(msg, _countof(msg), _TRUNCATE,
            L"起動に失敗しました。\n\nコマンドライン:\n%s\n\nエラーコード: %lu",
            cmdline, err);
        MessageBoxW(nullptr, msg, L"nshell エラー", MB_OK | MB_ICONERROR);
        return nullptr;
    }
    CloseHandle(pi.hThread);
    return pi.hProcess;
}

// ─── コマンドライン操作ヘルパー ──────────────────────────────────────────────

// GetCommandLineW() から空白を読み飛ばし、トークンをひとつ読み飛ばした先頭を返す
static const WCHAR* SkipOneToken(const WCHAR* p) {
    while (*p == L' ' || *p == L'\t') ++p;
    if (*p == L'"') {
        ++p;
        while (*p && *p != L'"') ++p;
        if (*p) ++p;
    } else {
        while (*p && *p != L' ' && *p != L'\t') ++p;
    }
    while (*p == L' ' || *p == L'\t') ++p;
    return p;
}

// GetCommandLineW() から「プログラム名 + extraSkip 個のトークン」を読み飛ばした残りを返す
//   ケース0: extraSkip=0 → "exe arg1 arg2 ..."
//   ケース1: extraSkip=1 → "exe"
//   ケース2: extraSkip=2 → "exe"  ("2" と title を読み飛ばす)
//   ケース4: extraSkip=1 → "exe"
static const WCHAR* GetRestCmdLine(int extraSkip) {
    const WCHAR* p = GetCommandLineW();
    p = SkipOneToken(p); // プログラム名を読み飛ばす
    for (int i = 0; i < extraSkip; ++i)
        p = SkipOneToken(p);
    return p;
}

// ─── ケース2: ウィンドウタイトル監視 ─────────────────────────────────────────

struct FindWndData { const WCHAR* title; HWND hwnd; };

static BOOL CALLBACK FindWndCallback(HWND hwnd, LPARAM lp) {
    FindWndData* d = reinterpret_cast<FindWndData*>(lp);
    WCHAR text[512] = {};
    GetWindowTextW(hwnd, text, _countof(text));
    if (wcscmp(text, d->title) == 0) { d->hwnd = hwnd; return FALSE; }
    return TRUE;
}

static HWND FindWindowByTitle(const WCHAR* title) {
    FindWndData d = { title, nullptr };
    EnumWindows(FindWndCallback, reinterpret_cast<LPARAM>(&d));
    return d.hwnd;
}

static void WaitAndRestartCase2(const NodokaInfo& info, const WCHAR* title) {
    DWORD deadline    = GetTickCount() + CASE2_TIMEOUT_MS;
    DWORD stableStart = 0;
    bool  appeared    = false;

    // フェーズ1: ウィンドウが CASE2_STABLE_MS 以上安定して存在するまで待つ（タイムアウトあり）
    while (GetTickCount() < deadline) {
        if (FindWindowByTitle(title)) {
            if (stableStart == 0) stableStart = GetTickCount();
            if (GetTickCount() - stableStart >= CASE2_STABLE_MS) {
                appeared = true;
                break;
            }
        } else {
            stableStart = 0;
        }
        Sleep(CASE2_POLL_MS);
    }

    // タイムアウト: nshell.exe はそのまま残存（タスクマネージャで終了が必要）
    if (!appeared) return;

    // フェーズ2: ウィンドウが消えるまで待つ
    while (FindWindowByTitle(title)) Sleep(CASE2_POLL_MS);

    RestartNodoka(info);
}

// ─── ヘルプダイアログ ────────────────────────────────────────────────────────

static void ShowUsage() {
    MessageBoxW(nullptr,
        L"nshell.exe / nshell64.exe  ver 1.06\n"
        L"\n"
        L"[ケース0]  のどか終了 → プログラム起動 → プロセス終了後のどか再起動\n"
        L"  nshell <実行プログラム名> [引数...]\n"
        L"\n"
        L"[ケース1]  のどか終了 → プログラム起動（再起動なし）\n"
        L"  nshell 1 <実行プログラム名>\n"
        L"\n"
        L"[ケース2]  のどか終了 → プログラム起動 → タイトル消滅後のどか再起動\n"
        L"  nshell 2 <ウィンドウタイトル> <実行プログラム名>\n"
        L"\n"
        L"[ケース3]  のどかを終了するのみ\n"
        L"  nshell 3\n"
        L"\n"
        L"[ケース4]  のどか終了 → プログラム起動 → プロセス終了後のどか再起動（引数なし）\n"
        L"  nshell 4 <実行プログラム名>",
        L"nshell", MB_OK | MB_ICONINFORMATION);
}

// ─── エントリポイント ─────────────────────────────────────────────────────────

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    int     argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 0;

    // 引数なし → ヘルプダイアログ
    if (argc < 2) {
        ShowUsage();
        LocalFree(argv);
        return 0;
    }

    NodokaInfo info = {};
    FindNodoka(info);

    const WCHAR* a1 = argv[1];

    // ケース3: のどか終了のみ
    if (wcscmp(a1, L"3") == 0) {
        if (info.found) TerminateNodoka(info);
        LocalFree(argv);
        return 0;
    }

    // ケース1: のどか終了 → 起動（再起動なし）
    if (wcscmp(a1, L"1") == 0) {
        if (argc < 3) { ShowUsage(); LocalFree(argv); return 0; }
        if (info.found) TerminateNodoka(info);
        Sleep(WAIT_MS);
        HANDLE h = LaunchProcess(GetRestCmdLine(1));
        if (h) CloseHandle(h);
        LocalFree(argv);
        return 0;
    }

    // ケース2: のどか終了 → 起動 → ウィンドウタイトル監視 → 再起動
    if (wcscmp(a1, L"2") == 0) {
        if (argc < 4) { ShowUsage(); LocalFree(argv); return 0; }
        const WCHAR* title = argv[2];
        if (FindWindowByTitle(title)) {
            MessageBoxW(nullptr,
                L"指定されたウィンドウタイトルが既に存在します。\n"
                L"のどかの再起動を行いません。",
                L"nshell エラー", MB_OK | MB_ICONERROR);
            LocalFree(argv);
            return 0;
        }
        if (info.found) TerminateNodoka(info);
        Sleep(WAIT_MS);
        HANDLE h = LaunchProcess(GetRestCmdLine(2));
        if (h) CloseHandle(h);
        WaitAndRestartCase2(info, title);
        LocalFree(argv);
        return 0;
    }

    // ケース4: のどか終了 → 起動 → プロセス終了待ち → 再起動
    if (wcscmp(a1, L"4") == 0) {
        if (argc < 3) { ShowUsage(); LocalFree(argv); return 0; }
        if (info.found) TerminateNodoka(info);
        Sleep(WAIT_MS);
        HANDLE h = LaunchProcess(GetRestCmdLine(1));
        if (h) { WaitForSingleObject(h, INFINITE); CloseHandle(h); }
        RestartNodoka(info);
        LocalFree(argv);
        return 0;
    }

    // ケース0 (デフォルト): のどか終了 → 起動（引数付き）→ プロセス終了待ち → 再起動
    if (info.found) TerminateNodoka(info);
    Sleep(WAIT_MS);
    HANDLE h = LaunchProcess(GetRestCmdLine(0));
    if (h) { WaitForSingleObject(h, INFINITE); CloseHandle(h); }
    RestartNodoka(info);

    LocalFree(argv);
    return 0;
}
