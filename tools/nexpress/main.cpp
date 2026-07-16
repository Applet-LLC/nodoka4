/*
 * main.cpp — nexpress: IExpress-compatible SFX packager
 *
 * Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
 * License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/
 *
 * Usage: nexpress /N <sedfile>
 *
 * Reads an IExpress .SED file, creates a Cabinet from the listed source files
 * (LZX compression), embeds stub.exe from resources, and writes:
 *   [stub.exe][CAB][SFXTrailer]  → TargetName EXE
 *
 * No RPT file is written; no IExpress RPT path bug can occur.
 */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <fci.h>
#include <strsafe.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "resource.h"
#include "secure_temp.h"

#pragma comment(lib, "cabinet.lib")

// -----------------------------------------------------------------------
// SFXTrailer — must match stub.cpp exactly
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
// Secure work folder (CWE-377/59): every temp file lives under this
// random-named, owner-only-DACL directory.  Created at the top of wmain.
// -----------------------------------------------------------------------
static WCHAR g_secureDirW[MAX_PATH];
static CHAR  g_secureDirA[MAX_PATH];
static LONG  g_tempSeq;

// -----------------------------------------------------------------------
// FCI callbacks
// -----------------------------------------------------------------------
static FNFCIALLOC(fci_alloc) { return HeapAlloc(GetProcessHeap(), 0, cb); }
static FNFCIFREE(fci_free)   { HeapFree(GetProcessHeap(), 0, memory); }

static FNFCIOPEN(fci_open) {
    DWORD access = (oflag & _O_RDONLY) ? GENERIC_READ
                 : (oflag & _O_WRONLY) ? GENERIC_WRITE
                 : (GENERIC_READ | GENERIC_WRITE);
    DWORD share  = (oflag & _O_RDONLY) ? FILE_SHARE_READ : 0;
    // Honor O_EXCL: temp names are unique inside g_secureDirA, so CREATE_NEW
    // succeeding proves nobody planted a file or link there first (CWE-59).
    DWORD disp   = (oflag & _O_CREAT)
                 ? ((oflag & _O_EXCL) ? CREATE_NEW : CREATE_ALWAYS)
                 : OPEN_EXISTING;
    HANDLE h = CreateFileA(pszFile, access, share, NULL, disp,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { if (err) *err = (int)GetLastError(); return -1; }
    return (INT_PTR)h;
}
static FNFCIREAD(fci_read) {
    DWORD n = 0; ReadFile((HANDLE)hf, memory, cb, &n, NULL); return n;
}
static FNFCIWRITE(fci_write) {
    DWORD n = 0; WriteFile((HANDLE)hf, memory, cb, &n, NULL); return n;
}
static FNFCICLOSE(fci_close) { CloseHandle((HANDLE)hf); return 0; }
static FNFCISEEK(fci_seek)   {
    DWORD m = (seektype == SEEK_SET) ? FILE_BEGIN
            : (seektype == SEEK_CUR) ? FILE_CURRENT : FILE_END;
    return (long)SetFilePointer((HANDLE)hf, dist, NULL, m);
}
static FNFCIDELETE(fci_delete) { DeleteFileA(pszFile); return 0; }

static FNFCIFILEPLACED(fci_file_placed) { return 0; }

static FNFCIGETTEMPFILE(fci_get_temp_file) {
    // Unique name inside the secure work folder; fci_open creates it with
    // CREATE_NEW, so no placeholder (and no delete/re-create race) is needed.
    LONG n = InterlockedIncrement(&g_tempSeq);
    return SUCCEEDED(StringCchPrintfA(pszTempName, cbTempName,
                                      "%s\\fci%04ld.tmp", g_secureDirA, n));
}

static FNFCISTATUS(fci_status) { return 0; }

static FNFCIGETOPENINFO(fci_get_open_info) {
    HANDLE h = CreateFileA(pszName, GENERIC_READ, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;

    FILETIME ft;
    WORD dosDate, dosTime;
    GetFileTime(h, NULL, NULL, &ft);
    FileTimeToDosDateTime(&ft, &dosDate, &dosTime);
    *pdate = dosDate;
    *ptime = dosTime;
    *pattribs = 0;
    return (INT_PTR)h;
}

static FNFCIGETNEXTCABINET(fci_get_next_cabinet) {
    // Single-cabinet output; this callback should never be called.
    return FALSE;
}

// -----------------------------------------------------------------------
// Helper: convert wchar path to narrow (ACP)
// -----------------------------------------------------------------------
static void W2A(LPCWSTR w, LPSTR a, int alen) {
    WideCharToMultiByte(CP_ACP, 0, w, -1, a, alen, NULL, NULL);
}
static void A2W(LPCSTR a, LPWSTR w, int wlen) {
    MultiByteToWideChar(CP_ACP, 0, a, -1, w, wlen);
}

// -----------------------------------------------------------------------
// Parse SourceFiles0 section: "rel\path\to\file=" entries
// Extract key (relative path) from each "key=value" line.
// -----------------------------------------------------------------------
struct FileList {
    char **paths;     // absolute ANSI paths (source)
    char **destNames; // basename only (destination in CAB)
    int    count;
    int    cap;
};

static void fl_add(FileList *fl, const char *absSrc, const char *baseName) {
    if (fl->count >= fl->cap) {
        fl->cap = fl->cap ? fl->cap * 2 : 64;
        fl->paths     = (char**)HeapReAlloc(GetProcessHeap(), 0, fl->paths,
                                            fl->cap * sizeof(char*));
        fl->destNames = (char**)HeapReAlloc(GetProcessHeap(), 0, fl->destNames,
                                            fl->cap * sizeof(char*));
    }
    size_t sl = strlen(absSrc) + 1;
    size_t bl = strlen(baseName) + 1;
    fl->paths[fl->count] = (char*)HeapAlloc(GetProcessHeap(), 0, sl);
    fl->destNames[fl->count] = (char*)HeapAlloc(GetProcessHeap(), 0, bl);
    memcpy(fl->paths[fl->count], absSrc, sl);
    memcpy(fl->destNames[fl->count], baseName, bl);
    fl->count++;
}

static void fl_free(FileList *fl) {
    for (int i = 0; i < fl->count; i++) {
        HeapFree(GetProcessHeap(), 0, fl->paths[i]);
        HeapFree(GetProcessHeap(), 0, fl->destNames[i]);
    }
    if (fl->paths)     HeapFree(GetProcessHeap(), 0, fl->paths);
    if (fl->destNames) HeapFree(GetProcessHeap(), 0, fl->destNames);
}

// -----------------------------------------------------------------------
// Basename (last component after / or \)
// -----------------------------------------------------------------------
static const char *BasenameA(const char *path) {
    const char *p = path;
    for (const char *q = path; *q; q++)
        if (*q == '\\' || *q == '/') p = q + 1;
    return p;
}

// -----------------------------------------------------------------------
// Write all bytes from a HANDLE to another HANDLE, return bytes written
// -----------------------------------------------------------------------
static UINT64 CopyHandleToHandle(HANDLE hSrc, HANDLE hDst) {
    BYTE buf[65536];
    UINT64 total = 0;
    DWORD r, w;
    while (ReadFile(hSrc, buf, sizeof(buf), &r, NULL) && r) {
        WriteFile(hDst, buf, r, &w, NULL);
        total += w;
    }
    return total;
}

// -----------------------------------------------------------------------
// Error output
// -----------------------------------------------------------------------
static void Die(LPCWSTR fmt, ...) {
    WCHAR msg[512];
    va_list ap;
    va_start(ap, fmt);
    StringCchVPrintfW(msg, 512, fmt, ap);
    va_end(ap);
    fwprintf(stderr, L"nexpress: error: %s\n", msg);
    if (g_secureDirW[0]) SecureDeleteDirRecursive(g_secureDirW);
    ExitProcess(1);
}

// -----------------------------------------------------------------------
// wmain
// -----------------------------------------------------------------------
int wmain(int argc, wchar_t **argv) {

    // --- CWE-427: restrict DLL search to System32 before anything else ---
    HardenDllSearchPath();

    // --- 1. Parse /N <sedfile> ---
    if (argc < 3) {
        fwprintf(stderr,
            L"Usage: nexpress /N <sedfile>\n"
            L"  IExpress-compatible SFX packager (no RPT file written)\n");
        return 1;
    }
    if (_wcsicmp(argv[1], L"/N") && _wcsicmp(argv[1], L"-N")) {
        fwprintf(stderr, L"nexpress: unknown option '%s'\n", argv[1]);
        return 1;
    }
    LPCWSTR sedArg = argv[2];

    // Resolve SED path to absolute
    WCHAR absSed[MAX_PATH];
    if (!GetFullPathNameW(sedArg, MAX_PATH, absSed, NULL))
        Die(L"Cannot resolve SED path: %s", sedArg);

    // SED directory (with trailing backslash)
    WCHAR sedDir[MAX_PATH];
    StringCchCopyW(sedDir, MAX_PATH, absSed);
    WCHAR *lastSep = NULL;
    for (WCHAR *p = sedDir; *p; p++)
        if (*p == L'\\' || *p == L'/') lastSep = p;
    if (lastSep) *(lastSep + 1) = L'\0';
    else { StringCchCopyW(sedDir, MAX_PATH, L".\\"); }

    // --- 2. Read [Options] ---
    WCHAR targetNameW[MAX_PATH] = {0};
    WCHAR appLaunchedW[256]     = {0};
    WCHAR baseDirW[MAX_PATH]    = {0};

    GetPrivateProfileStringW(L"Options",     L"TargetName",    L"",    targetNameW,  MAX_PATH, absSed);
    GetPrivateProfileStringW(L"Options",     L"AppLaunched",   L"",    appLaunchedW, 256,      absSed);
    GetPrivateProfileStringW(L"SourceFiles", L"SourceFiles0",  L".\\", baseDirW,     MAX_PATH, absSed);

    if (!targetNameW[0]) Die(L"SED missing TargetName");
    if (!appLaunchedW[0]) Die(L"SED missing AppLaunched");

    // Strip leading ".\" from TargetName (geniexpress adds it for IExpress compat)
    LPCWSTR targetEffective = targetNameW;
    if (targetNameW[0] == L'.' && (targetNameW[1] == L'\\' || targetNameW[1] == L'/'))
        targetEffective = targetNameW + 2;

    // Resolve TargetName relative to sedDir (if not absolute)
    WCHAR targetAbsW[MAX_PATH];
    if (targetEffective[1] == L':' || targetEffective[0] == L'\\') {
        StringCchCopyW(targetAbsW, MAX_PATH, targetEffective);
    } else {
        StringCchCopyW(targetAbsW, MAX_PATH, sedDir);
        StringCchCatW(targetAbsW, MAX_PATH, targetEffective);
    }

    // Resolve baseDir relative to sedDir
    WCHAR resolvedBaseW[MAX_PATH];
    if (baseDirW[0] == L'.' && (baseDirW[1] == L'\\' || baseDirW[1] == L'/')) {
        StringCchCopyW(resolvedBaseW, MAX_PATH, sedDir);
        StringCchCatW(resolvedBaseW, MAX_PATH, baseDirW + 2);
    } else if (baseDirW[1] == L':' || baseDirW[0] == L'\\') {
        StringCchCopyW(resolvedBaseW, MAX_PATH, baseDirW);
    } else {
        StringCchCopyW(resolvedBaseW, MAX_PATH, sedDir);
        StringCchCatW(resolvedBaseW, MAX_PATH, baseDirW);
    }
    // Ensure trailing backslash
    size_t rlen = wcslen(resolvedBaseW);
    if (rlen && resolvedBaseW[rlen - 1] != L'\\')
        StringCchCatW(resolvedBaseW, MAX_PATH, L"\\");

    fwprintf(stdout, L"nexpress: Target    = %s\n", targetAbsW);
    fwprintf(stdout, L"nexpress: Launch    = %s\n", appLaunchedW);
    fwprintf(stdout, L"nexpress: BaseDir   = %s\n", resolvedBaseW);

    // --- 3. Read [SourceFiles0] and build file list ---
    // GetPrivateProfileSection returns "key=value\0key=value\0\0"
    WCHAR *secBuf = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 1 << 20);
    if (!secBuf) Die(L"Out of memory");
    DWORD secLen = GetPrivateProfileSectionW(L"SourceFiles0", secBuf, 1 << 19, absSed);
    if (!secLen) Die(L"SED [SourceFiles0] section is empty");

    FileList fl = {0};
    fl.paths     = (char**)HeapAlloc(GetProcessHeap(), 0, sizeof(char*));
    fl.destNames = (char**)HeapAlloc(GetProcessHeap(), 0, sizeof(char*));
    fl.cap = 1;

    for (WCHAR *p = secBuf; *p; p += wcslen(p) + 1) {
        // Each entry: "relpath=" or "relpath=destname"
        // Strip the "=" and everything after it to get the key (relative path)
        WCHAR entry[MAX_PATH];
        StringCchCopyW(entry, MAX_PATH, p);
        WCHAR *eq = wcschr(entry, L'=');
        if (eq) *eq = L'\0';
        if (!entry[0]) continue;

        // Resolve: resolvedBaseW + entry
        WCHAR srcAbsW[MAX_PATH];
        StringCchCopyW(srcAbsW, MAX_PATH, resolvedBaseW);
        StringCchCatW(srcAbsW, MAX_PATH, entry);
        // Normalize: GetFullPathName collapses "..\\"
        WCHAR normW[MAX_PATH];
        GetFullPathNameW(srcAbsW, MAX_PATH, normW, NULL);

        char normA[MAX_PATH];
        W2A(normW, normA, MAX_PATH);

        // Basename for CAB destination
        const char *bn = BasenameA(normA);

        fl_add(&fl, normA, bn);
    }
    HeapFree(GetProcessHeap(), 0, secBuf);

    // Reject duplicate destination (basename) collisions up front: the CAB
    // namespace is flat, so two source files sharing a basename (e.g. an
    // x86 and x64 build both named foo.pdb) would silently overwrite one
    // another in the CAB, or -- with the stub's CWE-59 CREATE_NEW extraction --
    // make extraction fail outright with a confusing ERROR_FILE_EXISTS.
    // Catch it at packaging time instead, with the two source paths named.
    for (int i = 0; i < fl.count; i++) {
        for (int j = i + 1; j < fl.count; j++) {
            if (!_stricmp(fl.destNames[i], fl.destNames[j]))
                Die(L"duplicate destination name '%hs':\n  %hs\n  %hs",
                    fl.destNames[i], fl.paths[i], fl.paths[j]);
        }
    }

    fwprintf(stdout, L"nexpress: %d files to package\n", fl.count);

    // --- 4. Create CAB inside a secure work folder (CWE-377/59) ---
    if (!CreateSecureTempDir(g_secureDirW, ARRAYSIZE(g_secureDirW)))
        Die(L"Cannot create secure temp dir (error=%d)", GetLastError());
    W2A(g_secureDirW, g_secureDirA, MAX_PATH);

    CHAR tempCabA[MAX_PATH];
    StringCchPrintfA(tempCabA, MAX_PATH, "%s\\payload.cab", g_secureDirA);

    // CCAB: cabinet configuration
    CCAB ccab;
    ZeroMemory(&ccab, sizeof(ccab));
    ccab.cb            = (ULONG)0x7FFFFFFF; // max cabinet size (effectively unlimited)
    ccab.cbFolderThresh = (ULONG)0x7FFFFFFF;
    ccab.iCab          = 1;
    ccab.iDisk         = 1;
    StringCchCopyA(ccab.szCab, ARRAYSIZE(ccab.szCab), "nexpress.cab");

    // Split tempCabA into dir + name for CCAB
    {
        const char *bn = BasenameA(tempCabA);
        StringCchCopyA(ccab.szCabPath, ARRAYSIZE(ccab.szCabPath), tempCabA);
        size_t dlen = (size_t)(bn - tempCabA);
        ccab.szCabPath[dlen] = '\0';
        StringCchCopyA(ccab.szCab, ARRAYSIZE(ccab.szCab), bn);
    }

    ERF erf = {0};
    HFCI hfci = FCICreate(&erf,
        fci_file_placed, fci_alloc, fci_free,
        fci_open, fci_read, fci_write, fci_close, fci_seek,
        fci_delete, fci_get_temp_file,
        &ccab, NULL);
    if (!hfci) Die(L"FCICreate failed (erf=%d)", erf.erfOper);

    for (int i = 0; i < fl.count; i++) {
        fwprintf(stdout, L"nexpress: adding %hs\n", fl.destNames[i]);
        if (!FCIAddFile(hfci,
                fl.paths[i],
                fl.destNames[i],
                FALSE,
                fci_get_next_cabinet,
                fci_status,
                fci_get_open_info,
                tcompTYPE_LZX | (0x15 << 8)))  // LZX window 2^21 (tcompLZX_WINDOW_HI)
        {
            FCIDestroy(hfci);
            Die(L"FCIAddFile failed for '%hs' (erf=%d)", fl.paths[i], erf.erfOper);
        }
    }

    if (!FCIFlushCabinet(hfci, FALSE, fci_get_next_cabinet, fci_status)) {
        FCIDestroy(hfci);
        Die(L"FCIFlushCabinet failed (erf=%d)", erf.erfOper);
    }
    FCIDestroy(hfci);
    fl_free(&fl);

    // cabinet.dll may create 0-byte fci*.tmp placeholder files in the CWD;
    // clean them up unconditionally now that FCI is done.
    {
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA("fci*.tmp", &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    DeleteFileA(fd.cFileName);
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }

    fwprintf(stdout, L"nexpress: CAB created: %hs\n", tempCabA);

    // --- 5. Get stub.exe from embedded resource ---
    HRSRC   hrsrc = FindResource(NULL, MAKEINTRESOURCE(IDR_STUB), RT_RCDATA);
    if (!hrsrc) Die(L"FindResource(IDR_STUB) failed (%d)", GetLastError());
    HGLOBAL hgbl  = LoadResource(NULL, hrsrc);
    if (!hgbl)  Die(L"LoadResource failed");
    LPVOID  pStub = LockResource(hgbl);
    DWORD   stubSize = SizeofResource(NULL, hrsrc);
    if (!pStub || !stubSize) Die(L"LockResource failed");

    // --- 6. Open CAB to get its size ---
    WCHAR tempCabW[MAX_PATH];
    A2W(tempCabA, tempCabW, MAX_PATH);
    HANDLE hCab = CreateFileW(tempCabW, GENERIC_READ, FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hCab == INVALID_HANDLE_VALUE) Die(L"Cannot open temp CAB: %s", tempCabW);

    LARGE_INTEGER cabSizeLI;
    GetFileSizeEx(hCab, &cabSizeLI);
    UINT64 cabSize = (UINT64)cabSizeLI.QuadPart;

    // --- 7. Write output EXE: stub + CAB + trailer ---
    HANDLE hOut = CreateFileW(targetAbsW, GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOut == INVALID_HANDLE_VALUE)
        Die(L"Cannot create output EXE: %s (error=%d)", targetAbsW, GetLastError());

    // a) stub bytes
    DWORD written = 0;
    WriteFile(hOut, pStub, stubSize, &written, NULL);
    UINT64 cabOffset = (UINT64)stubSize;

    // b) CAB bytes
    SetFilePointer(hCab, 0, NULL, FILE_BEGIN);
    CopyHandleToHandle(hCab, hOut);
    CloseHandle(hCab);

    // c) SFXTrailer
    SFXTrailer trailer;
    ZeroMemory(&trailer, sizeof(trailer));
    trailer.magic     = NEXPRESS_MAGIC;
    trailer.cabOffset = cabOffset;
    trailer.cabSize   = cabSize;
    // AppLaunched stored as ANSI (stub.cpp uses MultiByteToWideChar to decode)
    W2A(appLaunchedW, trailer.appLaunched, sizeof(trailer.appLaunched));

    WriteFile(hOut, &trailer, sizeof(trailer), &written, NULL);
    CloseHandle(hOut);

    // --- 8. Cleanup (removes the CAB and all fci*.tmp with it) ---
    SecureDeleteDirRecursive(g_secureDirW);

    fwprintf(stdout, L"nexpress: done -> %s\n", targetAbsW);
    return 0;
}
