///////////////////////////////////////////////////////////////////////////////
// setup.cpp
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#include "..\nodoka\misc.h"
#include "..\nodoka\registry.h"
#include "..\nodoka\stringtool.h"
#include "..\nodoka\windowstool.h"
#include "installer.h"

#include <shlobj.h>
#include <sys/types.h>
#include <sys/stat.h>

// g_destDir is the install destination directory, defined in setup.cpp (global scope)
extern tstringi g_destDir;

namespace Installer
{
using namespace std;

/////////////////////////////////////////////////////////////////////////////
// Utility Functions

/** createLink
	uses the shell's IShellLink and IPersistFile interfaces to
	create and store a shortcut to the specified object.
	@return
	the result of calling the member functions of the interfaces.
	@param i_pathObj
	address of a buffer containing the path of the object.
	@param i_pathLink
	address of a buffer containing the path where the
	shell link is to be stored.
	@param i_desc
	address of a buffer containing the description of the
	shell link.
	*/
HRESULT createLink(LPCTSTR i_pathObj, LPCTSTR i_pathLink, LPCTSTR i_desc,
				   LPCTSTR i_workingDirectory, int argMK)
{
	// Get a pointer to the IShellLink interface.
	IShellLink *psl;
	HRESULT hres =
		CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
						 IID_IShellLink, (void **)&psl);
	if (SUCCEEDED(hres))
	{
		// Set the path to the shortcut target and add the description.
		// 0: none, bit1: -m, bit2: -k, bit3: -n

		psl->SetPath(i_pathObj);
		psl->SetDescription(i_desc);

		if (argMK == 1)
			psl->SetArguments(L"-m");
		if (argMK == 2)
			psl->SetArguments(L"-k");
		if (argMK == 3)
			psl->SetArguments(L"-k -m");
		if (argMK == 4)
			psl->SetArguments(L"-n");
		if (argMK == 5)
			psl->SetArguments(L"-m -n");
		if (argMK == 6)
			psl->SetArguments(L"-k -n");
		if (argMK == 7)
			psl->SetArguments(L"-k -m -n");
		if (argMK == 8)
			psl->SetArguments(L"-w");
		if (argMK == 9)
			psl->SetArguments(L"-m -w");
		if (argMK == 10)
			psl->SetArguments(L"-k -w");
		if (argMK == 11)
			psl->SetArguments(L"-k -m -w");
		if (argMK == 12)
			psl->SetArguments(L"-n -w");
		if (argMK == 13)
			psl->SetArguments(L"-m -n -w");
		if (argMK == 14)
			psl->SetArguments(L"-k -n -w");
		if (argMK == 15)
			psl->SetArguments(L"-k -m -n -w");

		if (i_workingDirectory)
			psl->SetWorkingDirectory(i_workingDirectory);

		// Query IShellLink for the IPersistFile interface for saving the
		// shortcut in persistent storage.
		IPersistFile *ppf;
		hres = psl->QueryInterface(IID_IPersistFile, (void **)&ppf);

		if (SUCCEEDED(hres))
		{
#ifdef UNICODE
			// Save the link by calling IPersistFile::Save.
			hres = ppf->Save(i_pathLink, TRUE);
#else
			wchar_t wsz[MAX_PATH];
			// Ensure that the string is ANSI.
			MultiByteToWideChar(CP_ACP, 0, i_pathLink, -1, wsz, MAX_PATH);
			// Save the link by calling IPersistFile::Save.
			hres = ppf->Save(wsz, TRUE);
#endif
			ppf->Release();
		}
		psl->Release();
	}
	return hres;
}

// create file extension information
void createFileExtension(const tstringi &i_ext, const tstring &i_contentType,
						 const tstringi &i_fileType,
						 const tstring &i_fileTypeName,
						 const tstringi &i_iconPath,
						 const tstring &i_command)
{
	tstring dummy;

	Registry regExt(HKEY_CLASSES_ROOT, i_ext);
	if (!regExt.read(_T(""), &dummy))
		CHECK_TRUE(regExt.write(_T(""), i_fileType));
	if (!regExt.read(_T("Content Type"), &dummy))
		CHECK_TRUE(regExt.write(_T("Content Type"), i_contentType));

	Registry regFileType(HKEY_CLASSES_ROOT, i_fileType);
	if (!regFileType.read(_T(""), &dummy))
		CHECK_TRUE(regFileType.write(_T(""), i_fileTypeName));

	Registry regFileTypeIcon(HKEY_CLASSES_ROOT,
							 i_fileType + _T("\\DefaultIcon"));
	if (!regFileTypeIcon.read(_T(""), &dummy))
		CHECK_TRUE(regFileTypeIcon.write(_T(""), i_iconPath));

	Registry regFileTypeComand(HKEY_CLASSES_ROOT,
							   i_fileType + _T("\\shell\\open\\command"));
	if (!regFileTypeComand.read(_T(""), &dummy))
		CHECK_TRUE(regFileTypeComand.write(_T(""), i_command));

	// Workaround remove old registry. because nodoka file is not use by ftype command.
	Registry::remove(HKEY_CLASSES_ROOT, _T("mayu file\\DefaultIcon"));
	Registry::remove(HKEY_CLASSES_ROOT, _T("mayu file"));
	Registry::remove(HKEY_CLASSES_ROOT, _T("nodoka file\\DefaultIcon"));
	Registry::remove(HKEY_CLASSES_ROOT, _T("nodoka file"));
}

// remove file extension information
void removeFileExtension(const tstringi &i_ext, const tstringi &i_fileType)
{
	Registry::remove(HKEY_CLASSES_ROOT, i_ext);
	Registry::remove(HKEY_CLASSES_ROOT,
					 i_fileType + _T("\\shell\\open\\command"));
	Registry::remove(HKEY_CLASSES_ROOT, i_fileType + _T("\\shell\\open"));
	Registry::remove(HKEY_CLASSES_ROOT, i_fileType + _T("\\shell"));
	Registry::remove(HKEY_CLASSES_ROOT, i_fileType + _T("\\DefaultIcon"));
	Registry::remove(HKEY_CLASSES_ROOT, i_fileType);
}

// create uninstallation information
void createUninstallInformation(const tstringi &i_name,
								const tstring &i_displayName,
								const tstring &i_commandLine)
{
	Registry reg(
		HKEY_LOCAL_MACHINE,
		_T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\") + i_name);

	CHECK_TRUE(reg.write(_T("DisplayName"), i_displayName));
	CHECK_TRUE(reg.write(_T("UninstallString"), i_commandLine));
}

// remove uninstallation information
void removeUninstallInformation(const tstringi &i_name)
{
	Registry::
		remove(HKEY_LOCAL_MACHINE,
			   _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\") + i_name);
}

// enable "Last Known Good Configuration" (前回正常起動時の構成)
void enableLastKnownGoodConfiguration()
{
	Registry regCm(
		HKEY_LOCAL_MACHINE,
		_T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Configuration Manager"));
	CHECK_TRUE(regCm.write(_T("BackupCount"), DWORD(2)));

	Registry regLkg(
		HKEY_LOCAL_MACHINE,
		_T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Configuration Manager\\LastKnownGood"));
	CHECK_TRUE(regLkg.write(_T("Enabled"), DWORD(1)));
}

// normalize path
tstringi normalizePath(tstringi i_path)
{
	tregex regSlash(_T("^(.*)/(.*)$"));
	tsmatch what;
	while (boost::regex_search(i_path, what, regSlash))
		i_path = what.str(1) + _T("\\") + what.str(2);

	tregex regTailBackSlash(_T("^(.*)\\\\$"));
	while (boost::regex_search(i_path, what, regTailBackSlash))
		i_path = what.str(1);

	return i_path;
}

// create deep directory
bool createDirectories(const _TCHAR *i_folder)
{
	const _TCHAR *s = _tcschr(i_folder, _T('\\')); // TODO: '/'
	if (s && s - i_folder == 2 && i_folder[1] == _T(':'))
		s = _tcschr(s + 1, _T('\\'));

	struct _stat sbuf;
	while (s)
	{
		tstringi f(i_folder, 0, s - i_folder);
		if (_tstat(f.c_str(), &sbuf) < 0)
			if (!CreateDirectory(f.c_str(), NULL))
				return false;
		s = _tcschr(s + 1, _T('\\'));
	}
	if (_tstat(i_folder, &sbuf) < 0)
		if (!CreateDirectory(i_folder, NULL))
			return false;
	return true;
}

// get driver directory
tstringi getDriverDirectory()
{
	_TCHAR buf[GANA_MAX_PATH];
	CHECK_TRUE(GetSystemDirectory(buf, NUMBER_OF(buf)));
	return tstringi(buf) + _T("\\drivers");
}

// get current directory
tstringi getModuleDirectory()
{
	_TCHAR buf[GANA_MAX_PATH];
	CHECK_TRUE(GetModuleFileName(g_hInst, buf, NUMBER_OF(buf)));
	tregex reg(_T("^(.*)\\\\[^\\\\]*$"));
	tsmatch what;
	tstringi path(buf);
	if (boost::regex_search(path, what, reg))
		return what.str(1);
	else
		return path;
}

// get start menu name
tstringi getStartMenuName(const tstringi &i_shortcutName)
{
#if 1
	_TCHAR buf[GANA_MAX_PATH];
	if (SUCCEEDED(SHGetSpecialFolderPathW(NULL, buf,
										  CSIDL_COMMON_PROGRAMS, FALSE)))
		return tstringi(buf) + _T("\\") + i_shortcutName + _T(".lnk");
#else
	tstringi programDir;
	if (Registry::read(HKEY_LOCAL_MACHINE,
					   _T("Software\\Microsoft\\Windows\\CurrentVersion\\")
					   _T("Explorer\\Shell Folders"),
					   _T("Common Programs"),
					   &programDir))
		return programDir + _T("\\") + i_shortcutName + _T(".lnk");
#endif
	return _T("");
}

// get start up name
tstringi getStartUpName(const tstringi &i_shortcutName)
{
#if 1
	_TCHAR buf[GANA_MAX_PATH];
	if (SUCCEEDED(SHGetSpecialFolderPath(NULL, buf,
										 CSIDL_STARTUP, FALSE)))
		return tstringi(buf) + _T("\\") + i_shortcutName + _T(".lnk");
#else
	tstringi startupDir;
	if (Registry::read(HKEY_CURRENT_USER,
					   _T("Software\\Microsoft\\Windows\\CurrentVersion\\")
					   _T("Explorer\\Shell Folders"),
					   _T("Startup"),
					   &startupDir))
		return startupDir + _T("\\") + i_shortcutName + _T(".lnk");
#endif
	return _T("");
}

// get DeskTopName
tstringi getDeskTopName(const tstringi &i_shortcutName)
{
#if 1
	_TCHAR buf[GANA_MAX_PATH];
	if (SUCCEEDED(SHGetSpecialFolderPath(NULL, buf,
										 CSIDL_DESKTOP, FALSE)))
		return tstringi(buf) + _T("\\") + i_shortcutName + _T(".lnk");
#else
	tstringi desktopDir;
	if (Registry::read(HKEY_CURRENT_USER,
					   _T("Software\\Microsoft\\Windows\\CurrentVersion\\")
					   _T("Explorer\\Shell Folders"),
					   _T("Desktop"),
					   &desktopDir))
		return desktopDir + _T("\\") + i_shortcutName + _T(".lnk");
#endif
	return _T("");
}

#if defined(_WINNT)

// Last captured stdout+stderr from DriverManager, for display in error dialogs.
static tstringi g_driverManagerLastOutput;

tstringi getDriverManagerLastOutput()
{
	return g_driverManagerLastOutput;
}

// launch DriverManager.exe from the install directory and wait for it to complete
// DriverManager.exe uses GetExecutablePath() to locate nodokad\nodokad.inf relative
// to itself, so it must be run from the directory where the driver package was installed.
static DWORD runDriverManager(const tstringi &i_arg)
{
	// After installFiles(), DriverManager.exe is in g_destDir.
	// Running it from there ensures GetExecutablePath() resolves nodokad\nodokad.inf
	// relative to the install directory, not the SFX temp directory.
	const tstringi &installDir = g_destDir;
#ifdef _WIN64
	tstringi exe = installDir + _T("\\DriverManager64.exe");
#else
	tstringi exe = installDir + _T("\\DriverManager.exe");
#endif

	g_driverManagerLastOutput.clear();

	// Create a pipe to capture DriverManager's stdout and stderr.
	SECURITY_ATTRIBUTES sa = {};
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	HANDLE hRead = NULL, hWrite = NULL;
	bool pipeOk = CreatePipe(&hRead, &hWrite, &sa, 0) &&
	              SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
	if (!pipeOk)
	{
		if (hRead)  { CloseHandle(hRead);  hRead  = NULL; }
		if (hWrite) { CloseHandle(hWrite); hWrite = NULL; }
	}

	if (pipeOk)
	{
		STARTUPINFO si = {};
		si.cb          = sizeof(si);
		si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
		si.hStdOutput  = hWrite;
		si.hStdError   = hWrite;   // merge stderr into stdout

		tstringi cmdLine = _T("\"") + exe + _T("\" ") + i_arg;
		PROCESS_INFORMATION pi = {};
		BOOL created = CreateProcess(NULL, const_cast<LPTSTR>(cmdLine.c_str()),
		                             NULL, NULL, TRUE, 0, NULL,
		                             installDir.c_str(), &si, &pi);
		// Close parent's write-end so ReadFile returns EOF when the child exits.
		CloseHandle(hWrite); hWrite = NULL;

		if (!created)
		{
			CloseHandle(hRead);
			return GetLastError();
		}

		// Drain the pipe while the child runs (avoids pipe-buffer deadlock).
		char rawBuf[4096];
		DWORD totalRead = 0, bytesRead = 0;
		while (totalRead < sizeof(rawBuf) - 1 &&
		       ReadFile(hRead, rawBuf + totalRead,
		                static_cast<DWORD>(sizeof(rawBuf) - 1 - totalRead),
		                &bytesRead, NULL) &&
		       bytesRead > 0)
		{
			totalRead += bytesRead;
		}
		CloseHandle(hRead);
		rawBuf[totalRead] = '\0';

		WaitForSingleObject(pi.hProcess, INFINITE);
		DWORD exitCode = 1;
		GetExitCodeProcess(pi.hProcess, &exitCode);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		if (totalRead > 0)
		{
			int wlen = MultiByteToWideChar(CP_ACP, 0, rawBuf, -1, NULL, 0);
			if (wlen > 1)
			{
				WCHAR *wbuf = new WCHAR[wlen];
				MultiByteToWideChar(CP_ACP, 0, rawBuf, -1, wbuf, wlen);
				g_driverManagerLastOutput = wbuf;
				delete[] wbuf;
			}
		}

		return exitCode == 0 ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
	}

	// Fallback when pipe creation fails: run without capturing output.
	SHELLEXECUTEINFO sei = {};
	sei.cbSize       = sizeof(sei);
	sei.fMask        = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
	sei.lpVerb       = NULL;
	sei.lpFile       = exe.c_str();
	sei.lpParameters = i_arg.c_str();
	sei.lpDirectory  = installDir.c_str();
	sei.nShow        = SW_HIDE;
	if (!ShellExecuteEx(&sei))
		return GetLastError();
	WaitForSingleObject(sei.hProcess, INFINITE);
	DWORD exitCode = 1;
	GetExitCodeProcess(sei.hProcess, &exitCode);
	CloseHandle(sei.hProcess);
	return exitCode == 0 ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
}

// create driver service
DWORD createDriverService(const tstringi &i_serviceName)
{
	tstringi arg = tstringi(_T("install ")) + i_serviceName;
	return runDriverManager(arg);
}
#endif // _WINNT

#if defined(_WINNT)
// remove driver service
DWORD removeDriverService(const tstringi &i_serviceName)
{
	tstringi arg = tstringi(_T("uninstall ")) + i_serviceName;
	return runDriverManager(arg);
}
#endif // _WINNT

#if defined(_WINNT)
// check whether the named service currently exists. Stopping/waiting for the
// service itself is fully delegated to DriverManager.exe's stopAndWaitService
// (called internally from InstallDriver/UninstallDriver) -- this is the single
// place that actually waits, so the previous non-waiting native stopDriverService
// here has been removed to avoid a duplicate, weaker implementation (W5対応).
bool driverServiceExists(const tstringi &i_serviceName)
{
	SC_HANDLE hscm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (!hscm)
		return false;
	SC_HANDLE hs = OpenService(hscm, i_serviceName.c_str(), SERVICE_QUERY_STATUS);
	bool exists = (hs != NULL);
	if (hs)
		CloseServiceHandle(hs);
	CloseServiceHandle(hscm);
	return exists;
}

// force-remove a driver name from the keyboard class UpperFilters value,
// independent of DriverManager.exe's own result. DriverManager already retries
// and aborts safely on failure (see DriverManager.cpp), but this is an
// additional, fully independent layer: even if DriverManager.exe itself is
// missing, fails to launch, or its registry write somehow fails, UpperFilters
// must never retain a reference to an uninstalled/failed driver (絶対要件1).
bool forceRemoveUpperFiltersEntry(const tstringi &i_driverName)
{
	Registry reg(HKEY_LOCAL_MACHINE, NODOKAD_FILTER_KEY);
	Registry::tstrings filters;
	if (!reg.read(_T("UpperFilters"), &filters))
		return true; // no UpperFilters value at all -> nothing to remove

	size_t before = filters.size();
	filters.remove(i_driverName);
	if (filters.size() == before)
		return true; // driver name was not present

	return reg.write(_T("UpperFilters"), filters);
}
#endif // _WINNT

// determine processor architecture
void getSysInfo(SYSTEM_INFO *sysInfo)
{
	static bool first = true;
	static void(WINAPI * pGetNativeSystemInfo)(LPSYSTEM_INFO);
	if (first)
	{
		first = false;
		*(FARPROC *)&pGetNativeSystemInfo =
			GetProcAddress(LoadLibrary(_T("kernel32")), "GetNativeSystemInfo");
	}
	if (pGetNativeSystemInfo)
	{
		pGetNativeSystemInfo(sysInfo);
		return;
	}
	GetSystemInfo(sysInfo);
}

// disable WOW64 file system redirection
BOOL disableWow64FsRedir(PVOID *oldValue)
{
#ifndef _WIN64
	static BOOL first = TRUE;
	static BOOL(WINAPI * pWow64DisableWow64FsRedirection)(PVOID *);
	if (first)
	{
		first = FALSE;
		*(FARPROC *)&pWow64DisableWow64FsRedirection =
			GetProcAddress(LoadLibrary(_T("kernel32")),
						   "Wow64DisableWow64FsRedirection");
	}
	if (!pWow64DisableWow64FsRedirection)
		return FALSE;
	return pWow64DisableWow64FsRedirection(oldValue);
#else
	return TRUE;
#endif
}

// revert WOW64 file system redirection
BOOL revertWow64FsRedir(PVOID oldValue)
{
#ifndef _WIN64
	static BOOL first = TRUE;
	static BOOL(WINAPI * pWow64RevertWow64FsRedirection)(PVOID);
	if (first)
	{
		first = FALSE;
		*(FARPROC *)&pWow64RevertWow64FsRedirection =
			GetProcAddress(LoadLibrary(_T("kernel32")),
						   "Wow64RevertWow64FsRedirection");
	}
	if (!pWow64RevertWow64FsRedirection)
		return FALSE;
	return pWow64RevertWow64FsRedirection(oldValue);
#else
	return TRUE;
#endif
}

// check operating system
bool checkOs(SetupFile::OS os)
{
	OSVERSIONINFO ver;
	ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&ver);
	SYSTEM_INFO sysInfo;
	getSysInfo(&sysInfo);

	// Intelアーキテクチャー以外は排除
	switch (os)
	{
	default:
		break;
	case SetupFile::NTx86:
	case SetupFile::NT4x86:
	case SetupFile::W2kx86:
	case SetupFile::XPx86:
		if (sysInfo.wProcessorArchitecture != PROCESSOR_ARCHITECTURE_INTEL)
			return false;
		break;
	}

	switch (os)
	{
	default:
	case SetupFile::ALL:
		return true;
	case SetupFile::W9x:
		return (ver.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS &&
				4 <= ver.dwMajorVersion);
	case SetupFile::NTx86:
	case SetupFile::NT:
		return (ver.dwPlatformId == VER_PLATFORM_WIN32_NT &&
				4 <= ver.dwMajorVersion);
	case SetupFile::NT4x86:
	case SetupFile::NT4:
		return (ver.dwPlatformId == VER_PLATFORM_WIN32_NT &&
				ver.dwMajorVersion == 4);
	case SetupFile::W2k:
		return (ver.dwPlatformId == VER_PLATFORM_WIN32_NT &&
				5 == ver.dwMajorVersion && 0 == ver.dwMinorVersion);
	case SetupFile::W2kx86:
		return (ver.dwPlatformId == VER_PLATFORM_WIN32_NT &&
				5 <= ver.dwMajorVersion && 0 <= ver.dwMinorVersion);
	case SetupFile::XPx86:
	case SetupFile::XP:
		return (ver.dwPlatformId == VER_PLATFORM_WIN32_NT &&
				5 <= ver.dwMajorVersion && 1 <= ver.dwMinorVersion);
	case SetupFile::AMD64:
		return sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;
	}
}

// install files
bool installFiles(const SetupFile::Data *i_setupFiles,
				  size_t i_setupFilesSize, u_int32 i_flags,
				  const tstringi &i_srcDir, const tstringi &i_destDir,
				  bool m_doRegisterDeviceDriver, bool m_doNotReviseDotNodoka)
{
	tstringi to, from;
	tstringi destDriverDir = getDriverDirectory();

	for (size_t i = 0; i < i_setupFilesSize; ++i)
	{
		const SetupFile::Data &s = i_setupFiles[i];
		const tstringi &fromDir = i_srcDir;
		const tstringi &toDir =
			(s.m_destination == SetupFile::ToDriver) ? destDriverDir : i_destDir;

		if (!s.m_from)
			continue; // remove only

		if (fromDir == toDir)
			continue; // same directory

		if (!checkOs(s.m_os)) // check operating system
			continue;

		if ((s.m_flags & i_flags) != i_flags) // check flags
			continue;

		if ((s.m_destination == SetupFile::ToDriver) && (m_doRegisterDeviceDriver == false))
			continue;

		if ((s.m_destination == SetupFile::ToDot) && (m_doNotReviseDotNodoka == true))
			continue;

		// type
		switch (s.m_kind)
		{
		case SetupFile::Dll:
		{
			// rename driver
			tstringi from_ = toDir + _T("\\") + s.m_to;
			tstringi to_ = toDir + _T("\\deleted.") + s.m_to;
			SetFileAttributes(to_.c_str(), FILE_ATTRIBUTE_NORMAL);
			DeleteFile(to_.c_str());
			MoveFile(from_.c_str(), to_.c_str());
			SetFileAttributes(to_.c_str(), FILE_ATTRIBUTE_NORMAL);
			DeleteFile(to_.c_str());
		}
			// fall through
		default:
		case SetupFile::File:
		{
			from += fromDir + _T('\\') + s.m_from + _T('\0');
			to += toDir + _T('\\') + s.m_to + _T('\0');
			break;
		}
		case SetupFile::Dir:
		{
			createDirectories((toDir + _T('\\') + s.m_to).c_str());
			break;
		}
		}
	}
#if 0
			{
			tstringi to_(to), from_(from);
			for (size_t i = 0; i < to_.size(); ++ i)
				if (!to_[i])
					to_[i] = ' ';
			for (size_t i = 0; i < from_.size(); ++ i)
				if (!from_[i])
					from_[i] = ' ';
			MessageBox(NULL, to_.c_str(), from_.c_str(), MB_OK);
			}
#endif

	PVOID oldValue;
	disableWow64FsRedir(&oldValue);
	SHFILEOPSTRUCT fo;
	::ZeroMemory(&fo, sizeof(fo));
	fo.wFunc = FO_COPY;
	fo.fFlags = FOF_MULTIDESTFILES | FOF_SIMPLEPROGRESS | FOF_NOCONFIRMATION;
	fo.pFrom = from.c_str();
	fo.pTo = to.c_str();

	DBG_PRINT((L"%s", fo.pFrom));
	DBG_PRINT((L" to "));
	DBG_PRINT((L"  %s\n", fo.pTo));

	bool result = !(SHFileOperation(&fo) || fo.fAnyOperationsAborted);
	if (result)
		DBG_PRINT((L"result = true\n"));
	else
		DBG_PRINT((L"result = false\n"));

	revertWow64FsRedir(oldValue);
	return result;
}

// remove files from src
bool removeSrcFiles(const SetupFile::Data *i_setupFiles,
					size_t i_setupFilesSize, u_int32 i_flags,
					const tstringi &i_srcDir)
{
	tstringi destDriverDir = getDriverDirectory();

	for (size_t i = 0; i < i_setupFilesSize; ++i)
	{
		const SetupFile::Data &s = i_setupFiles[i_setupFilesSize - i - 1];
		const tstringi &fromDir = i_srcDir;

		if (!s.m_from)
			continue; // remove only

		if (!checkOs(s.m_os)) // check operating system
			continue;

		if ((s.m_flags & i_flags) != i_flags) // check flags
			continue;

		// type
		switch (s.m_kind)
		{
		default:
		case SetupFile::Dll:
		case SetupFile::File:
			SetFileAttributes((fromDir + _T('\\') + s.m_from).c_str(), FILE_ATTRIBUTE_NORMAL);
			DeleteFile((fromDir + _T('\\') + s.m_from).c_str());
			break;
		case SetupFile::Dir:
			RemoveDirectory((fromDir + _T('\\') + s.m_from).c_str());
			break;
		}
	}
	RemoveDirectory(i_srcDir.c_str());
	return true;
}

// remove files
void removeFiles(const SetupFile::Data *i_setupFiles,
				 size_t i_setupFilesSize, u_int32 i_flags,
				 const tstringi &i_destDir)
{
	tstringi destDriverDir = getDriverDirectory();

	for (size_t i = 0; i < i_setupFilesSize; ++i)
	{
		const SetupFile::Data &s = i_setupFiles[i_setupFilesSize - i - 1];
		const tstringi &toDir =
			(s.m_destination == SetupFile::ToDriver) ? destDriverDir : i_destDir;

		if (!checkOs(s.m_os)) // check operating system
			continue;

		if ((s.m_flags & i_flags) != i_flags) // check flags
			continue;

		// type
		switch (s.m_kind)
		{
		case SetupFile::Dll:
			SetFileAttributes((toDir + _T("\\deleted.") + s.m_to).c_str(), FILE_ATTRIBUTE_NORMAL);
			DeleteFile((toDir + _T("\\deleted.") + s.m_to).c_str());
			// fall through
		default:
		case SetupFile::File:
			PVOID oldValue;
			if (s.m_destination == SetupFile::ToDriver)
				disableWow64FsRedir(&oldValue);
			SetFileAttributes((toDir + _T('\\') + s.m_to).c_str(), FILE_ATTRIBUTE_NORMAL);
			DeleteFile((toDir + _T('\\') + s.m_to).c_str());
			if (s.m_destination == SetupFile::ToDriver)
				revertWow64FsRedir(oldValue);

			break;
		case SetupFile::Dir:
			RemoveDirectory((toDir + _T('\\') + s.m_to).c_str());
			break;
		}
	}
	RemoveDirectory(i_destDir.c_str());
}

// uninstall step1
int uninstallStep1(const _TCHAR *i_uninstallOption)
{
	// copy this EXEcutable image into the user's temp directory
	_TCHAR setup_exe[GANA_MAX_PATH], tmp_setup_exe[GANA_MAX_PATH];
	GetModuleFileName(NULL, setup_exe, NUMBER_OF(setup_exe));
	GetTempPath(NUMBER_OF(tmp_setup_exe), tmp_setup_exe);
	GetTempFileName(tmp_setup_exe, _T("del"), 0, tmp_setup_exe);
	CopyFile(setup_exe, tmp_setup_exe, FALSE);

	// open the clone EXE using FILE_FLAG_DELETE_ON_CLOSE
	HANDLE hfile = CreateFile(tmp_setup_exe, 0, FILE_SHARE_READ, NULL,
							  OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);

	// spawn the clone EXE passing it our EXE's process handle
	// and the full path name to the original EXE file.
	_TCHAR commandLine[512];
	HANDLE hProcessOrig =
		OpenProcess(SYNCHRONIZE, TRUE, GetCurrentProcessId());
	_sntprintf_s(commandLine, NUMBER_OF(commandLine), _TRUNCATE, _T("%s %s %d"),
				 tmp_setup_exe, i_uninstallOption, hProcessOrig);
	STARTUPINFO si;
	::ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
	CreateProcess(NULL, commandLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
	Sleep(2000); // important
	CloseHandle(hProcessOrig);
	CloseHandle(hfile);

	return 0;
}

// uninstall step2
// (after this function, we cannot use any resource)
void uninstallStep2(const _TCHAR *argByStep1)
{
	// clone EXE: When original EXE terminates, delete it
	HANDLE hProcessOrig = (HANDLE)_ttoi(argByStep1);
	WaitForSingleObject(hProcessOrig, INFINITE);
	CloseHandle(hProcessOrig);
}

// ngen.exe
void dongen(LPCTSTR i_pathLink)
{
	// get ngen.exe path
	_TCHAR winPath[MAX_PATH];
	_TCHAR frameworkPath[MAX_PATH];
	_TCHAR exeLine[MAX_PATH];
	_TCHAR commandLine[MAX_PATH];

	SHELLEXECUTEINFO shExecInfo;

	CHECK_TRUE(GetWindowsDirectory(winPath, NUMBER_OF(winPath)));
#ifdef _WIN64
	_sntprintf_s(frameworkPath, NUMBER_OF(frameworkPath), _TRUNCATE, _T("%s\\%s"),
				 winPath, _T("\\Microsoft.NET\\Framework64\\v4.0.30319"));
#else
	_sntprintf_s(frameworkPath, NUMBER_OF(frameworkPath), _TRUNCATE, _T("%s\\%s"),
				 winPath, _T("\\Microsoft.NET\\Framework\\v4.0.30319"));
#endif
	_sntprintf_s(exeLine, NUMBER_OF(exeLine), _TRUNCATE, _T("%s\\%s"),
				 frameworkPath, _T("ngen.exe"));

	_sntprintf_s(commandLine, NUMBER_OF(commandLine), _TRUNCATE, _T("install \"%s\""),
				 i_pathLink);

	// do ngen.exe
	ZeroMemory(&shExecInfo, sizeof(shExecInfo));
	shExecInfo.cbSize = sizeof(shExecInfo);
	shExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	shExecInfo.hwnd = NULL;
	shExecInfo.lpVerb = L"open";
	shExecInfo.lpFile = exeLine;
	shExecInfo.lpDirectory = frameworkPath;
	shExecInfo.nShow = SW_HIDE;
	shExecInfo.hInstApp = NULL;
	shExecInfo.lpParameters = commandLine;

	//MessageBox(NULL, shExecInfo.lpFile, shExecInfo.lpParameters, MB_OK);

	HCURSOR cur = SetCursor(LoadCursor(NULL, IDC_WAIT));

	if ((ShellExecuteEx(&shExecInfo)) == 0)
	{
		LPVOID lpMsgBuf;
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			GetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf,
			0,
			NULL);

		MessageBox(NULL, (LPCTSTR)lpMsgBuf, L"ngen.exe", MB_OK);
		LocalFree(lpMsgBuf);
	}

	WaitForSingleObject(shExecInfo.hProcess, INFINITE); // wait exit
	SetCursor(cur);
}

bool checkDotNet()
{
	// get ngen.exe path
	_TCHAR winPath[MAX_PATH];
	_TCHAR frameworkPath[MAX_PATH];
	_TCHAR exeLine[MAX_PATH];

	GetWindowsDirectory(winPath, NUMBER_OF(winPath));

#ifdef _WIN64
	_sntprintf_s(frameworkPath, NUMBER_OF(frameworkPath), _TRUNCATE, _T("%s\\%s"),
				 winPath, _T("\\Microsoft.NET\\Framework64\\v4.0.30319"));
#else
	_sntprintf_s(frameworkPath, NUMBER_OF(frameworkPath), _TRUNCATE, _T("%s\\%s"),
				 winPath, _T("\\Microsoft.NET\\Framework\\v4.0.30319"));
#endif
	_sntprintf_s(exeLine, NUMBER_OF(exeLine), _TRUNCATE, _T("%s\\%s"),
				 frameworkPath, _T("ngen.exe"));

	// check ngen.exe
	WIN32_FIND_DATA wfd;
	HANDLE hFile = FindFirstFile(exeLine, &wfd);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		FindClose(hFile);
		return false;
	}
	else
	{
		FindClose(hFile);
		return true;
	}
}

/////////////////////////////////////////////////////////////////////////////
// Locale / StringResource

// constructor
Resource::Resource(const StringResource *i_stringResources)
	: m_stringResources(i_stringResources),
	  m_locale(LOCALE_C)
{
	struct LocaleInformaton
	{
		const _TCHAR *m_localeString;
		Locale m_locale;
	};

	// set locale information
	const _TCHAR *localeString = ::_tsetlocale(LC_ALL, _T(""));

	static const LocaleInformaton locales[] =
		{
			{_T("Japanese_Japan.932"), LOCALE_Japanese_Japan_932},
		};

	for (size_t i = 0; i < NUMBER_OF(locales); ++i)
		if (_tcsicmp(localeString, locales[i].m_localeString) == 0)
		{
			m_locale = locales[i].m_locale;
			break;
		}
}

// get resource string
const _TCHAR *Resource::loadString(UINT i_id)
{
	int n = static_cast<int>(m_locale);
	int index = -1;
	for (int i = 0; m_stringResources[i].m_str; ++i)
		if (m_stringResources[i].m_id == i_id)
		{
			if (n == 0)
				return m_stringResources[i].m_str;
			index = i;
			n--;
		}
	if (0 <= index)
		return m_stringResources[index].m_str;
	else
		return _T("");
}
} // namespace Installer
