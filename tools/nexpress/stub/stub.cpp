/*
 * stub.cpp — nexpress SFX stub
 *
 * Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
 * License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/
 *
 * File layout (created by nexpress.exe):
 *   [stub.exe bytes][CAB data][SFXTrailer]
 * After signtool signing:
 *   [stub.exe bytes][CAB data][SFXTrailer][WIN_CERTIFICATE]
 *
 * On launch: extracts the embedded CAB to a temp dir and runs AppLaunched.
 */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <fdi.h>
#include <strsafe.h>
#include <fcntl.h>

#pragma comment(lib, "cabinet.lib")

// -----------------------------------------------------------------------
// SFX trailer (must match main.cpp exactly)
// -----------------------------------------------------------------------
#pragma pack(push, 1)
struct SFXTrailer {
    UINT64 magic;
    UINT64 cabOffset;
    UINT64 cabSize;
    char   appLaunched[256];
};
#pragma pack(pop)

static const UINT64 NEXPRESS_MAGIC =
    ((UINT64)'N')        |
    ((UINT64)'E' <<  8)  |
    ((UINT64)'X' << 16)  |
    ((UINT64)'P' << 24)  |
    ((UINT64)'R' << 32)  |
    ((UINT64)'S' << 40)  |
    ((UINT64)'S' << 48)  |
    ((UINT64)'1' << 56);

// -----------------------------------------------------------------------
// Log (written next to the SFX exe as <name>.log)
// -----------------------------------------------------------------------
static HANDLE g_hLog = INVALID_HANDLE_VALUE;

static void Log(LPCWSTR fmt, ...) {
    if (g_hLog == INVALID_HANDLE_VALUE) return;
    WCHAR wbuf[1024];
    va_list ap;
    va_start(ap, fmt);
    StringCchVPrintfW(wbuf, ARRAYSIZE(wbuf), fmt, ap);
    va_end(ap);
    // Convert to UTF-8
    CHAR abuf[2048];
    int n = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, abuf, (int)sizeof(abuf) - 2, NULL, NULL);
    if (n > 0) {
        abuf[n - 1] = '\r';
        abuf[n]     = '\n';
        DWORD w = 0;
        WriteFile(g_hLog, abuf, n + 1, &w, NULL);
    }
}

static void OpenLog(LPCWSTR selfPath) {
    WCHAR logPath[MAX_PATH];
    StringCchCopyW(logPath, MAX_PATH, selfPath);
    // Replace extension with .log (find last dot after last backslash)
    WCHAR *lastDot = NULL;
    for (WCHAR *p = logPath; *p; p++) {
        if (*p == L'\\' || *p == L'/') lastDot = NULL;
        else if (*p == L'.') lastDot = p;
    }
    if (lastDot) StringCchCopyW(lastDot, MAX_PATH - (lastDot - logPath), L".log");
    else         StringCchCatW(logPath, MAX_PATH, L".log");

    g_hLog = CreateFileW(logPath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_hLog != INVALID_HANDLE_VALUE) {
        // UTF-8 BOM
        BYTE bom[] = { 0xEF, 0xBB, 0xBF };
        DWORD w = 0;
        WriteFile(g_hLog, bom, sizeof(bom), &w, NULL);
    }
}

// -----------------------------------------------------------------------
// FDI callbacks
// -----------------------------------------------------------------------
static FNALLOC(fdi_alloc) { return HeapAlloc(GetProcessHeap(), 0, cb); }
static FNFREE (fdi_free)  { HeapFree(GetProcessHeap(), 0, pv); }

static FNOPEN(fdi_open) {
    // NOTE: _O_RDONLY == 0, so we must check _O_WRONLY/_O_RDWR; default is read.
    DWORD access;
    if      (oflag & _O_RDWR)   access = GENERIC_READ | GENERIC_WRITE;
    else if (oflag & _O_WRONLY) access = GENERIC_WRITE;
    else                        access = GENERIC_READ;   // _O_RDONLY = 0

    DWORD share = (access == GENERIC_READ) ? FILE_SHARE_READ : 0;
    DWORD disp  = (oflag & _O_CREAT) ? CREATE_ALWAYS : OPEN_EXISTING;

    HANDLE h = CreateFileA(pszFile, access, share, NULL, disp,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        Log(L"fdi_open FAILED: %hs (oflag=0x%x err=%lu)", pszFile, oflag, GetLastError());
    return (h == INVALID_HANDLE_VALUE) ? -1 : (INT_PTR)h;
}
static FNREAD (fdi_read)  { DWORD n=0; ReadFile ((HANDLE)hf,pv,cb,&n,NULL); return n; }
static FNWRITE(fdi_write) { DWORD n=0; WriteFile((HANDLE)hf,pv,cb,&n,NULL); return n; }
static FNCLOSE(fdi_close) { CloseHandle((HANDLE)hf); return 0; }
static FNSEEK (fdi_seek)  { return (long)SetFilePointer((HANDLE)hf,dist,NULL,seektype); }

// -----------------------------------------------------------------------
// Extraction context and notification callback
// -----------------------------------------------------------------------
struct ExtractCtx { WCHAR destDir[MAX_PATH]; };

static FNFDINOTIFY(fdi_notify) {
    switch (fdint) {
    case fdintCOPY_FILE: {
        ExtractCtx *ctx = (ExtractCtx *)pfdin->pv;
        WCHAR wname[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, pfdin->psz1, -1, wname, MAX_PATH);
        Log(L"  extract: %s", wname);

        WCHAR dest[MAX_PATH];
        StringCchCopyW(dest, MAX_PATH, ctx->destDir);
        size_t dlen = wcslen(dest);
        if (dlen && dest[dlen-1] != L'\\') StringCchCatW(dest, MAX_PATH, L"\\");
        StringCchCatW(dest, MAX_PATH, wname);

        HANDLE h = CreateFileW(dest, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE)
            Log(L"  CreateFile FAILED for %s (err=%lu)", dest, GetLastError());
        return (h == INVALID_HANDLE_VALUE) ? -1 : (INT_PTR)h;
    }
    case fdintCLOSE_FILE_INFO:
        CloseHandle((HANDLE)pfdin->hf);
        return TRUE;
    default:
        return 0;
    }
}

// -----------------------------------------------------------------------
// Recursive directory deletion
// -----------------------------------------------------------------------
static void DeleteDirRecursive(LPCWSTR dir) {
    WCHAR pat[MAX_PATH];
    StringCchCopyW(pat, MAX_PATH, dir);
    StringCchCatW(pat, MAX_PATH, L"\\*");

    WIN32_FIND_DATAW fd;
    HANDLE hf = FindFirstFileW(pat, &fd);
    if (hf != INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
            WCHAR child[MAX_PATH];
            StringCchCopyW(child, MAX_PATH, dir);
            StringCchCatW(child, MAX_PATH, L"\\");
            StringCchCatW(child, MAX_PATH, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                DeleteDirRecursive(child);
            else
                DeleteFileW(child);
        } while (FindNextFileW(hf, &fd));
        FindClose(hf);
    }
    RemoveDirectoryW(dir);
}

// -----------------------------------------------------------------------
// WinMain
// -----------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

    // --- 1. Open self ---
    WCHAR selfPath[MAX_PATH];
    GetModuleFileNameW(NULL, selfPath, MAX_PATH);

    OpenLog(selfPath);
    Log(L"nexpress stub start");
    Log(L"self: %s", selfPath);

    HANDLE hSelf = CreateFileW(selfPath, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSelf == INVALID_HANDLE_VALUE) {
        Log(L"FAILED: cannot open self (err=%lu)", GetLastError());
        MessageBoxW(NULL, L"Cannot open self.", L"nexpress", MB_ICONERROR);
        return 1;
    }

    LARGE_INTEGER fileSize;
    GetFileSizeEx(hSelf, &fileSize);
    Log(L"file size: %I64u bytes", fileSize.QuadPart);

    // --- 2. Locate SFXTrailer ---
    // signtool appends WIN_CERTIFICATE to EOF.  Scan the last 64 KB backwards
    // for NEXPRESS_MAGIC; Authenticode certs are well under 64 KB.
    const DWORD SCAN_LIMIT = 65536;
    DWORD scanSize = (DWORD)min((LONGLONG)SCAN_LIMIT, fileSize.QuadPart);
    BYTE *scanBuf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, scanSize);
    if (!scanBuf) {
        CloseHandle(hSelf);
        MessageBoxW(NULL, L"Out of memory.", L"nexpress", MB_ICONERROR);
        return 1;
    }

    LARGE_INTEGER scanStart;
    scanStart.QuadPart = fileSize.QuadPart - scanSize;
    SetFilePointerEx(hSelf, scanStart, NULL, FILE_BEGIN);
    DWORD nr = 0;
    ReadFile(hSelf, scanBuf, scanSize, &nr, NULL);
    Log(L"scanning last %lu bytes for trailer magic", nr);

    SFXTrailer trailer;
    BOOL trailerFound = FALSE;
    for (int i = (int)nr - (int)sizeof(SFXTrailer); i >= 0; i--) {
        UINT64 m = 0;
        memcpy(&m, scanBuf + i, sizeof(m));
        if (m == NEXPRESS_MAGIC) {
            memcpy(&trailer, scanBuf + i, sizeof(SFXTrailer));
            trailerFound = TRUE;
            Log(L"trailer found at file offset %I64u",
                scanStart.QuadPart + i);
            break;
        }
    }
    HeapFree(GetProcessHeap(), 0, scanBuf);

    if (!trailerFound) {
        CloseHandle(hSelf);
        Log(L"FAILED: trailer not found");
        MessageBoxW(NULL, L"Invalid SFX format.", L"nexpress", MB_ICONERROR);
        return 1;
    }

    WCHAR appW[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, trailer.appLaunched, -1, appW, MAX_PATH);
    Log(L"cabOffset: %I64u", trailer.cabOffset);
    Log(L"cabSize:   %I64u", trailer.cabSize);
    Log(L"launch:    %s", appW);

    // --- 3. Build unique temp paths ---
    WCHAR tempRoot[MAX_PATH];
    GetTempPathW(MAX_PATH, tempRoot);
    DWORD pid = GetCurrentProcessId();

    WCHAR tempCab[MAX_PATH];
    StringCchPrintfW(tempCab, MAX_PATH, L"%snexpress_%08X.cab", tempRoot, pid);

    WCHAR tempDir[MAX_PATH];
    StringCchPrintfW(tempDir, MAX_PATH, L"%snexpress_%08X_out", tempRoot, pid);
    CreateDirectoryW(tempDir, NULL);

    Log(L"tempCab: %s", tempCab);
    Log(L"tempDir: %s", tempDir);

    // --- 4. Copy CAB bytes from self to temp file ---
    LARGE_INTEGER cabPos;
    cabPos.QuadPart = (LONGLONG)trailer.cabOffset;
    SetFilePointerEx(hSelf, cabPos, NULL, FILE_BEGIN);

    HANDLE hCab = CreateFileW(tempCab, GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hCab == INVALID_HANDLE_VALUE) {
        CloseHandle(hSelf);
        Log(L"FAILED: cannot create temp cab (err=%lu)", GetLastError());
        MessageBoxW(NULL, L"Cannot create temporary file.", L"nexpress", MB_ICONERROR);
        return 1;
    }

    UINT64 remaining = trailer.cabSize;
    UINT64 written = 0;
    BYTE buf[65536];
    while (remaining > 0) {
        DWORD chunk = (remaining < sizeof(buf)) ? (DWORD)remaining : (DWORD)sizeof(buf);
        DWORD r = 0, w = 0;
        if (!ReadFile(hSelf, buf, chunk, &r, NULL) || r == 0) break;
        WriteFile(hCab, buf, r, &w, NULL);
        remaining -= r;
        written   += w;
    }
    CloseHandle(hCab);
    CloseHandle(hSelf);
    Log(L"CAB written: %I64u bytes (expected %I64u)", written, trailer.cabSize);

    // --- 5. Extract CAB via FDI ---
    Log(L"FDI extract start");
    ERF erf = {0};
    HFDI hfdi = FDICreate(fdi_alloc, fdi_free,
                          fdi_open, fdi_read, fdi_write, fdi_close, fdi_seek,
                          cpuUNKNOWN, &erf);
    BOOL fdiOk = FALSE;
    if (hfdi) {
        CHAR ansiCab[MAX_PATH];
        WideCharToMultiByte(CP_ACP, 0, tempCab, -1, ansiCab, MAX_PATH, NULL, NULL);

        CHAR *pName = ansiCab;
        for (CHAR *q = ansiCab; *q; q++)
            if (*q == '\\') pName = q + 1;

        CHAR cabName[MAX_PATH];
        StringCchCopyA(cabName, MAX_PATH, pName);
        CHAR cabDir[MAX_PATH];
        StringCchCopyA(cabDir, MAX_PATH, ansiCab);
        cabDir[(size_t)(pName - ansiCab)] = '\0';

        Log(L"FDICopy: dir=[%hs] name=[%hs]", cabDir, cabName);

        ExtractCtx ctx;
        StringCchCopyW(ctx.destDir, MAX_PATH, tempDir);
        fdiOk = FDICopy(hfdi, cabName, cabDir, 0, fdi_notify, NULL, &ctx);
        if (!fdiOk)
            Log(L"FDICopy FAILED: erfOper=%d erfType=%d", erf.erfOper, erf.erfType);
        FDIDestroy(hfdi);
    } else {
        Log(L"FDICreate FAILED: erfOper=%d", erf.erfOper);
    }
    DeleteFileW(tempCab);

    if (!fdiOk) {
        DeleteDirRecursive(tempDir);
        if (g_hLog != INVALID_HANDLE_VALUE) CloseHandle(g_hLog);
        MessageBoxW(NULL, L"Failed to extract files from package.", L"nexpress", MB_ICONERROR);
        return 1;
    }
    Log(L"FDI extract done");

    // --- 6. Run AppLaunched ---
    WCHAR cmdLine[MAX_PATH * 2];
    StringCchCopyW(cmdLine, ARRAYSIZE(cmdLine), tempDir);
    StringCchCatW(cmdLine, ARRAYSIZE(cmdLine), L"\\");
    StringCchCatW(cmdLine, ARRAYSIZE(cmdLine), appW);
    Log(L"CreateProcess: %s", cmdLine);

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, tempDir, &si, &pi)) {
        DWORD err = GetLastError();
        Log(L"CreateProcess FAILED (err=%lu)", err);
        WCHAR msg[MAX_PATH + 64];
        StringCchPrintfW(msg, ARRAYSIZE(msg), L"Failed to launch %s (error %lu).", appW, err);
        DeleteDirRecursive(tempDir);
        if (g_hLog != INVALID_HANDLE_VALUE) CloseHandle(g_hLog);
        MessageBoxW(NULL, msg, L"nexpress", MB_ICONERROR);
        return 1;
    }
    Log(L"waiting for process...");
    if (g_hLog != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hLog);
        g_hLog = INVALID_HANDLE_VALUE;
    }

    DWORD exitCode = 1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // --- 7. Cleanup temp dir ---
    DeleteDirRecursive(tempDir);

    return (int)exitCode;
}
