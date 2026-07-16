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
 * On launch: extracts the embedded CAB to a secure temp dir (random name,
 * owner-only DACL) and runs AppLaunched.
 *
 * wextract-compatible options:
 *   /T:<full path>  extract into the given folder instead of the temp dir
 *   /C              extract only, do not run AppLaunched; when /T is omitted,
 *                   a folder-browse dialog prompts for the destination
 */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <fdi.h>
#include <strsafe.h>
#include <fcntl.h>

#include "..\secure_temp.h"

#pragma comment(lib, "cabinet.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

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
// Command line (wextract-compatible subset): /C  /T:<full path>
// -----------------------------------------------------------------------
struct Options {
    BOOL  extractOnly;          // /C : extract, do not run AppLaunched
    WCHAR targetDir[MAX_PATH];  // /T:<path>, empty = default temp folder
};

// Returns FALSE on an unrecognized argument (copied to badArg).
static BOOL ParseCommandLine(Options *opt, WCHAR *badArg, size_t badCch) {
    ZeroMemory(opt, sizeof(*opt));
    badArg[0] = L'\0';

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return TRUE;

    BOOL ok = TRUE;
    for (int i = 1; i < argc; i++) {
        LPCWSTR a = argv[i];
        if (a[0] == L'/' || a[0] == L'-') {
            if ((a[1] == L'C' || a[1] == L'c') && a[2] == L'\0') {
                opt->extractOnly = TRUE;
                continue;
            }
            if ((a[1] == L'T' || a[1] == L't') && a[2] == L':' && a[3] != L'\0') {
                StringCchCopyW(opt->targetDir, MAX_PATH, a + 3);
                continue;
            }
        }
        StringCchCopyW(badArg, badCch, a);
        ok = FALSE;
        break;
    }
    LocalFree(argv);
    return ok;
}

static void ShowUsage(LPCWSTR badArg) {
    WCHAR msg[512];
    if (badArg && badArg[0])
        StringCchPrintfW(msg, ARRAYSIZE(msg), L"Unknown option: %s\n\n", badArg);
    else
        msg[0] = L'\0';
    StringCchCatW(msg, ARRAYSIZE(msg),
        L"Usage: <package> [/T:<full path>] [/C]\n"
        L"/T:<full path> -- Specifies working folder to extract into\n"
        L"/C -- Extract files only (prompts for a folder when /T is omitted)");
    MessageBoxW(NULL, msg, L"nexpress", MB_ICONERROR);
}

// Folder-browse dialog for "/C" without "/T:" (IExpress-compatible).
// Returns FALSE when the user cancels.
static BOOL BrowseForExtractFolder(WCHAR *out, size_t cch) {
    HRESULT hrCo = CoInitializeEx(NULL,
        COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    BROWSEINFOW bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = NULL;
    bi.lpszTitle = L"Select the folder to extract the files into:";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;

    BOOL ok = FALSE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        WCHAR path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            StringCchCopyW(out, cch, path);
            ok = TRUE;
        }
        CoTaskMemFree(pidl);
    }
    if (SUCCEEDED(hrCo)) CoUninitialize();
    return ok;
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
struct ExtractCtx {
    WCHAR  destDir[MAX_PATH];
    BOOL   userDest;    // /T: folder chosen by the user (overwrite allowed)
    WCHAR **files;      // full paths of extracted files (for /T-mode cleanup)
    int    fileCount;
    int    fileCap;
};

static void ctx_track(ExtractCtx *ctx, LPCWSTR path) {
    if (ctx->fileCount >= ctx->fileCap) {
        int cap = ctx->fileCap ? ctx->fileCap * 2 : 32;
        WCHAR **p = ctx->files
            ? (WCHAR **)HeapReAlloc(GetProcessHeap(), 0, ctx->files, cap * sizeof(WCHAR *))
            : (WCHAR **)HeapAlloc(GetProcessHeap(), 0, cap * sizeof(WCHAR *));
        if (!p) return;
        ctx->files   = p;
        ctx->fileCap = cap;
    }
    size_t bytes = (wcslen(path) + 1) * sizeof(WCHAR);
    WCHAR *copy = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, bytes);
    if (!copy) return;
    memcpy(copy, path, bytes);
    ctx->files[ctx->fileCount++] = copy;
}

// Free the tracked list; when deleteFiles, also delete the files themselves
// (used for /T: folders, which must never be deleted recursively as a whole).
static void ctx_cleanup(ExtractCtx *ctx, BOOL deleteFiles) {
    for (int i = 0; i < ctx->fileCount; i++) {
        if (deleteFiles) DeleteFileW(ctx->files[i]);
        HeapFree(GetProcessHeap(), 0, ctx->files[i]);
    }
    if (ctx->files) HeapFree(GetProcessHeap(), 0, ctx->files);
    ctx->files = NULL;
    ctx->fileCount = ctx->fileCap = 0;
}

static FNFDINOTIFY(fdi_notify) {
    switch (fdint) {
    case fdintCOPY_FILE: {
        ExtractCtx *ctx = (ExtractCtx *)pfdin->pv;

        // Reject entry names that could escape destDir (Zip-Slip / CWE-22)
        if (!pfdin->psz1[0] ||
            strchr(pfdin->psz1, '\\') || strchr(pfdin->psz1, '/') ||
            strchr(pfdin->psz1, ':')  || strstr(pfdin->psz1, "..")) {
            Log(L"  REJECTED unsafe entry name: %hs", pfdin->psz1);
            return -1;
        }

        WCHAR wname[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, pfdin->psz1, -1, wname, MAX_PATH);
        Log(L"  extract: %s", wname);

        WCHAR dest[MAX_PATH];
        StringCchCopyW(dest, MAX_PATH, ctx->destDir);
        size_t dlen = wcslen(dest);
        if (dlen && dest[dlen-1] != L'\\') StringCchCatW(dest, MAX_PATH, L"\\");
        StringCchCatW(dest, MAX_PATH, wname);

        // CREATE_NEW: never follow or overwrite a pre-planted link (CWE-59).
        // In a user-chosen /T: folder an old copy may legitimately exist;
        // remove it (unlinks a symlink itself) so CREATE_NEW still succeeds.
        if (ctx->userDest) DeleteFileW(dest);
        HANDLE h = CreateFileW(dest, GENERIC_WRITE, 0, NULL,
                               CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            Log(L"  CreateFile FAILED for %s (err=%lu)", dest, GetLastError());
            return -1;
        }
        ctx_track(ctx, dest);
        return (INT_PTR)h;
    }
    case fdintCLOSE_FILE_INFO:
        CloseHandle((HANDLE)pfdin->hf);
        return TRUE;
    default:
        return 0;
    }
}

// -----------------------------------------------------------------------
// WinMain
// -----------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

    // --- CWE-427: restrict DLL search to System32 before anything else ---
    HardenDllSearchPath();

    // --- 0. Parse command line ---
    Options opt;
    WCHAR badArg[64];
    BOOL argsOk = ParseCommandLine(&opt, badArg, ARRAYSIZE(badArg));

    // --- 1. Open self ---
    WCHAR selfPath[MAX_PATH];
    GetModuleFileNameW(NULL, selfPath, MAX_PATH);

    OpenLog(selfPath);
    Log(L"nexpress stub start");
    Log(L"self: %s", selfPath);
    Log(L"mode: extractOnly=%d targetDir=%s",
        opt.extractOnly, opt.targetDir[0] ? opt.targetDir : L"(default)");

    if (!argsOk) {
        Log(L"FAILED: unknown option '%s'", badArg);
        if (g_hLog != INVALID_HANDLE_VALUE) CloseHandle(g_hLog);
        ShowUsage(badArg);
        return 1;
    }
    if (opt.extractOnly && !opt.targetDir[0]) {
        // IExpress-compatible: /C without /T: prompts for the destination folder
        Log(L"/C without /T: prompting for folder");
        if (!BrowseForExtractFolder(opt.targetDir, ARRAYSIZE(opt.targetDir))) {
            Log(L"folder selection cancelled");
            if (g_hLog != INVALID_HANDLE_VALUE) CloseHandle(g_hLog);
            return 0;   // user cancelled: nothing to extract
        }
        Log(L"selected folder: %s", opt.targetDir);
    }

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

    // --- 3. Build work folders ---
    // CWE-377/59: random name, freshly created by us, owner-only DACL.
    WCHAR secureRoot[MAX_PATH];
    if (!CreateSecureTempDir(secureRoot, ARRAYSIZE(secureRoot))) {
        CloseHandle(hSelf);
        Log(L"FAILED: cannot create secure temp dir (err=%lu)", GetLastError());
        MessageBoxW(NULL, L"Cannot create temporary folder.", L"nexpress", MB_ICONERROR);
        return 1;
    }

    WCHAR tempCab[MAX_PATH];
    StringCchPrintfW(tempCab, MAX_PATH, L"%s\\payload.cab", secureRoot);

    WCHAR destDir[MAX_PATH];
    BOOL destCreatedByUs = FALSE;
    if (opt.targetDir[0]) {
        // /T: user-chosen folder; existing folder is the user's decision
        if (!GetFullPathNameW(opt.targetDir, MAX_PATH, destDir, NULL)) {
            CloseHandle(hSelf);
            SecureDeleteDirRecursive(secureRoot);
            Log(L"FAILED: cannot resolve /T: path (err=%lu)", GetLastError());
            MessageBoxW(NULL, L"Invalid /T: folder path.", L"nexpress", MB_ICONERROR);
            return 1;
        }
        if (CreateDirectoryW(destDir, NULL)) {
            destCreatedByUs = TRUE;
        } else if (GetLastError() != ERROR_ALREADY_EXISTS) {
            CloseHandle(hSelf);
            SecureDeleteDirRecursive(secureRoot);
            Log(L"FAILED: cannot create /T: folder (err=%lu)", GetLastError());
            MessageBoxW(NULL, L"Cannot create /T: folder.", L"nexpress", MB_ICONERROR);
            return 1;
        }
    } else {
        // inherits the owner-only DACL from secureRoot (OICI ACEs)
        StringCchPrintfW(destDir, MAX_PATH, L"%s\\out", secureRoot);
        if (!CreateDirectoryW(destDir, NULL)) {
            CloseHandle(hSelf);
            SecureDeleteDirRecursive(secureRoot);
            Log(L"FAILED: cannot create extract dir (err=%lu)", GetLastError());
            MessageBoxW(NULL, L"Cannot create temporary folder.", L"nexpress", MB_ICONERROR);
            return 1;
        }
    }

    Log(L"secureRoot: %s", secureRoot);
    Log(L"tempCab: %s", tempCab);
    Log(L"destDir: %s", destDir);

    // --- 4. Copy CAB bytes from self to temp file ---
    LARGE_INTEGER cabPos;
    cabPos.QuadPart = (LONGLONG)trailer.cabOffset;
    SetFilePointerEx(hSelf, cabPos, NULL, FILE_BEGIN);

    // CREATE_NEW inside our fresh secureRoot: existing file = planted link (CWE-59)
    HANDLE hCab = CreateFileW(tempCab, GENERIC_WRITE, 0, NULL,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hCab == INVALID_HANDLE_VALUE) {
        CloseHandle(hSelf);
        SecureDeleteDirRecursive(secureRoot);
        if (destCreatedByUs) RemoveDirectoryW(destDir);
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
    ExtractCtx ctx;
    ZeroMemory(&ctx, sizeof(ctx));
    StringCchCopyW(ctx.destDir, MAX_PATH, destDir);
    ctx.userDest = (opt.targetDir[0] != L'\0');

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

        fdiOk = FDICopy(hfdi, cabName, cabDir, 0, fdi_notify, NULL, &ctx);
        if (!fdiOk)
            Log(L"FDICopy FAILED: erfOper=%d erfType=%d", erf.erfOper, erf.erfType);
        FDIDestroy(hfdi);
    } else {
        Log(L"FDICreate FAILED: erfOper=%d", erf.erfOper);
    }
    DeleteFileW(tempCab);

    if (!fdiOk) {
        // /T: folder is user-owned: remove only what we extracted
        ctx_cleanup(&ctx, ctx.userDest);
        if (destCreatedByUs) RemoveDirectoryW(destDir);
        SecureDeleteDirRecursive(secureRoot);
        if (g_hLog != INVALID_HANDLE_VALUE) CloseHandle(g_hLog);
        MessageBoxW(NULL, L"Failed to extract files from package.", L"nexpress", MB_ICONERROR);
        return 1;
    }
    Log(L"FDI extract done");

    // --- 6a. /C: extract-only mode ends here (keep destDir) ---
    if (opt.extractOnly) {
        Log(L"extract-only (/C): %d files extracted to %s", ctx.fileCount, destDir);
        ctx_cleanup(&ctx, FALSE);
        SecureDeleteDirRecursive(secureRoot);
        if (g_hLog != INVALID_HANDLE_VALUE) CloseHandle(g_hLog);
        return 0;
    }

    // --- 6b. Run AppLaunched ---
    // CWE-428/427: pass the full exe path as lpApplicationName and quote
    // argv[0], so a space in the work-folder path can never be misparsed
    // into launching an unintended binary.
    WCHAR appPath[MAX_PATH];
    StringCchCopyW(appPath, ARRAYSIZE(appPath), destDir);
    StringCchCatW(appPath, ARRAYSIZE(appPath), L"\\");
    StringCchCatW(appPath, ARRAYSIZE(appPath), appW);

    WCHAR cmdLine[MAX_PATH * 2];
    StringCchPrintfW(cmdLine, ARRAYSIZE(cmdLine), L"\"%s\"", appPath);
    Log(L"CreateProcess: app=%s cmd=%s", appPath, cmdLine);

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(appPath, cmdLine, NULL, NULL, FALSE, 0, NULL, destDir, &si, &pi)) {
        DWORD err = GetLastError();
        Log(L"CreateProcess FAILED (err=%lu)", err);
        WCHAR msg[MAX_PATH + 64];
        StringCchPrintfW(msg, ARRAYSIZE(msg), L"Failed to launch %s (error %lu).", appW, err);
        ctx_cleanup(&ctx, ctx.userDest);
        if (destCreatedByUs) RemoveDirectoryW(destDir);
        SecureDeleteDirRecursive(secureRoot);
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

    // --- 7. Cleanup ---
    if (ctx.userDest) {
        // /T: folder is user-owned: delete only the files we extracted;
        // remove the folder itself only if we created it (and it is empty)
        ctx_cleanup(&ctx, TRUE);
        if (destCreatedByUs) RemoveDirectoryW(destDir);
    } else {
        ctx_cleanup(&ctx, FALSE);   // destDir lives under secureRoot
    }
    SecureDeleteDirRecursive(secureRoot);

    return (int)exitCode;
}
