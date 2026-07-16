/*
 * secure_temp.h -- secure work-folder helpers shared by nexpress.exe and stub.exe
 *
 * Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
 * License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/
 *
 * CWE-377 / CWE-59 mitigation, built on three guarantees:
 *   1. directory names contain 128 bits of crypto randomness (not guessable)
 *   2. the directory must be created fresh by us -- an existing directory,
 *      junction or symlink is never adopted
 *   3. a protected DACL grants access to SYSTEM and the launching user only
 * Plus: recursive deletion never follows reparse points, so a junction
 * planted inside the work folder cannot redirect the cleanup elsewhere.
 */

#pragma once

#include <windows.h>
#include <sddl.h>
#include <strsafe.h>

#pragma comment(lib, "advapi32.lib")

#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif

// RtlGenRandom (advapi32); no SDK header declares it under this export name.
extern "C" BOOLEAN WINAPI SystemFunction036(PVOID RandomBuffer, ULONG RandomBufferLength);

// -----------------------------------------------------------------------
// Harden the DLL search path (CWE-427).  Call once at process entry.
//   - SetDefaultDllDirectories(SYSTEM32): later LoadLibrary resolves from
//     System32 only, not the app dir / CWD / PATH.
//   - SetDllDirectory(""): drops the current directory from the search set.
// Both APIs live in kernel32 (a KnownDLL), so they are already safely loaded.
// Implicitly-linked DLLs (loaded before entry) are covered separately by the
// linker option /DEPENDENTLOADFLAG:0x800 in each vcxproj.
// -----------------------------------------------------------------------
static void HardenDllSearchPath(void) {
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    if (k32) {
        typedef BOOL (WINAPI *PFN_SDDD)(DWORD);
        PFN_SDDD pSetDefaultDllDirectories =
            (PFN_SDDD)GetProcAddress(k32, "SetDefaultDllDirectories");
        if (pSetDefaultDllDirectories)
            pSetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    SetDllDirectoryW(L"");
}

// -----------------------------------------------------------------------
// 16 crypto-random bytes -> 32 hex chars.  cch must be >= 33.
// -----------------------------------------------------------------------
static BOOL RandomHexName(WCHAR *buf, size_t cch) {
    BYTE rnd[16];
    if (cch < ARRAYSIZE(rnd) * 2 + 1) return FALSE;
    if (!SystemFunction036(rnd, sizeof(rnd))) return FALSE;
    static const WCHAR hex[] = L"0123456789abcdef";
    for (int i = 0; i < ARRAYSIZE(rnd); i++) {
        buf[i * 2]     = hex[rnd[i] >> 4];
        buf[i * 2 + 1] = hex[rnd[i] & 0xF];
    }
    buf[ARRAYSIZE(rnd) * 2] = L'\0';
    return TRUE;
}

// -----------------------------------------------------------------------
// Security descriptor: protected DACL (no inherited ACEs), full control
// for SYSTEM and the launching user only.  Caller frees with LocalFree.
// -----------------------------------------------------------------------
static PSECURITY_DESCRIPTOR BuildOwnerOnlySD(void) {
    PSECURITY_DESCRIPTOR sd = NULL;
    HANDLE hTok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hTok))
        return NULL;

    BYTE tuBuf[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
    DWORD len = 0;
    if (GetTokenInformation(hTok, TokenUser, tuBuf, sizeof(tuBuf), &len)) {
        LPWSTR sidStr = NULL;
        if (ConvertSidToStringSidW(((TOKEN_USER *)tuBuf)->User.Sid, &sidStr)) {
            WCHAR sddl[256];
            // D:P = protected DACL; OICI = children inherit; FA = full access
            if (SUCCEEDED(StringCchPrintfW(sddl, ARRAYSIZE(sddl),
                    L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;%s)", sidStr))) {
                ConvertStringSecurityDescriptorToSecurityDescriptorW(
                    sddl, SDDL_REVISION_1, &sd, NULL);
            }
            LocalFree(sidStr);
        }
    }
    CloseHandle(hTok);
    return sd;
}

// -----------------------------------------------------------------------
// Create %TEMP%\nexpress_<32 hex> with the owner-only DACL.
// CreateDirectoryW succeeding proves the final component was created by us
// this instant -- it cannot be a pre-planted directory, junction or symlink.
// ERROR_ALREADY_EXISTS retries with a new random name; never reuses.
// -----------------------------------------------------------------------
static BOOL CreateSecureTempDir(WCHAR *outPath, size_t cch) {
    WCHAR tempRoot[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, tempRoot)) return FALSE;

    PSECURITY_DESCRIPTOR sd = BuildOwnerOnlySD();
    if (!sd) return FALSE;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), sd, FALSE };

    BOOL ok = FALSE;
    for (int attempt = 0; attempt < 10; attempt++) {
        WCHAR name[33];
        if (!RandomHexName(name, ARRAYSIZE(name))) break;
        if (FAILED(StringCchPrintfW(outPath, cch, L"%snexpress_%s",
                                    tempRoot, name))) break;
        if (CreateDirectoryW(outPath, &sa)) { ok = TRUE; break; }
        if (GetLastError() != ERROR_ALREADY_EXISTS) break;
        // 128-bit collision is practically an attack; pick another name
    }
    LocalFree(sd);
    return ok;
}

// -----------------------------------------------------------------------
// Recursive delete that does NOT follow reparse points: a junction or
// directory symlink is removed as a link (RemoveDirectoryW) so its target
// stays untouched; DeleteFileW likewise removes file symlinks themselves.
// -----------------------------------------------------------------------
static void SecureDeleteDirRecursive(LPCWSTR dir) {
    WCHAR pat[MAX_PATH];
    if (FAILED(StringCchCopyW(pat, MAX_PATH, dir)) ||
        FAILED(StringCchCatW(pat, MAX_PATH, L"\\*")))
        return;

    WIN32_FIND_DATAW fd;
    HANDLE hf = FindFirstFileW(pat, &fd);
    if (hf != INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
            WCHAR child[MAX_PATH];
            if (FAILED(StringCchCopyW(child, MAX_PATH, dir)) ||
                FAILED(StringCchCatW(child, MAX_PATH, L"\\")) ||
                FAILED(StringCchCatW(child, MAX_PATH, fd.cFileName)))
                continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                    RemoveDirectoryW(child);   // unlink only; never recurse into it
                else
                    SecureDeleteDirRecursive(child);
            } else {
                DeleteFileW(child);
            }
        } while (FindNextFileW(hf, &fd));
        FindClose(hf);
    }
    RemoveDirectoryW(dir);
}
