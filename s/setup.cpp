///////////////////////////////////////////////////////////////////////////////
// setup.cpp
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#include "..\nodoka\misc.h"
#include "..\nodoka\registry.h"
#include "..\nodoka\stringtool.h"
#include "..\nodoka\windowstool.h"
#include "..\nodoka\nodoka.h"
#include "setuprc.h"
#include "installer.h"

#include <windowsx.h>
#include <shlobj.h>

#define ID_MENUITEM_quit 40001
#define NODOKA_OLD_REGISTRY_ROOT1 HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Wow6432Node\\appletkan\\nodoka")
#define NODOKA_OLD_REGISTRY_ROOT2 HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Wow6432Node\\appletkan")
#define NODOKA_OLD_REGISTRY_ROOT3 HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\nodoka")

using namespace Installer;

///////////////////////////////////////////////////////////////////////////////
// Registry

#define DIR_REGISTRY_ROOT \
	HKEY_LOCAL_MACHINE,   \
		_T("Software\\appletkan\\nodoka")

///////////////////////////////////////////////////////////////////////////////
// Globals

enum
{
	Flag_Usb = 1 << 1,
};
u_int32 g_flags = SetupFile::Normal;

using namespace SetupFile;

// same name
#define SN(i_kind, i_os, i_from, i_destination)			\
		{ i_kind, i_os, Normal|Flag_Usb, _T(i_from), i_destination, _T(i_from) }
// different name
#define DN(i_kind, i_os, i_from, i_destination, i_to)	\
		{ i_kind, i_os, Normal|Flag_Usb, _T(i_from), i_destination, _T(i_to) }

const SetupFile::Data g_setupFiles[] =
	{
		// executables
		SN(Dll, ALL, "nodoka.dll", ToDest),
		SN(File, ALL, "nodoka.exe", ToDest),
		SN(File, ALL, "nodoka_limit.exe", ToDest),
		SN(File, ALL, "nodoka_hil.exe", ToDest),
		SN(File, ALL, "setup.exe", ToDest),
		SN(File, ALL, "nshell.exe", ToDest),
		SN(Dll, ALL, "gamepad.dll", ToDest),
		SN(File, ALL, "GuiEdit.exe", ToDest),
		SN(File, ALL, "dotnet_starter.exe", ToDest),
		SN(Dll, ALL, "sirius_hook_for_nodoka_x86.dll", ToDest),
		SN(Dll, AMD64, "gamepad64.dll", ToDest),
		SN(Dll, AMD64, "nodoka64.dll", ToDest),
		SN(File, AMD64, "nodoka64.exe", ToDest),
		SN(File, AMD64, "nodoka64_limit.exe", ToDest),
		SN(File, AMD64, "nodoka64_hil.exe", ToDest),
		SN(File, AMD64, "nodoka64_nua.exe", ToDest),
		SN(File, AMD64, "nodoka_helper.exe", ToDest),
		SN(File, AMD64, "setup64.exe", ToDest),
		SN(File, AMD64, "nshell64.exe", ToDest),
		SN(Dll, AMD64, "sirius_hook_for_nodoka_x64.dll", ToDest),

		// PDB files for crash analysis
		SN(File, ALL,  "nodoka.dll.pdb",          ToDest),
		SN(File, ALL,  "nodoka.exe.pdb",           ToDest),
		SN(File, AMD64, "nodoka64.dll.pdb",        ToDest),
		SN(File, AMD64, "nodoka64.exe.pdb",        ToDest),
		SN(File, AMD64, "nodoka_helper.exe.pdb",   ToDest),

		// drivers
		// DriverManager executables
		DN(File, AMD64,  "DriverManager64.exe", ToDest, "DriverManager64.exe"),
		DN(File, W2kx86, "DriverManager.exe",   ToDest, "DriverManager.exe"),

		// nodokad driver package for DriverManager (relative to DriverManager.exe)
		// "driver\" directory removed: DriverManager uses "nodokad\" exclusively
		SN(Dir, AMD64,  "nodokad", ToDest), // mkdir
		SN(Dir, W2kx86, "nodokad", ToDest), // mkdir
		DN(File, AMD64,  "nodokad.sys", ToDest, "nodokad\\nodokad.sys"),
		DN(File, AMD64,  "nodokad.inf", ToDest, "nodokad\\nodokad.inf"),
		DN(File, AMD64,  "nodokad.cat", ToDest, "nodokad\\nodokad.cat"),
		DN(File, AMD64,  "nodokad.pdb", ToDest, "nodokad\\nodokad.pdb"),
		DN(File, W2kx86, "nodokadx86.sys", ToDest, "nodokad\\nodokad.sys"),
		DN(File, W2kx86, "nodokadx86.inf", ToDest, "nodokad\\nodokad.inf"),
		DN(File, W2kx86, "nodokadx86.cat", ToDest, "nodokad\\nodokad.cat"),
		DN(File, W2kx86, "nodokad.pdb",    ToDest, "nodokad\\nodokad.pdb"),

		// setting files
		SN(File, ALL, "104.nodoka", ToDot),
		SN(File, ALL, "104on109.nodoka", ToDot),
		SN(File, ALL, "109.nodoka", ToDot),
		SN(File, ALL, "109on104.nodoka", ToDot),
		SN(File, ALL, "default.nodoka", ToDot),
		SN(File, ALL, "default2.nodoka", ToDot),
		SN(File, ALL, "dot.nodoka", ToDot),
		SN(File, ALL, "doten.nodoka", ToDot),
		SN(File, ALL, "dotjp.nodoka", ToDot),
		SN(File, ALL, "read-keyboard-define.nodoka", ToDot),
		SN(File, ALL, "Shift-F2_toggle_US-JP-Keyboard.nodoka", ToDot),
		SN(File, ALL, "emacsedit.nodoka", ToDot),
		SN(File, ALL, "gamepad.nodoka", ToDot),
		SN(File, ALL, "gamepad-mouse.nodoka", ToDot),
		SN(File, ALL, "gamepad2-mouse.nodoka", ToDot),
		SN(File, ALL, "add-mouse-gamepad.nodoka", ToDot),

		DN(File, ALL, "104.nodoka", ToDot, "104.mayu"),
		DN(File, ALL, "104on109.nodoka", ToDot, "104on109.mayu"),
		DN(File, ALL, "109.nodoka", ToDot, "109.mayu"),
		DN(File, ALL, "109on104.nodoka", ToDot, "109on104.mayu"),
		DN(File, ALL, "default.nodoka", ToDot, "default.mayu"),
		DN(File, ALL, "emacsedit.nodoka", ToDot, "emacsedit.mayu"),

		// setting files original
		SN(Dir, ALL, "original", ToDest), // mkdir
		DN(File, ALL, "104.nodoka", ToDest, "original\\104.nodoka"),
		DN(File, ALL, "104on109.nodoka", ToDest, "original\\104on109.nodoka"),
		DN(File, ALL, "109.nodoka", ToDest, "original\\109.nodoka"),
		DN(File, ALL, "109on104.nodoka", ToDest, "original\\109on104.nodoka"),
		DN(File, ALL, "default.nodoka", ToDest, "original\\default.nodoka"),
		DN(File, ALL, "default2.nodoka", ToDest, "original\\default2.nodoka"),
		DN(File, ALL, "dot.nodoka", ToDest, "original\\dot.nodoka"),
		DN(File, ALL, "doten.nodoka", ToDest, "original\\doten.nodoka"),
		DN(File, ALL, "dotjp.nodoka", ToDest, "original\\dotjp.nodoka"),
		DN(File, ALL, "read-keyboard-define.nodoka", ToDest, "original\\read-keyboard-define.nodoka"),
		DN(File, ALL, "Shift-F2_toggle_US-JP-Keyboard.nodoka", ToDest, "original\\Shift-F2_toggle_US-JP-Keyboard.nodoka"),
		DN(File, ALL, "emacsedit.nodoka", ToDest, "original\\emacsedit.nodoka"),
		DN(File, ALL, "gamepad.nodoka", ToDest, "original\\gamepad.nodoka"),
		DN(File, ALL, "gamepad-mouse.nodoka", ToDest, "original\\gamepad-mouse.nodoka"),
		DN(File, ALL, "gamepad2-mouse.nodoka", ToDest, "original\\gamepad2-mouse.nodoka"),
		DN(File, ALL, "add-mouse-gamepad.nodoka", ToDest, "original\\add-mouse-gamepad.nodoka"),

		DN(File, ALL, "104.nodoka", ToDest, "original\\104.mayu"),
		DN(File, ALL, "104on109.nodoka", ToDest, "original\\104on109.mayu"),
		DN(File, ALL, "109.nodoka", ToDest, "original\\109.mayu"),
		DN(File, ALL, "109on104.nodoka", ToDest, "original\\109on104.mayu"),
		DN(File, ALL, "default.nodoka", ToDest, "original\\default.mayu"),
		DN(File, ALL, "emacsedit.nodoka", ToDest, "original\\emacsedit.mayu"),

		// documents
		SN(Dir, ALL, "doc", ToDest), // mkdir
		DN(File, ALL, "banner-ja.gif", ToDest, "doc\\banner-ja.gif"),
		DN(File, ALL, "edit-setting-ja.png", ToDest, "doc\\edit-setting-ja.png"),
		DN(File, ALL, "investigate-ja.png", ToDest, "doc\\investigate-ja.png"),
		DN(File, ALL, "log-ja.jpg", ToDest, "doc\\log-ja.jpg"),
		DN(File, ALL, "menu-ja.png", ToDest, "doc\\menu-ja.png"),
		DN(File, ALL, "pause-ja.png", ToDest, "doc\\pause-ja.png"),
		DN(File, ALL, "setting-ja.png", ToDest, "doc\\setting-ja.png"),
		DN(File, ALL, "target.png", ToDest, "doc\\target.png"),
		DN(File, ALL, "version.jpg", ToDest, "doc\\version.jpg"),
		DN(File, ALL, "tasktray-icon.png", ToDest, "doc\\tasktray-icon.png"),
		DN(File, ALL, "copy-ja.png", ToDest, "doc\\copy-ja.png"),
		DN(File, ALL, "virtualstore-ja.png", ToDest, "doc\\virtualstore-ja.png"),
		DN(File, ALL, "icon0.png", ToDest, "doc\\icon0.png"),
		DN(File, ALL, "regedit.png", ToDest, "doc\\regedit.png"),
		DN(File, ALL, "version86.jpg", ToDest, "doc\\version86.jpg"),
		DN(File, ALL, "tasktray-icon7.png", ToDest, "doc\\tasktray-icon7.png"),
		DN(File, ALL, "tasktray-icon7help.png", ToDest, "doc\\tasktray-icon7help.png"),
		DN(File, ALL, "tasktray-icon7help2.png", ToDest, "doc\\tasktray-icon7help2.png"),
		DN(File, ALL, "tasktray-icon7help3.png", ToDest, "doc\\tasktray-icon7help3.png"),
		DN(File, ALL, "GuiEdit.png", ToDest, "doc\\GuiEdit.png"),
		DN(File, ALL, "setup0.jpg", ToDest, "doc\\setup0.jpg"),
		DN(File, ALL, "setup1.jpg", ToDest, "doc\\setup1.jpg"),
		DN(File, ALL, "setup3.jpg", ToDest, "doc\\setup3.jpg"),
		DN(File, ALL, "CONTENTS-ja.html", ToDest, "doc\\CONTENTS-ja.html"),
		DN(File, ALL, "CONTENTS-en.html", ToDest, "doc\\CONTENTS-en.html"),
		DN(File, ALL, "CUSTOMIZE-ja.html", ToDest, "doc\\CUSTOMIZE-ja.html"),
		DN(File, ALL, "CUSTOMIZE-en.html", ToDest, "doc\\CUSTOMIZE-en.html"),
		DN(File, ALL, "MANUAL-ja.html", ToDest, "doc\\MANUAL-ja.html"),
		DN(File, ALL, "MANUAL-en.html", ToDest, "doc\\MANUAL-en.html"),
		DN(File, ALL, "README-ja.html", ToDest, "doc\\README-ja.html"),
		DN(File, ALL, "README-en.html", ToDest, "doc\\README-en.html"),
		DN(File, ALL, "README.css", ToDest, "doc\\README.css"),
		DN(File, ALL, "syntax.txt", ToDest, "doc\\syntax.txt"),
		DN(File, ALL, "104.nodoka", ToDest, "doc\\104.nodoka.txt"),
		DN(File, ALL, "104on109.nodoka", ToDest, "doc\\104on109.nodoka.txt"),
		DN(File, ALL, "109.nodoka", ToDest, "doc\\109.nodoka.txt"),
		DN(File, ALL, "109on104.nodoka", ToDest, "doc\\109on104.nodoka.txt"),
		DN(File, ALL, "default.nodoka", ToDest, "doc\\default.nodoka.txt"),
		DN(File, ALL, "default2.nodoka", ToDest, "doc\\default2.nodoka.txt"),
		DN(File, ALL, "doten.nodoka", ToDest, "doc\\doten.nodoka.txt"),
		DN(File, ALL, "dotjp.nodoka", ToDest, "doc\\dotjp.nodoka.txt"),
		DN(File, ALL, "read-keyboard-define.nodoka", ToDest, "doc\\read-keyboard-define.nodoka.txt"),
		DN(File, ALL, "Shift-F2_toggle_US-JP-Keyboard.nodoka", ToDest, "doc\\Shift-F2_toggle_US-JP-Keyboard.nodoka.txt"),
		DN(File, ALL, "emacsedit.nodoka", ToDest, "doc\\emacsedit.nodoka.txt"),
		DN(File, ALL, "gamepad.nodoka", ToDest, "doc\\gamepad.nodoka.txt"),
		DN(File, ALL, "gamepad-mouse.nodoka", ToDest, "doc\\gamepad-mouse.nodoka.txt"),
		DN(File, ALL, "gamepad2-mouse.nodoka", ToDest, "doc\\gamepad2-mouse.nodoka.txt"),
		DN(File, ALL, "add-mouse-gamepad.nodoka", ToDest, "doc\\add-mouse-gamepad.nodoka.txt"),
		DN(File, ALL, "thumbsense.nodoka", ToDest, "doc\\thumbsense.nodoka.txt"),
		DN(File, ALL, "nodoka-mode.el", ToDest, "doc\\nodoka-mode.el.txt"),
		DN(File, ALL, "109onAX.nodoka", ToDest, "doc\\109onAX.nodoka.txt"),
		DN(File, ALL, "98x1.nodoka", ToDest, "doc\\98x1.nodoka.txt"),
		DN(File, ALL, "ax.nodoka", ToDest, "doc\\ax.nodoka.txt"),
		DN(File, ALL, "dvorak.nodoka", ToDest, "doc\\dvorak.nodoka.txt"),
		DN(File, ALL, "dvorak109.nodoka", ToDest, "doc\\dvorak109.nodoka.txt"),
		DN(File, ALL, "DVORAKon109.nodoka", ToDest, "doc\\DVORAKon109.nodoka.txt"),
		DN(File, ALL, "keitai.nodoka", ToDest, "doc\\keitai.nodoka.txt"),
		DN(File, ALL, "sample.nodoka", ToDest, "doc\\sample.nodoka.txt"),
		DN(File, ALL, "other.nodoka", ToDest, "doc\\other.nodoka.txt"),
		DN(File, ALL, "ime.nodoka", ToDest, "doc\\ime.nodoka.txt"),
		DN(File, ALL, "cursor.nodoka", ToDest, "doc\\cursor.nodoka.txt"),
		DN(File, ALL, "no_badusb.nodoka", ToDest, "doc\\no_badusb.nodoka.txt"),
		DN(File, ALL, "104.gif", ToDest, "doc\\104.gif"),
		DN(File, ALL, "109.gif", ToDest, "doc\\109.gif"),

		DN(File, ALL, "GUIEdit-ja.html", ToDest, "doc\\GUIEdit-ja.html"),
		DN(File, ALL, "gui-edit-main-describe.png", ToDest, "doc\\gui-edit-main-describe.png"),
		DN(File, ALL, "gui-edit-command-main-edited.png", ToDest, "doc\\gui-edit-command-main-edited.png"),
		DN(File, ALL, "gui-edit-command-wizard-other3.png", ToDest, "doc\\gui-edit-command-wizard-other3.png"),
		DN(File, ALL, "gui-edit-command-wizard-other2.png", ToDest, "doc\\gui-edit-command-wizard-other2.png"),
		DN(File, ALL, "gui-edit-command-wizard-other1.png", ToDest, "doc\\gui-edit-command-wizard-other1.png"),
		DN(File, ALL, "gui-edit-command-wizard-mod3.png", ToDest, "doc\\gui-edit-command-wizard-mod3.png"),
		DN(File, ALL, "gui-edit-command-wizard-mod2.png", ToDest, "doc\\gui-edit-command-wizard-mod2.png"),
		DN(File, ALL, "gui-edit-command-wizard-mod1.png", ToDest, "doc\\gui-edit-command-wizard-mod1.png"),
		DN(File, ALL, "gui-edit-command-wizard-include2.png", ToDest, "doc\\gui-edit-command-wizard-include2.png"),
		DN(File, ALL, "gui-edit-command-wizard-include1.png", ToDest, "doc\\gui-edit-command-wizard-include1.png"),
		DN(File, ALL, "gui-edit-command-wizard-keymap3.png", ToDest, "doc\\gui-edit-command-wizard-keymap3.png"),
		DN(File, ALL, "gui-edit-command-wizard-keymap2.png", ToDest, "doc\\gui-edit-command-wizard-keymap2.png"),
		DN(File, ALL, "gui-edit-command-wizard-keymap1.png", ToDest, "doc\\gui-edit-command-wizard-keymap1.png"),
		DN(File, ALL, "gui-edit-command-wizard-3.png", ToDest, "doc\\gui-edit-command-wizard-3.png"),
		DN(File, ALL, "gui-edit-command-wizard-2.png", ToDest, "doc\\gui-edit-command-wizard-2.png"),
		DN(File, ALL, "gui-edit-command-wizard-1.png", ToDest, "doc\\gui-edit-command-wizard-1.png"),
		DN(File, ALL, "gui-edit-start-new.png", ToDest, "doc\\gui-edit-start-new.png"),
		DN(File, ALL, "gui-edit-main-loaded.png", ToDest, "doc\\gui-edit-main-loaded.png"),
		DN(File, ALL, "gui-edit-right-click.png", ToDest, "doc\\gui-edit-right-click.png"),
		DN(File, ALL, "gui-edit-setting1.png", ToDest, "doc\\gui-edit-setting1.png"),
		DN(File, ALL, "gui-edit-setting2.png", ToDest, "doc\\gui-edit-setting2.png"),
		DN(File, ALL, "copy-contrib.png", ToDest, "doc\\copy-contrib.png"),
		DN(File, ALL, "gui-edit-dot.nodoka.png", ToDest, "doc\\gui-edit-dot.nodoka.png"),
		DN(File, ALL, "gui-edit-sample.nodoka.png", ToDest, "doc\\gui-edit-sample.nodoka.png"),
		DN(File, ALL, "gui-edit-cursor.nodoka.png", ToDest, "doc\\gui-edit-cursor.nodoka.png"),

		SN(File, ALL, "readme.txt", ToDest),
		SN(File, ALL, "readme-en.txt", ToDest),
		SN(File, ALL, "nshell.txt", ToDest),
		SN(File, ALL, "LICENSE.txt", ToDest),
		SN(File, ALL, "LICENSE_JP.txt", ToDest),
		SN(File, ALL, "nodoka-mode.el", ToDest),

		SN(Dir, ALL, "contrib", ToDest), // mkdir
		DN(File, ALL, "nodoka-settings.txt", ToDest, "contrib\\nodoka-settings.txt"),
		DN(File, ALL, "dvorak.nodoka", ToDest, "contrib\\dvorak.nodoka"),
		DN(File, ALL, "DVORAKon109.nodoka", ToDest, "contrib\\DVORAKon109.nodoka"),
		DN(File, ALL, "keitai.nodoka", ToDest, "contrib\\keitai.nodoka"),
		DN(File, ALL, "ax.nodoka", ToDest, "contrib\\ax.nodoka"),
		DN(File, ALL, "98x1.nodoka", ToDest, "contrib\\98x1.nodoka"),
		DN(File, ALL, "109onAX.nodoka", ToDest, "contrib\\109onAX.nodoka"),
		DN(File, ALL, "sample.nodoka", ToDest, "contrib\\sample.nodoka"),
		DN(File, ALL, "other.nodoka", ToDest, "contrib\\other.nodoka"),
		DN(File, ALL, "ime.nodoka", ToDest, "contrib\\ime.nodoka"),
		DN(File, ALL, "cursor.nodoka", ToDest, "contrib\\cursor.nodoka"),
		DN(File, ALL, "no_badusb.nodoka", ToDest, "contrib\\no_badusb.nodoka"),

		SN(Dir, ALL, "Plugins", ToDest), // mkdir

		SN(Dir, ALL, "ts4nodoka", ToDest), // mkdir
		DN(File, ALL, "thumbsense.nodoka", ToDest, "ts4nodoka\\thumbsense.nodoka"),
		DN(File, ALL, "cts4nodoka.dll", ToDest, "ts4nodoka\\cts4nodoka.dll"),
		DN(File, ALL, "sts4nodoka.dll", ToDest, "ts4nodoka\\sts4nodoka.dll"),
		DN(File, ALL, "ats4nodoka.dll", ToDest, "ts4nodoka\\ats4nodoka.dll"),
		DN(File, AMD64, "ats4nodoka64.dll", ToDest, "ts4nodoka\\ats4nodoka64.dll"),
		DN(File, AMD64, "sts4nodoka64.dll", ToDest, "ts4nodoka\\sts4nodoka64.dll"),
};

enum KeyboardKind
{
	KEYBOARD_KIND_109,
	KEYBOARD_KIND_104,
} g_keyboardKind;

static const StringResource g_strres[] =
	{
#include "strres.h"
};

bool g_wasExecutedBySFX = false; // Was setup executed by cab32 SFX ?
Resource *g_resource;			 // resource information
tstringi g_destDir;				 // destination directory
tstringi g_envNODOKA;			 // 環境変数NODOKA
bool g_update = false;			 // update is true;
bool g_useDriver = false;		 // kbdclassのfilterにnodokadがある場合、TRUE

///////////////////////////////////////////////////////////////////////////////
// functions

// show message
int message(int i_id, int i_flag, HWND i_hwnd = NULL)
{
	return MessageBox(i_hwnd, g_resource->loadString(i_id),
					  g_resource->loadString(IDS_nodokaSetup), i_flag);
}

// driver service error
void driverServiceError(DWORD i_err)
{
	switch (i_err)
	{
	case ERROR_ACCESS_DENIED:
		message(IDS_notAdministrator, MB_OK | MB_ICONSTOP);
		break;
	case ERROR_SERVICE_MARKED_FOR_DELETE:
		message(IDS_alreadyUninstalled, MB_OK | MB_ICONSTOP);
		break;
	default:
	{
		TCHAR *errmsg;
		int err = int(i_err);
		if (err < 0)
		{
			i_err = -err;
		}
		tstringi fullMsg;
		if (FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				NULL, i_err, 0, (LPTSTR)&errmsg, 0, NULL))
		{
			TCHAR buf[1024];
			_sntprintf_s(buf, NUMBER_OF(buf), _TRUNCATE, _T("%s: %d: %s\n"),
						 g_resource->loadString(IDS_error),
						 err, errmsg);
			LocalFree(errmsg);
			fullMsg = buf;
		}
		else
		{
			fullMsg = g_resource->loadString(IDS_error);
		}
#if defined(_WINNT)
		{
			tstringi dmOut = Installer::getDriverManagerLastOutput();
			if (!dmOut.empty())
			{
				fullMsg += _T("\n--- DriverManager ---\n");
				fullMsg += dmOut;
			}
		}
#endif
		MessageBox(NULL, fullMsg.c_str(), g_resource->loadString(IDS_nodokaSetup),
				   MB_OK | MB_ICONSTOP);
		break;
	}
	}
}

#if defined(_WINNT)
// Check keyboard filter Entry  TURE: OK,   FALSE: NG
BOOL checkDriverEntry()
{
	BOOL bOK = TRUE;
	Registry reg(HKEY_LOCAL_MACHINE, NODOKAD_FILTER_KEY);
	typedef std::list<tstring> Filters;
	Filters filters;
	tstringi filtername = _T("Drivers Name: ");

	if (!reg.read(_T("UpperFilters"), &filters))
		return TRUE; // Entryが無いが、通常ありえないのでTRUE

	for (Filters::iterator i = filters.begin(); i != filters.end();)
	{
		Filters::iterator next = i;
		++next;
		if ((*i != _T("kbdclass")) && (*i != _T("nodokad")))
		{
			// ex. AltIME:altime, Nekomaneki:nmkcore, VMware:vmkbd, PGPi:pgpsdk
			bOK = FALSE;
			filtername += *i + _T(" ");
		}
		i = next;
	}
	// kbdclass, nodokad 以外のフィルタドライバを見つけたのでダイアログを出す。
	if (!bOK)
	{
		TCHAR buf[1024];
		_sntprintf_s(buf, NUMBER_OF(buf), _TRUNCATE, _T("%s\n\n%s\n"), g_resource->loadString(IDS_detectfilterdriver), filtername.c_str());

		int iYES = MessageBox(NULL, buf, g_resource->loadString(IDS_nodokaSetup), MB_YESNO | MB_ICONWARNING);

		if (iYES == IDYES)
			bOK = TRUE;
	}
	return bOK;
}

void checkDriverEntry2()
{
	Registry reg(HKEY_LOCAL_MACHINE, NODOKAD_FILTER_KEY);
	typedef std::list<tstring> Filters;
	Filters filters;
	tstringi filtername = _T("Drivers Name: ");

	reg.read(_T("UpperFilters"), &filters);

	for (Filters::iterator i = filters.begin(); i != filters.end();)
	{
		Filters::iterator next = i;
		++next;
		if (*i == _T("nodokad"))
		{
			// nodokadがある場合、以前はデバイスドライバを使っていたと判断
			g_useDriver = true;
		}
		i = next;
	}
}

bool ExitNodoka(void)
{
	HWND hwnd;			  // nodokaのウィンドウハンドル
	bool bMayu = false;   // nodokaではなくmayuが居た?
	bool bNodoka = false; // nodokaを終了させた?

	// nodokaを探す
	hwnd = FindWindow(_T("nodokaTasktray"), NULL);

	// nodokaが居なければ、mayuを探す。
	if (!hwnd)
	{
		hwnd = FindWindow(_T("mayuTasktray"), NULL);
		if (hwnd)
			bMayu = true;
	}
	else
		bNodoka = true;

	if (hwnd) // 居たら、正常終了させる。
	{
		// プロセスハンドルを取得してから終了要求を送る
		DWORD pid = 0;
		GetWindowThreadProcessId(hwnd, &pid);
		HANDLE hProcess = (pid != 0) ? OpenProcess(SYNCHRONIZE, FALSE, pid) : NULL;

		if (bMayu)
			SendMessage(hwnd, WM_COMMAND, MAKELONG(ID_MENUITEM_quit, 0), 0);
		else
			SendMessage(hwnd, WM_CLOSE, 0, 0);

		// 最大60秒、プロセス終了を確認してから次に進む
		const DWORD STEP_MS = 500;
		const DWORD MAX_MS  = 60000;
		if (hProcess != NULL)
		{
			// プロセスハンドルで確実に終了を検知
			DWORD elapsed = 0;
			while (elapsed < MAX_MS)
			{
				if (WaitForSingleObject(hProcess, STEP_MS) == WAIT_OBJECT_0)
					break;
				elapsed += STEP_MS;
			}
			CloseHandle(hProcess);
		}
		else
		{
			// ハンドル取得失敗時はウィンドウ消滅でポーリング
			DWORD elapsed = 0;
			while (elapsed < MAX_MS)
			{
				Sleep(STEP_MS);
				elapsed += STEP_MS;
				if (!FindWindow(_T("nodokaTasktray"), NULL) &&
				    !FindWindow(_T("mayuTasktray"), NULL))
					break;
			}
		}
	}

	return bNodoka;
}

#endif // _WINNT

///////////////////////////////////////////////////////////////////////////////
// dialogue

// dialog box
class DlgMain
{
	HWND m_hwnd;
	bool m_doRegisterToStartMenu;	// if register to the start menu
	bool m_doRegisterToStartUp;		// if register to the start up
	bool m_doRegisterToStartUp2;	// if do ngen.exe, and dotnet_starter.exe register to the start up
	bool m_doRegisterMouseHook;		// -mの登録
	bool m_doRegisterKeyboardHook;	// -kの登録
	bool m_doRegisterWin8WA;		// -wの登録
	bool m_doRegisterLimitVersion;	// _limit.exe登録
	bool m_doRegisterHILVersion;	// _hil.exe登録
	bool m_doRegisterToStartUp3;	// if register to the desktop

	bool m_doNotReviseShortCut;		// ショートカットは上書きしない
	bool m_doNotReviseDotNodoka;	// 設定ファイルは上書きしない
	bool m_doNotRegisterDriver;		// デバイスドライバを使わない
	bool m_reloadScancodeMap;		// ScancodeMap 変更モード -n の登録

private:
	// install
	int install(bool m_doNotRegisterDriver, bool m_doNotReviseDotNodoka)
	{
		Registry reg(DIR_REGISTRY_ROOT);
		CHECK_TRUE(reg.write(_T("dir"), g_destDir));
		tstringi srcDir = getModuleDirectory();
		DWORD err;
		BOOL bError = FALSE;
		PVOID oldValue;

		// 前回正常起動時の構成を有効化 (デバイスドライバの不具合からの復旧用)
		enableLastKnownGoodConfiguration();

#ifdef _WIN64
		// remove old Registry
		Registry::remove(NODOKA_OLD_REGISTRY_ROOT1);
		Registry::remove(NODOKA_OLD_REGISTRY_ROOT2);
		Registry::remove(NODOKA_OLD_REGISTRY_ROOT3);
#endif

		// 今回の呼び出しで新規にサービスを作るのか、アップグレードで既存サービスが
		// 既に動いているのかを記録する。ファイルコピー失敗時のロールバック判断に使う（W7対応）。
		// サービス停止+待機は DriverManager.exe の stopAndWaitService に一本化したため
		// （W5対応）、ここでの個別の停止呼び出しは行わない。実際の停止は
		// createDriverService() (= DriverManager.exe install) の内部で行われる。
		bool serviceExistedBeforeInstall = !m_doNotRegisterDriver && driverServiceExists(_T("nodokad"));

		// ファイルコピー
		if (!installFiles(g_setupFiles, NUMBER_OF(g_setupFiles), g_flags, srcDir, g_destDir, !m_doNotRegisterDriver, m_doNotReviseDotNodoka))
		{
			// ファイルコピーに失敗したので、ドライバの登録を解除する。
			// ただし、アップグレードで既存サービスが正常に動いていた場合は、
			// この時点ではまだ何も新規登録していないため、既存の登録を壊さないよう呼ばない（W7対応）。
			if (!serviceExistedBeforeInstall)
				err = removeDriverService(_T("nodokad"));
			bError = TRUE;
		}

		if (g_wasExecutedBySFX)
		{
			removeSrcFiles(g_setupFiles, NUMBER_OF(g_setupFiles), g_flags, srcDir);
		}
		if (bError) // ショートカットの削除、アンインストール情報の削除。拡張子の設定は残っているがあるが、そのまま
		{
			disableWow64FsRedir(&oldValue);

			DeleteFile(getStartMenuName(g_resource->loadString(IDS_shortcutName)).c_str());
			DeleteFile(getStartUpName(g_resource->loadString(IDS_shortcutName)).c_str());
			DeleteFile(getStartUpName(g_resource->loadString(IDS_shortcutName2)).c_str());
			DeleteFile(getDeskTopName(g_resource->loadString(IDS_shortcutName)).c_str());

			revertWow64FsRedir(oldValue);

			removeUninstallInformation(_T("nodoka"));
			message(IDS_installerror, MB_OK | MB_ICONSTOP);
			return 1;
		}
		else // doten/jp.nodoka -> dot.nodoka, doten/jp.nodoka.txt -> dot.nodoka.txt
		{
			disableWow64FsRedir(&oldValue);
			//MessageBox(NULL, (g_destDir + _T("\\") + g_resource->loadString(IDS_dotnodoka)).c_str(), _T("Test"), MB_OK);
			if (!m_doNotReviseDotNodoka)
			{
				CopyFile((g_destDir + _T("\\") + g_resource->loadString(IDS_dotnodoka)).c_str(), (g_destDir + _T("\\dot.nodoka")).c_str(), false);
			}
			CopyFile((g_destDir + _T("\\doc\\") + g_resource->loadString(IDS_dotnodoka) + _T(".txt")).c_str(), (g_destDir + _T("\\doc\\dot.nodoka.txt")).c_str(), false);
			revertWow64FsRedir(oldValue);
		}

		// ドライバインストール

		if (!m_doNotRegisterDriver)
		{
			err = createDriverService(_T("nodokad"));

			if (err != ERROR_SUCCESS)
			{
				// ドライバの登録に失敗した。
				driverServiceError(err);

				// アップグレード中（既存の正常な登録が今回の試行以前から動いていた場合）は
				// ここでファイル・サービス・UpperFiltersを丸ごと削除すると、今回の失敗とは
				// 無関係に動いていた既存のインストールまで壊してしまう（W11対応）。
				// 新規インストールが最終段で失敗した場合のみ、今回作ろうとしたものだけなので
				// 安全に完全ロールバックできる。
				if (!serviceExistedBeforeInstall)
				{
					// ドライバの登録を解除し、インストールしたファイルを削除する。
					err = removeDriverService(_T("nodokad"));

					// DriverManagerの戻り値に関わらず、UpperFiltersから確実に削除する
					// 冗長な安全網（絶対要件1対応）。
					forceRemoveUpperFiltersEntry(_T("nodokad"));

					// インストールしたファイルを削除する（W6対応:
					// 以前はコメントアウトされコピー済みファイルが残置されていた）。
					removeFiles(g_setupFiles, NUMBER_OF(g_setupFiles), g_flags, g_destDir);

					// ショートカットの削除、アンインストール情報の削除。拡張子は残っているがあるが、そのまま
					disableWow64FsRedir(&oldValue);

					DeleteFile(getStartMenuName(g_resource->loadString(IDS_shortcutName)).c_str());
					DeleteFile(getStartUpName(g_resource->loadString(IDS_shortcutName)).c_str());
					DeleteFile(getStartUpName(g_resource->loadString(IDS_shortcutName2)).c_str());
					DeleteFile(getDeskTopName(g_resource->loadString(IDS_shortcutName)).c_str());

					revertWow64FsRedir(oldValue);

					removeUninstallInformation(_T("nodoka"));
				}
				return 1;
			}

			if (g_flags == Flag_Usb)
				CHECK_TRUE(reg.write(_T("isUsbDriver"), DWORD(1)));
		}
		else
		{
			// device driverdevice driver不要なので、登録を削除
			err = removeDriverService(_T("nodokad"));
		}

		// create shortcut
		tstringi GuiExe, ExeName2, ExeName, FileName;
		int argMK = 0;

		if (m_doNotReviseShortCut)
			goto no_shortcut;

		if (checkOs(SetupFile::AMD64))
		{
			FileName = L"nodoka64.exe";
			if (m_doRegisterLimitVersion)
				FileName = L"nodoka64_limit.exe";
			if (m_doRegisterHILVersion)
				FileName = L"nodoka64_hil.exe";
		}
		else
		{
			FileName = L"nodoka.exe";
			if (m_doRegisterLimitVersion)
				FileName = L"nodoka_limit.exe";
			if (m_doRegisterHILVersion)
				FileName = L"nodoka_hil.exe";
		}
		ExeName = g_destDir + L"\\" + FileName;
		ExeName2 = g_destDir + L"\\dotnet_starter.exe";
		GuiExe = g_destDir + L"\\GuiEdit.exe";

		// 0: none, bit1: -m, bit2: -k, bit3: -n, bit4: -w
		if (m_doRegisterMouseHook)
			argMK += 1;
		if (m_doRegisterKeyboardHook)
			argMK += 2;
		if (m_reloadScancodeMap)
			argMK += 4;
		if (m_doRegisterWin8WA)
			argMK += 8;

		if (m_doRegisterToStartMenu)
		{
			tstringi shortcut = getStartMenuName(loadString(IDS_shortcutName));
			if (!shortcut.empty())
				createLink(ExeName.c_str(), shortcut.c_str(), g_resource->loadString(IDS_shortcutName), g_destDir.c_str(), argMK);

			tstringi shortcutGUI = getStartMenuName(loadString(IDS_shortcutNameGUI));
			if (!shortcutGUI.empty() && !(checkOs(SetupFile::W2k)) && checkDotNet())
				createLink(GuiExe.c_str(), shortcutGUI.c_str(), g_resource->loadString(IDS_shortcutNameGUI), g_destDir.c_str(), 0);
		}
		if (m_doRegisterToStartUp)
		{
			tstringi shortcut = getStartUpName(loadString(IDS_shortcutName));
			if (!shortcut.empty())
				createLink(ExeName.c_str(), shortcut.c_str(), g_resource->loadString(IDS_shortcutName), g_destDir.c_str(), argMK);
		}
		if (m_doRegisterToStartUp2) // dotnet_starter の登録
		{
			tstringi shortcut = getStartUpName(loadString(IDS_shortcutName2));

			// dotnet_starter.exe のショートカットがあるなら削除する。
			// DeleteFile(getStartUpName(g_resource->loadString(IDS_shortcutName2)).c_str());

			// dotnet_starter.exe のショートカットを設定する。
			if (!shortcut.empty())
				createLink(ExeName2.c_str(), shortcut.c_str(), g_resource->loadString(IDS_shortcutName2), g_destDir.c_str(), 0);

			//  ngen.exeをdotnet_starter.exe, GuiEdit.exeに対して実行する。
			dongen(ExeName2.c_str());
			dongen(GuiExe.c_str());
		}
		else
		{
			tstringi shortcut = getStartUpName(loadString(IDS_shortcutName2));

			// dotnet_starter.exe のショートカットがあるなら削除する。
			DeleteFile(getStartUpName(g_resource->loadString(IDS_shortcutName2)).c_str());
		}
		if (m_doRegisterToStartUp3) // desktopへの登録
		{
			tstringi shortcut = getDeskTopName(loadString(IDS_shortcutName));
			if (!shortcut.empty())
				createLink(ExeName.c_str(), shortcut.c_str(), g_resource->loadString(IDS_shortcutName), g_destDir.c_str(), argMK);

			tstringi shortcutGUI = getDeskTopName(loadString(IDS_shortcutNameGUI));
			if (!shortcutGUI.empty() && !(checkOs(SetupFile::W2k)) && checkDotNet())
				createLink(GuiExe.c_str(), shortcutGUI.c_str(), g_resource->loadString(IDS_shortcutNameGUI), g_destDir.c_str(), 0);
		}

	no_shortcut:
		// set registry
		reg.write(_T("layout"),
				  (g_keyboardKind == KEYBOARD_KIND_109) ? _T("109") : _T("104"));
		reg.write(_T("FileName"), FileName);

		// file extension
		createFileExtension(_T(".nodoka"), _T("text/plain"),
							_T("nodokafile"), g_resource->loadString(IDS_nodokaFile),
							g_destDir + _T("\\nodoka.exe,1"),
							g_resource->loadString(IDS_nodokaShellOpen));

		// uninstall information
		createUninstallInformation(_T("nodoka"), g_resource->loadString(IDS_nodoka),
								   g_destDir + _T("\\setup.exe -u"));

		if (g_flags == Flag_Usb)
		{
			if (message(IDS_copyFinishUsb, MB_YESNO | MB_ICONQUESTION, m_hwnd) == IDYES)
			{
				// reboot ...
				HANDLE hToken;
				// Get a token for this process.
				if (!OpenProcessToken(GetCurrentProcess(),
									  TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
				{
					message(IDS_failedToReboot, MB_OK | MB_ICONSTOP);
					return 0;
				}
				// Get the LUID for the shutdown privilege.
				TOKEN_PRIVILEGES tkp;
				LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
				tkp.PrivilegeCount = 1; // one privilege to set
				tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
				// Get the shutdown privilege for this process.
				AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
									  (PTOKEN_PRIVILEGES)NULL, 0);
				// Cannot test the return value of AdjustTokenPrivileges.
				if (GetLastError() != ERROR_SUCCESS)
				{
					message(IDS_failedToReboot, MB_OK | MB_ICONSTOP);
					return 0;
				}
				// Shut down the system and force all applications to close.
				if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0))
				{
					message(IDS_failedToReboot, MB_OK | MB_ICONSTOP);
					return 0;
				}
			}
		}
		else
		{
			if (message(IDS_copyFinish, MB_YESNO | MB_ICONQUESTION, m_hwnd) == IDYES)
				ExitWindows(0, 0); // logoff
		}
		return 0;
	}

private:
	// WM_INITDIALOG
	BOOL wmInitDialog(HWND /* focus */, LPARAM /* lParam */)
	{
		setSmallIcon(m_hwnd, IDI_ICON_nodoka);
		setBigIcon(m_hwnd, IDI_ICON_nodoka);

		Registry reg(DIR_REGISTRY_ROOT);
		int bCheck;

		// アップデイトならばショートカット/設定ファイル上書きを選択可能とする他、以前の値を取ってくる。
		if (g_update == true)
		{
			reg.read(_T("doNotReviseShortCut"), &bCheck, 0);
			CheckDlgButton(m_hwnd, IDC_CHECKoffShortCut, bCheck);

			reg.read(_T("doNotReviseDotNodoka"), &bCheck, 0);
			CheckDlgButton(m_hwnd, IDC_CHECKoffDotNodoka, bCheck);

			if (IsDlgButtonChecked(m_hwnd, IDC_CHECKoffShortCut) == BST_CHECKED)
			{
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_registerStartMenu), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_registerStartUp), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_registerStartUp3), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_registerStartUp2), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_mouse), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_keyboard), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_limit), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_HIL), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_ScancodeMapReload), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_win8wa), FALSE);
			}
		}
		else
		{
			EnableWindow(GetDlgItem(m_hwnd, IDC_CHECKoffShortCut), FALSE);
			EnableWindow(GetDlgItem(m_hwnd, IDC_CHECKoffDotNodoka), FALSE);
		}

		reg.read(_T("registerStartMenu"), &bCheck, 1);
		CheckDlgButton(m_hwnd, IDC_CHECK_registerStartMenu, bCheck);

		reg.read(_T("registerStartUp"), &bCheck, 1);
		CheckDlgButton(m_hwnd, IDC_CHECK_registerStartUp, bCheck);

		reg.read(_T("registerDeskTop"), &bCheck, 1);
		CheckDlgButton(m_hwnd, IDC_CHECK_registerStartUp3, bCheck);

		reg.read(_T("registerFastDotNet"), &bCheck, 1);
		if (checkOs(SetupFile::W2k) || !checkDotNet())
		{
			CheckDlgButton(m_hwnd, IDC_CHECK_registerStartUp2, BST_UNCHECKED);
			EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_registerStartUp2), FALSE);
		}
		else
		{
			CheckDlgButton(m_hwnd, IDC_CHECK_registerStartUp2, bCheck);
		}

		reg.read(_T("registerMouseHook"), &bCheck, 0);
		CheckDlgButton(m_hwnd, IDC_CHECK_mouse, bCheck);

		reg.read(_T("registerKeyboardHook"), &bCheck, 0);
		CheckDlgButton(m_hwnd, IDC_CHECK_keyboard, bCheck);
		if (bCheck == 1)
		{
			EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_win8wa), TRUE);
		}
		else
		{
			EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_win8wa), FALSE);
		}

		reg.read(_T("registerLimit"), &bCheck, 0);
		CheckDlgButton(m_hwnd, IDC_CHECK_limit, bCheck);

		reg.read(_T("registerHIL"), &bCheck, 0);
		CheckDlgButton(m_hwnd, IDC_CHECK_HIL, bCheck);

		// レジストリの値は使わずに、レジストリを調べてnodokadがあるならチェックを外す。
		//reg.read(_T("doNotRegisterDriver"), &bCheck, 0);
		checkDriverEntry2();
		if (g_useDriver == true)
			CheckDlgButton(m_hwnd, IDC_CHECK_dont_devicedriver, FALSE);

		reg.read(_T("reloadScancodeMap"), &bCheck, 0);
		CheckDlgButton(m_hwnd, IDC_CHECK_ScancodeMapReload, bCheck);

		reg.read(_T("win8wa"), &bCheck, 0);
		CheckDlgButton(m_hwnd, IDC_CHECK_win8wa, bCheck);

		Edit_SetText(GetDlgItem(m_hwnd, IDC_EDIT_path), g_destDir.c_str());

		tstring EnvNODOKA;
		if (GetEnv(_T("NODOKA")) == NULL)
		{
			Edit_SetText(GetDlgItem(m_hwnd, IDC_EDIT_pathNODOKA), _T(""));
		}
		else
		{
			EnvNODOKA = GetEnv(_T("NODOKA"));
			Edit_SetText(GetDlgItem(m_hwnd, IDC_EDIT_pathNODOKA), EnvNODOKA.c_str());
		}
		HWND hwndCombo = GetDlgItem(m_hwnd, IDC_COMBO_keyboard);

		ComboBox_AddString(hwndCombo,
						   g_resource->loadString(IDS_keyboard109usb));
		ComboBox_AddString(hwndCombo,
						   g_resource->loadString(IDS_keyboard104usb));

		ComboBox_SetCurSel(hwndCombo,
						   (g_keyboardKind == KEYBOARD_KIND_109) ? 0 : 1);
		tstring note;
		for (int i = IDS_note01; i <= IDS_note23; ++i)
		{
			note += g_resource->loadString(i);
		}
		Edit_SetText(GetDlgItem(m_hwnd, IDC_EDIT_note), note.c_str());
		return TRUE;
	}

	// WM_CLOSE
	BOOL wmClose()
	{
		EndDialog(m_hwnd, 0);
		return TRUE;
	}

	// WM_COMMAND
	BOOL wmCommand(int /* notify_code */, int i_id, HWND /* hwnd_control */)
	{
		switch (i_id)
		{
		case IDC_BUTTON_browse: // インストール先参照ボタン
		{
			_TCHAR folder[GANA_MAX_PATH];

			BROWSEINFO bi;
			ZeroMemory(&bi, sizeof(bi));
			bi.hwndOwner = m_hwnd;
			bi.pidlRoot = NULL;
			bi.pszDisplayName = folder;
			bi.lpszTitle = g_resource->loadString(IDS_selectDir);
			ITEMIDLIST *browse = SHBrowseForFolder(&bi);
			if (browse != NULL)
			{
				if (SHGetPathFromIDList(browse, folder))
				{
					if (createDirectories(folder))
						Edit_SetText(GetDlgItem(m_hwnd, IDC_EDIT_path), folder);
				}
				IMalloc *imalloc = NULL;
				if (SHGetMalloc(&imalloc) == NOERROR)
					imalloc->Free((void *)browse);
			}
			return TRUE;
		}

		case IDC_BUTTON_browseNODOKA: // 環境変数NODOKA参照ボタン
		{
			_TCHAR folder[GANA_MAX_PATH];

			BROWSEINFO bi;
			ZeroMemory(&bi, sizeof(bi));
			bi.hwndOwner = m_hwnd;
			bi.pidlRoot = NULL;
			bi.pszDisplayName = folder;
			bi.lpszTitle = g_resource->loadString(IDS_selectDirNODOKA);
			ITEMIDLIST *browse = SHBrowseForFolder(&bi);
			if (browse != NULL)
			{
				if (SHGetPathFromIDList(browse, folder))
				{
					if (createDirectories(folder))
						Edit_SetText(GetDlgItem(m_hwnd, IDC_EDIT_pathNODOKA), folder);
				}
				IMalloc *imalloc = NULL;
				if (SHGetMalloc(&imalloc) == NOERROR)
					imalloc->Free((void *)browse);
			}
			return TRUE;
		}

		case IDOK:
		{
			_TCHAR buf[GANA_MAX_PATH];
			_TCHAR buf2[GANA_MAX_PATH];
			Edit_GetText(GetDlgItem(m_hwnd, IDC_EDIT_path), buf, NUMBER_OF(buf));
			Edit_GetText(GetDlgItem(m_hwnd, IDC_EDIT_pathNODOKA), buf2, NUMBER_OF(buf2));
			if (buf[0])
			{
				g_destDir = normalizePath(buf);

				if (buf2[0])
				{
					g_envNODOKA = normalizePath(buf2);
					Registry reg(HKEY_CURRENT_USER, _T("Environment"));
					reg.write(_T("NODOKA"), g_envNODOKA);
				}

				Registry reg(DIR_REGISTRY_ROOT);

				m_doNotReviseShortCut =
					(IsDlgButtonChecked(m_hwnd, IDC_CHECKoffShortCut) ==
					 BST_CHECKED);
				reg.write(_T("doNotReviseShortCut"), (m_doNotReviseShortCut) ? 1 : 0);

				m_doNotReviseDotNodoka =
					(IsDlgButtonChecked(m_hwnd, IDC_CHECKoffDotNodoka) ==
					 BST_CHECKED);
				reg.write(_T("doNotReviseDotNodoka"), (m_doNotReviseDotNodoka) ? 1 : 0);

				m_doRegisterToStartMenu =
					(IsDlgButtonChecked(m_hwnd, IDC_CHECK_registerStartMenu) ==
					 BST_CHECKED);
				reg.write(_T("registerStartMenu"), (m_doRegisterToStartMenu) ? 1 : 0);

				m_doRegisterToStartUp =
					(IsDlgButtonChecked(m_hwnd, IDC_CHECK_registerStartUp) ==
					 BST_CHECKED);
				reg.write(_T("registerStartUp"), (m_doRegisterToStartUp) ? 1 : 0);

				m_doRegisterToStartUp3 =
					(IsDlgButtonChecked(m_hwnd, IDC_CHECK_registerStartUp3) ==
					 BST_CHECKED);
				reg.write(_T("registerDeskTop"), (m_doRegisterToStartUp3) ? 1 : 0);

				m_doRegisterToStartUp2 =
					(IsDlgButtonChecked(m_hwnd, IDC_CHECK_registerStartUp2) ==
					 BST_CHECKED);
				reg.write(_T("registerFastDotNet"), (m_doRegisterToStartUp2) ? 1 : 0);

				m_doRegisterMouseHook =
					(IsDlgButtonChecked(m_hwnd, IDC_CHECK_mouse) ==
					 BST_CHECKED);
				reg.write(_T("registerMouseHook"), (m_doRegisterMouseHook) ? 1 : 0);

				m_doRegisterKeyboardHook =
					(IsDlgButtonChecked(m_hwnd, IDC_CHECK_keyboard) ==
					 BST_CHECKED);
				reg.write(_T("registerKeyboardHook"), (m_doRegisterKeyboardHook) ? 1 : 0);

				m_doNotRegisterDriver =
					(IsDlgButtonChecked(m_hwnd, IDC_CHECK_dont_devicedriver) ==
					 BST_CHECKED);
				reg.write(_T("doNotRegisterDriver"), (m_doNotRegisterDriver) ? 1 : 0);

				m_reloadScancodeMap =
					(IsDlgButtonChecked(m_hwnd, IDC_CHECK_ScancodeMapReload) ==
					 BST_CHECKED);
				reg.write(_T("reloadScancodeMap"), (m_reloadScancodeMap) ? 1 : 0);

				m_doRegisterLimitVersion =
					(IsDlgButtonChecked(m_hwnd, IDC_CHECK_limit) ==
					 BST_CHECKED);
				reg.write(_T("registerLimit"), (m_doRegisterLimitVersion) ? 1 : 0);

				m_doRegisterHILVersion =
					(IsDlgButtonChecked(m_hwnd, IDC_CHECK_HIL) ==
					 BST_CHECKED);
				reg.write(_T("registerHIL"), (m_doRegisterHILVersion) ? 1 : 0);

				m_doRegisterWin8WA =
					(IsDlgButtonChecked(m_hwnd, IDC_CHECK_win8wa) ==
					 BST_CHECKED);
				reg.write(_T("win8wa"), (m_doRegisterWin8WA) ? 1 : 0);

				int curSel =
					ComboBox_GetCurSel(GetDlgItem(m_hwnd, IDC_COMBO_keyboard));
				g_flags = SetupFile::Normal;

				switch (curSel)
				{
				case 0:
					g_keyboardKind = KEYBOARD_KIND_109;
					g_flags = Flag_Usb;
					break;
				case 1:
					g_keyboardKind = KEYBOARD_KIND_104;
					g_flags = Flag_Usb;
					break;
				}

				if (createDirectories(g_destDir.c_str()))
					EndDialog(m_hwnd, install(m_doNotRegisterDriver, m_doNotReviseDotNodoka));
				else
					message(IDS_invalidDirectory, MB_OK | MB_ICONSTOP, m_hwnd);
			}
			else
				message(IDS_nodokaEmpty, MB_OK, m_hwnd);
			return TRUE;
		}

		case IDCANCEL:
		{
			CHECK_TRUE(EndDialog(m_hwnd, 0));
			return TRUE;
		}
		case IDC_CHECK_limit:
		{
			if (IsDlgButtonChecked(m_hwnd, IDC_CHECK_limit) == BST_CHECKED)
			{
				CheckDlgButton(m_hwnd, IDC_CHECK_limit, BST_UNCHECKED); // clear
			}
			else
			{
				CheckDlgButton(m_hwnd, IDC_CHECK_limit, BST_CHECKED); // set
				CheckDlgButton(m_hwnd, IDC_CHECK_HIL, BST_UNCHECKED); // clear
			}
			return TRUE;
		}
		case IDC_CHECK_HIL:
		{
			if (IsDlgButtonChecked(m_hwnd, IDC_CHECK_HIL) == BST_CHECKED)
			{
				CheckDlgButton(m_hwnd, IDC_CHECK_HIL, BST_UNCHECKED); // clear
			}
			else
			{
				CheckDlgButton(m_hwnd, IDC_CHECK_HIL, BST_CHECKED);		// set
				CheckDlgButton(m_hwnd, IDC_CHECK_limit, BST_UNCHECKED); // clear
			}
			return TRUE;
		}
		case IDC_CHECK_dont_devicedriver:
		{
			if (IsDlgButtonChecked(m_hwnd, IDC_CHECK_dont_devicedriver) == BST_CHECKED)
			{
				CheckDlgButton(m_hwnd, IDC_CHECK_keyboard, BST_CHECKED); // set
			}
			return TRUE;
		}
		case IDC_CHECK_keyboard:
		{
			if (IsDlgButtonChecked(m_hwnd, IDC_CHECK_keyboard) == BST_CHECKED)
			{
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_win8wa), TRUE);
			}
			else
			{
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_win8wa), FALSE);
				CheckDlgButton(m_hwnd, IDC_CHECK_win8wa, BST_UNCHECKED); // clear
			}
			return TRUE;
		}
		case IDC_CHECKoffShortCut:
		{
			if (IsDlgButtonChecked(m_hwnd, IDC_CHECKoffShortCut) == BST_CHECKED)
			{
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_registerStartMenu), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_registerStartUp), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_registerStartUp3), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_registerStartUp2), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_mouse), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_keyboard), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_limit), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_HIL), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_ScancodeMapReload), FALSE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_win8wa), FALSE);
			}
			else
			{
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_registerStartMenu), TRUE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_registerStartUp), TRUE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_registerStartUp3), TRUE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_registerStartUp2), TRUE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_mouse), TRUE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_keyboard), TRUE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_limit), TRUE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_HIL), TRUE);
				EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_ScancodeMapReload), TRUE);

				if (IsDlgButtonChecked(m_hwnd, IDC_CHECK_keyboard) == BST_CHECKED)
				{
					EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_win8wa), TRUE);
				}
				else
				{
					EnableWindow(GetDlgItem(m_hwnd, IDC_CHECK_win8wa), FALSE);
				}
			}
			return TRUE;
		}
		}
		return FALSE;
	}

public:
	DlgMain(HWND i_hwnd)
		: m_hwnd(i_hwnd),
		  m_doRegisterToStartMenu(false),
		  m_doRegisterToStartUp(false),
		  m_doRegisterToStartUp2(false),
		  m_doRegisterToStartUp3(false),
		  m_doRegisterMouseHook(false),
		  m_doRegisterKeyboardHook(false),
		  m_doNotRegisterDriver(false),
		  m_doRegisterLimitVersion(false),
		  m_doRegisterHILVersion(false),
		  m_reloadScancodeMap(false)
	{
	}

	static BOOL CALLBACK dlgProc(HWND i_hwnd, UINT i_message,
								 WPARAM i_wParam, LPARAM i_lParam)
	{
		DlgMain *wc;
		getUserData(i_hwnd, &wc);
		if (!wc)
			switch (i_message)
			{
			case WM_INITDIALOG:
				wc = setUserData(i_hwnd, new DlgMain(i_hwnd));
				return wc->wmInitDialog(reinterpret_cast<HWND>(i_wParam), i_lParam);
			}
		else
			switch (i_message)
			{
			case WM_COMMAND:
				return wc->wmCommand(HIWORD(i_wParam), LOWORD(i_wParam),
									 reinterpret_cast<HWND>(i_lParam));
			case WM_CLOSE:
				return wc->wmClose();
			case WM_NCDESTROY:
				delete wc;
				return TRUE;
			}
		return FALSE;
	}
};

// uninstall
// (in this function, we cannot use any resource, so we use strres[])
int uninstall()
{
	if (IDYES != message(IDS_removeOk, MB_YESNO | MB_ICONQUESTION))
		return 1;

#if defined(_WINNT)
	DWORD err = removeDriverService(_T("nodokad"));
	/* errorになっても先に進める。インストールしていない可能性があるため。
	if (err != ERROR_SUCCESS)
		{
		driverServiceError(err);
		return 1;
		}
	*/

	// DriverManagerの戻り値に関わらず、UpperFiltersから確実に削除する冗長な安全網。
	// kbdaddidの不完全なアンインストールでUpperFiltersに死んだ参照が残留した事故の再発防止。
	forceRemoveUpperFiltersEntry(_T("nodokad"));
#endif // _WINNT

	PVOID oldValue;
	disableWow64FsRedir(&oldValue);

	BOOL bFlag = DeleteFile(getStartMenuName(g_resource->loadString(IDS_shortcutName)).c_str());

#if 0
	if(!bFlag)
		{
		LPVOID lpMsgBuf;
		FormatMessage( 
			FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			FORMAT_MESSAGE_FROM_SYSTEM | 
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			GetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
			(LPTSTR) &lpMsgBuf,
			0,
			NULL 
			);

		MessageBox(NULL, (LPCTSTR)lpMsgBuf, L"debug", MB_OK);
		LocalFree( lpMsgBuf );
		}

	MessageBox(NULL, getStartMenuName(g_resource->loadString(IDS_shortcutName)).c_str(), L"debug", MB_OK);
#endif

	DeleteFile(getStartUpName(g_resource->loadString(IDS_shortcutName)).c_str());
	DeleteFile(getStartUpName(g_resource->loadString(IDS_shortcutName2)).c_str());
	DeleteFile(getDeskTopName(g_resource->loadString(IDS_shortcutName)).c_str());

	removeFiles(g_setupFiles, NUMBER_OF(g_setupFiles), g_flags, g_destDir);
	removeFileExtension(_T(".nodoka"), _T("nodokafile"));
	removeUninstallInformation(_T("nodoka"));

	Registry::remove(DIR_REGISTRY_ROOT);
	Registry::remove(HKEY_CURRENT_USER, _T("Software\\appletkan\\nodoka"));

	revertWow64FsRedir(oldValue);

	message(IDS_removeFinish, MB_OK | MB_ICONINFORMATION);
	return 0;
}

int WINAPI _tWinMain(HINSTANCE i_hInstance, HINSTANCE /* hPrevInstance */,
					 LPTSTR /* lpszCmdLine */, int /* nCmdShow */)
{
	CoInitialize(NULL);

	g_hInst = i_hInstance;
	Resource resource(g_strres);
	g_resource = &resource;
	bool bToGo = false;
	HANDLE mutex = NULL;
	HANDLE mutexPrevVer = NULL;
	HWND hwnd;

	// check OS
	if (!checkOs(SetupFile::NT))
	{
		message(IDS_invalidOS, MB_OK | MB_ICONSTOP);
		return 1;
	}
#ifndef _WIN64 // is setup.exe, Not setup64.exe
	if (checkOs(SetupFile::AMD64))
	{
		SHELLEXECUTEINFO shExecInfo;
		tstringi curDir = getModuleDirectory();

		shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);

		shExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		shExecInfo.hwnd = NULL;
		shExecInfo.lpVerb = L"open";
		shExecInfo.lpFile = L"setup64.exe";
		shExecInfo.lpDirectory = curDir.c_str();
		shExecInfo.nShow = SW_SHOWNORMAL;
		shExecInfo.hInstApp = NULL;

		if (__argc == 3 && _tcsicmp(__targv[1], _T("-u")) == 0)
		{
			shExecInfo.lpParameters = _T("-u");
		}
		else if (__argc == 2 && _tcsicmp(__targv[1], _T("-u")) == 0)
		{
			shExecInfo.lpParameters = _T("-u");
		}
		else if (__argc == 2 && _tcsicmp(__targv[1], _T("-s")) == 0)
		{
			shExecInfo.lpParameters = _T("-s");
		}
		else if (__argc == 1)
		{
			shExecInfo.lpParameters = NULL;
		}

		ShellExecuteEx(&shExecInfo);

		WaitForSingleObject(shExecInfo.hProcess, INFINITE); // wait exit

		return 0; // exit setup.exe
	}
#endif

	// keyboard kind
	// システムロケールを参照していったんlayoutを決める。その後レジストリ設定があれば、それを使う。

	g_keyboardKind = (resource.getLocale() == LOCALE_Japanese_Japan_932) ? KEYBOARD_KIND_109 : KEYBOARD_KIND_104;

	tstring layout104109;
	Registry::read(DIR_REGISTRY_ROOT, _T("layout"), &layout104109, _T(""));

	// レジストリにlayoutがあるなら上書きインストールなのでフラグをセットする。
	if (layout104109 == _T(""))
		g_update = false;
	else
		g_update = true;

	if (layout104109 == _T("109"))
		g_keyboardKind = KEYBOARD_KIND_109;
	if (layout104109 == _T("104"))
		g_keyboardKind = KEYBOARD_KIND_104;

	// インストール先
	tstring programFiles; // "Program Files" directory

	// OS既定の ProgramFilesを取得
	if (GetEnv(_T("ProgramW6432")) == NULL)
	{
		// ProgramW6432が無い場合は、ProgramFilesを参照。
		programFiles = GetEnv(_T("ProgramFiles"));
	}
	else
	{
		// ProgramW6432 を参照。WoW64環境で、(x86)がつかない方。
		programFiles = GetEnv(_T("ProgramW6432"));
	}

	// 前回のインストール先のフォルダ名を取得。もしなかったら、OS規定+\nodoka にする。
	Registry::read(DIR_REGISTRY_ROOT, _T("dir"), &g_destDir, programFiles + _T("\\nodoka"));

	int retval = 1;

	if (__argc == 2 && _tcsicmp(__targv[1], _T("-u")) == 0)
		retval = uninstallStep1(_T("-u"));
	else
	{
		bToGo = true;

		mutexPrevVer = CreateMutex((SECURITY_ATTRIBUTES *)NULL, TRUE, MUTEX_NODOKA_EXCLUSIVE_RUNNING);
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{ // nodoka is running
			if (IDOK == (message(IDS_nodokaRunning, MB_OKCANCEL | MB_ICONQUESTION)))
			{
				// のどか,窓使いの憂鬱の終了
				ExitNodoka();

				// 再度確認
				hwnd = FindWindow(_T("nodokaTasktray"), NULL);
				if (hwnd)
					bToGo = false;
				else
					bToGo = true;
			}
			else
			{
				bToGo = false;
			}
		}
		else
		{
			bToGo = true;
			// is nodoka running ?
			mutex = CreateMutex((SECURITY_ATTRIBUTES *)NULL, TRUE, addSessionId(MUTEX_NODOKA_EXCLUSIVE_RUNNING).c_str());
			if (GetLastError() == ERROR_ALREADY_EXISTS)
			{ // nodoka is running
				if (IDOK == (message(IDS_nodokaRunning, MB_OKCANCEL | MB_ICONQUESTION)))
				{
					// のどか,窓使いの憂鬱の終了
					ExitNodoka();

					// 再度確認
					hwnd = FindWindow(_T("nodokaTasktray"), NULL);
					if (hwnd)
						bToGo = false;
					else
						bToGo = true;
				}
				else
				{
					bToGo = false;
				}
			}
		}

		if (bToGo)
		{
			if (__argc == 3 && _tcsicmp(__targv[1], _T("-u")) == 0)
			{
				uninstallStep2(__targv[2]);
				retval = uninstall();
			}
			else if (__argc == 2 && _tcsicmp(__targv[1], _T("-s")) == 0)
			{
				g_wasExecutedBySFX = true;
				if (checkDriverEntry())
					retval = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_main), NULL, (DLGPROC)(DlgMain::dlgProc));
			}
			else if (__argc == 1)
			{
				if (checkDriverEntry())
					retval = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_main), NULL, (DLGPROC)(DlgMain::dlgProc));
			}
		}
	}
	if (mutex != NULL)
		CloseHandle(mutex);
	if (mutexPrevVer != NULL)
		CloseHandle(mutexPrevVer);

	return retval;
}
