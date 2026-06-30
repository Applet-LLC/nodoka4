//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting.cpp
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#include "misc.h"

#include "dlgsetting.h"
#include "errormessage.h"
#include "nodoka.h"
#include "nodokarc.h"
#include "registry.h"
#include "setting.h"
#include "windowstool.h"
#include "vkeytable.h"
#include "array.h"
#include "rawinput.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#include <windows.h>

// ...
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sys/stat.h>

#define DBG_PRINT_LENGTH 1440

inline void DBG_PRINT(const _TCHAR *fmt, ...)
{
	_TCHAR buf[DBG_PRINT_LENGTH];
	va_list ap;
	va_start(ap, fmt);
	_vsntprintf_s(buf, DBG_PRINT_LENGTH, _TRUNCATE, fmt, ap);
	va_end(ap);
	OutputDebugString(buf);
}

typedef UINT(CALLBACK *FUNCTYPE1)(PRAWINPUTDEVICELIST, PUINT, UINT);
static FUNCTYPE1 myGetRawInputDeviceList = (FUNCTYPE1)GetProcAddress(GetModuleHandle(L"user32.dll"), "GetRawInputDeviceList");

typedef UINT(CALLBACK *FUNCTYPE2)(HANDLE, UINT, LPVOID, PUINT);
static FUNCTYPE2 myGetRawInputDeviceInfo = (FUNCTYPE2)GetProcAddress(GetModuleHandle(L"user32.dll"), "GetRawInputDeviceInfoW");

typedef BOOL(CALLBACK *FUNCTYPE3)(PRAWINPUTDEVICE, UINT, UINT);
static FUNCTYPE3 myRegisterRawInputDevices = (FUNCTYPE3)GetProcAddress(GetModuleHandle(L"user32.dll"), "RegisterRawInputDevices");

typedef DWORD(WINAPI *GetCompressedFileSizeW_t)(LPCWSTR lpFileName, LPDWORD lpFileSizeHigh);
static GetCompressedFileSizeW_t myGetCompressedFileSizeW =
	(GetCompressedFileSizeW_t)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "GetCompressedFileSizeW");

// FNV-1a 16-bit hash — kbdaddid.sys の Fnv1aHash16Upper と同一アルゴリズム (user-mode 版)
// 戻り値は上位 16bit にハッシュ値を格納した ULONG (= kbdaddid の DeviceId 形式)
static ULONG KbdAddId_Fnv1aHash16Upper(const WCHAR* str)
{
	if (str == NULL) return 0;
	const unsigned short offset_basis = 0x9DC5;
	const unsigned short FNV_prime16  = 0x0193;
	unsigned short hash = offset_basis;
	while (*str) {
		WCHAR ch = *str++;
		unsigned char b0 = (unsigned char)(ch & 0xFF);
		unsigned char b1 = (unsigned char)((ch >> 8) & 0xFF);
		hash ^= b0; hash = (unsigned short)(hash * FNV_prime16);
		hash ^= b1; hash = (unsigned short)(hash * FNV_prime16);
	}
	return ((ULONG)hash) << 16;
}

// RIDI_DEVICENAME から \\?\ / \\??\ プレフィックスと #{GUID} サフィックスを除去し
// # を \ に置換した文字列 (デバイスインスタンスID 形式) をバッファに書き込む。
// 大文字化はしない (呼び出し側で用途に応じて行う)。
static bool KbdAddId_ExtractInstanceId(const WCHAR* ridiDeviceName, WCHAR* outBuf, size_t outBufCount)
{
	const WCHAR* p = ridiDeviceName;
	if (wcsncmp(p, L"\\\\?\\", 4) == 0)       p += 4;
	else if (wcsncmp(p, L"\\\\??\\", 5) == 0)  p += 5;

	wcsncpy_s(outBuf, outBufCount, p, outBufCount - 1);
	outBuf[outBufCount - 1] = L'\0';

	WCHAR* guid = wcsstr(outBuf, L"#{");
	if (guid) *guid = L'\0';

	for (WCHAR* q = outBuf; *q; q++)
		if (*q == L'#') *q = L'\\';

	return outBuf[0] != L'\0';
}

// DeviceIdMode=0: デバイスインスタンスID のハッシュ
// kbdaddid の IoGetDevicePropertyData(DEVPKEY_Device_InstanceId) はレジストリに保存されたケース
// (例: "9&b9b90bd&0&0000" のような小文字混じり) のまま返す。
// RIDI_DEVICENAME も同じケースを使うため、大文字化せずそのままハッシュする。
static ULONG KbdAddId_ComputeExtraInfoByInstanceId(const WCHAR* ridiDeviceName)
{
	WCHAR buf[512];
	if (!KbdAddId_ExtractInstanceId(ridiDeviceName, buf, _countof(buf)))
		return 0;
	// 大文字化しない: kbdaddid も as-is でハッシュする
	return KbdAddId_Fnv1aHash16Upper(buf);
}

// DeviceIdMode=1: HardwareID (REG_MULTI_SZ の先頭エントリ) のハッシュ
// HKLM\SYSTEM\CurrentControlSet\Enum\{instanceId}\HardwareID を読む。
// kbdaddid の IoGetDeviceProperty(DevicePropertyHardwareId) と同一データ。
static ULONG KbdAddId_ComputeExtraInfoByHardwareId(const WCHAR* ridiDeviceName)
{
	WCHAR instId[512];
	if (!KbdAddId_ExtractInstanceId(ridiDeviceName, instId, _countof(instId)))
		return 0;

	// レジストリキー: HKLM\SYSTEM\CurrentControlSet\Enum\{instId}\HardwareID
	// キーパス検索はケース非依存なので大文字化不要
	WCHAR regPath[600] = L"SYSTEM\\CurrentControlSet\\Enum\\";
	wcscat_s(regPath, _countof(regPath), instId);

	HKEY hKey;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		return 0;

	// REG_MULTI_SZ: 複数エントリが \0 区切りで並ぶ。先頭エントリが最詳細なハードウェアID。
	WCHAR hwid[512] = {};
	DWORD hwidLen = sizeof(hwid);
	DWORD type = 0;
	LSTATUS st = RegQueryValueExW(hKey, L"HardwareID", NULL, &type, (LPBYTE)hwid, &hwidLen);
	RegCloseKey(hKey);

	if (st != ERROR_SUCCESS || hwid[0] == L'\0')
		return 0;

	// kbdaddid はレジストリ値をそのまま (大文字化せず) ハッシュする。
	// Windows の HardwareID は通常大文字だが、念のためここでは as-is で渡す。
	return KbdAddId_Fnv1aHash16Upper(hwid);
}

namespace Event
{
Key prefixed(_T("prefixed"));
Key before_key_down(_T("before-key-down"));
Key after_key_up(_T("after-key-up"));
Key *events[] =
	{
		&prefixed,
		&before_key_down,
		&after_key_up,
		NULL,
};
} // namespace Event

// get nodoka filename
static bool getFilenameFromRegistry(
	tstringi *o_name, tstringi *o_filename, Setting::Symbols *o_symbols)
{
	Registry reg(NODOKA_REGISTRY_ROOT);
	int index;
	reg.read(_T(".nodokaIndex"), &index, 0);
	_TCHAR buf[100];
	_sntprintf_s(buf, NUMBER_OF(buf), _TRUNCATE, _T(".nodoka%d"), index);

	tstringi entry;
	if (!reg.read(buf, &entry))
		return false;

	tregex getFilename(_T("^([^;]*);([^;]*);(.*)$"));
	tsmatch getFilenameResult;
	if (!boost::regex_match(entry, getFilenameResult, getFilename))
		return false;

	if (o_name)
		*o_name = getFilenameResult.str(1);
	if (o_filename)
		*o_filename = getFilenameResult.str(2);
	if (o_symbols)
	{
		tstringi symbols = getFilenameResult.str(3);
		tregex symbol(_T("-D([^;]*)(.*)$"));
		tsmatch symbolResult;
		while (boost::regex_search(symbols, symbolResult, symbol))
		{
			o_symbols->insert(symbolResult.str(1));
			symbols = symbolResult.str(2);
		}
	}
	return true;
}

// set home directory path to reg
static void setFolderPathToRegistry(tstring value)
{
	_TCHAR buf[100];

	_sntprintf_s(buf, NUMBER_OF(buf), _TRUNCATE, _T("currentPath"));

	Registry reg(NODOKA_REGISTRY_ROOT);
	reg.write(buf, value);
}

// get home directory path
void getHomeDirectories(HomeDirectories *o_pathes)
{
	tstringi filename;
	if (getFilenameFromRegistry(NULL, &filename, NULL) &&
		!filename.empty())
	{
		tregex getPath(_T("^(.*[/\\\\])[^/\\\\]*$"));
		tsmatch getPathResult;
		if (boost::regex_match(filename, getPathResult, getPath))
			o_pathes->push_back(getPathResult.str(1));
	}
	const _TCHAR *nodoka = GetEnv(_T("NODOKA"));
	if (nodoka)
		o_pathes->push_back(nodoka);

	const _TCHAR *home = GetEnv(_T("HOME"));
	if (home)
		o_pathes->push_back(home);

	const _TCHAR *userprofile = GetEnv(_T("USERPROFILE"));
	if (userprofile)
		o_pathes->push_back(userprofile);

	const _TCHAR *homedrive = GetEnv(_T("HOMEDRIVE"));
	const _TCHAR *homepath = GetEnv(_T("HOMEPATH"));
	if (homedrive && homepath)
		o_pathes->push_back(tstringi(homedrive) + homepath);

	_TCHAR buf[GANA_MAX_PATH];

#if 0
	DWORD len = GetCurrentDirectory(NUMBER_OF(buf), buf);
	if (0 < len && len < NUMBER_OF(buf))
		o_pathes->push_back(buf);
#endif

	if (GetModuleFileName(GetModuleHandle(NULL), buf, NUMBER_OF(buf)))
	{
		tstring currentPath = pathRemoveFileSpec(buf);
		o_pathes->push_back(currentPath);
		setFolderPathToRegistry(currentPath);
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// SettingLoader

// is there no more tokens ?
bool SettingLoader::isEOL()
{
	return m_ti == m_tokens.end();
}

// get next token
Token *SettingLoader::getToken()
{
	if (isEOL())
		throw ErrorMessage() << _T("too few words.");
	return &*(m_ti++);
}

// look next token
Token *SettingLoader::lookToken()
{
	if (isEOL())
		throw ErrorMessage() << _T("too few words.");
	return &*m_ti;
}

// argument "("
bool SettingLoader::getOpenParen(bool i_doesThrow, const _TCHAR *i_name)
{
	if (!isEOL() && lookToken()->isOpenParen())
	{
		getToken();
		return true;
	}
	if (i_doesThrow)
		throw ErrorMessage() << _T("there must be `(' after `&")
							 << i_name << _T("'.");
	return false;
}

// argument ")"
bool SettingLoader::getCloseParen(bool i_doesThrow, const _TCHAR *i_name)
{
	if (!isEOL() && lookToken()->isCloseParen())
	{
		getToken();
		return true;
	}
	if (i_doesThrow)
		throw ErrorMessage() << _T("`&") << i_name
							 << _T("': too many arguments.");
	return false;
}

// argument ","
bool SettingLoader::getComma(bool i_doesThrow, const _TCHAR *i_name)
{
	if (!isEOL() && lookToken()->isComma())
	{
		getToken();
		return true;
	}
	if (i_doesThrow)
		throw ErrorMessage() << _T("`&") << i_name
							 << _T("': comma expected.");
	return false;
}

// <INCLUDE>
void SettingLoader::load_INCLUDE()
{
	SettingLoader loader(m_soLog, m_log);
	loader.m_defaultAssignModifier = m_defaultAssignModifier;
	loader.m_defaultKeySeqModifier = m_defaultKeySeqModifier;
	if (!loader.load(m_setting, (*getToken()).getString()))
		m_isThereAnyError = true;
}

// <SCAN_CODES>
void SettingLoader::load_SCAN_CODES(Key *o_key)
{
	for (int j = 0; j < Key::MAX_SCAN_CODES_SIZE && !isEOL(); ++j)
#ifndef FOR_LIMIT
	{
		ScanCode sc;
		sc.m_flags = 0;
		while (true)
		{
			Token *t = getToken();
			if (t->isNumber())
			{
				sc.m_scan = (u_char)t->getNumber();
				o_key->addScanCode(sc);
				break;
			}
			if (*t == _T("E0-"))
				sc.m_flags |= ScanCode::E0;
			else if (*t == _T("E1-"))
				sc.m_flags |= ScanCode::E1;
			else if (*t == _T("E0E1-"))
				sc.m_flags |= ScanCode::E0E1;
			else
				throw ErrorMessage() << _T("`") << *t
									 << _T("': invalid modifier.");
		}
	}
#else
	{
		ScanCode scPause;
		scPause.m_scan = 0x1d;
		scPause.m_flags = ScanCode::E1;

		ScanCode sc;
		sc.m_flags = 0;
		unsigned int uPause = 0;
		while (true)
		{
			Token *t = getToken();
			if (t->isNumber())
			{
				sc.m_scan = (u_char)t->getNumber();
				if (uPause == 1 && sc.m_scan == 0x1d)
				{ // First scancode as E1-0x1d;
					uPause = 2;
					break;
				}
				if (uPause == 0 || uPause == 1)
				{ // add only 1 scancode
					o_key->addScanCode(sc);
					uPause = 9;
					break;
				}
				if (uPause == 2 && sc.m_scan == 0x45)
				{ // we found Pause
					o_key->addScanCode(scPause);
					o_key->addScanCode(sc);
					break;
				}
			}
			if (*t == _T("E0-"))
				sc.m_flags |= ScanCode::E0;
			else if (*t == _T("E1-"))
				sc.m_flags |= ScanCode::E1;
			else if (*t == _T("E0E1-"))
				sc.m_flags |= ScanCode::E0E1;
			else
				throw ErrorMessage() << _T("`") << *t
									 << _T("': invalid modifier.");
			if (sc.m_flags & ScanCode::E1)
				uPause = 1;
		}
	}
#endif
}

// <DEFINE_KEY>
void SettingLoader::load_DEFINE_KEY()
{
	Token *t = getToken();
	Key key;

	// <KEY_NAMES>
	if (*t == _T('('))
	{
		key.addName(getToken()->getString());
		while (t = getToken(), *t != _T(')'))
			key.addName(t->getString());
		if (*getToken() != _T("="))
			throw ErrorMessage() << _T("there must be `=' after `)'.");
	}
	else
	{
		key.addName(t->getString());
		while (t = getToken(), *t != _T("="))
			key.addName(t->getString());
	}

	load_SCAN_CODES(&key);
	m_setting->m_keyboard.addKey(key);
}

// <DEFINE_MODIFIER>
void SettingLoader::load_DEFINE_MODIFIER()
{
	Token *t = getToken();
	Modifier::Type mt;
	if (*t == _T("shift") ||
		*t == _T("S"))
		mt = Modifier::Type_Shift;
	else if (*t == _T("alt") ||
			 *t == _T("meta") ||
			 *t == _T("menu") ||
			 *t == _T("A") ||
			 *t == _T("M"))
		mt = Modifier::Type_Alt;
	else if (*t == _T("control") ||
			 *t == _T("ctrl") ||
			 *t == _T("C"))
		mt = Modifier::Type_Control;
	else if (*t == _T("windows") ||
			 *t == _T("win") ||
			 *t == _T("W"))
		mt = Modifier::Type_Windows;
	else
		throw ErrorMessage() << _T("`") << *t
							 << _T("': invalid modifier name.");

	if (*getToken() != _T("="))
		throw ErrorMessage() << _T("there must be `=' after modifier name.");

	while (!isEOL())
	{
		t = getToken();
		Key *key =
			m_setting->m_keyboard.searchKeyByNonAliasName(t->getString());
		if (!key)
			throw ErrorMessage() << _T("`") << *t << _T("': invalid key name @ define modifier.");
		m_setting->m_keyboard.addModifier(mt, key);
	}
}

// <DEFINE_SYNC_KEY>
void SettingLoader::load_DEFINE_SYNC_KEY()
{
	Key *key = m_setting->m_keyboard.getSyncKey();
	key->initialize();
	key->addName(_T("sync"));

	if (*getToken() != _T("="))
		throw ErrorMessage() << _T("there must be `=' after `sync'.");

	load_SCAN_CODES(key);
}

// <DEFINE_ALIAS>
void SettingLoader::load_DEFINE_ALIAS()
{
	Token *name = getToken();

	if (*getToken() != _T("="))
		throw ErrorMessage() << _T("there must be `=' after `alias'.");

	Token *t = getToken();
	Key *key = m_setting->m_keyboard.searchKeyByNonAliasName(t->getString());
	if (!key)
		throw ErrorMessage() << _T("`") << *t << _T("': invalid key name @ define alias");
	m_setting->m_keyboard.addAlias(name->getString(), key);
}

// <DEFINE_SUBSTITUTE>
void SettingLoader::load_DEFINE_SUBSTITUTE()
{
	typedef std::list<ModifiedKey> AssignedKeys;
	AssignedKeys assignedKeys;
	do
	{
		ModifiedKey mkey;
		mkey.m_modifier =
			load_MODIFIER(Modifier::Type_ASSIGN, m_defaultAssignModifier);
		mkey.m_key = load_KEY_NAME();
		assignedKeys.push_back(mkey);
	} while (!(*lookToken() == _T("=>") || *lookToken() == _T("=")));
	getToken();

	KeySeq *keySeq = load_KEY_SEQUENCE(_T(""), false, Modifier::Type_ASSIGN);
	ModifiedKey mkey = keySeq->getFirstModifiedKey();
	if (!mkey.m_key)
		throw ErrorMessage() << _T("no key is specified for substitute.");

	for (AssignedKeys::iterator i = assignedKeys.begin();
		 i != assignedKeys.end(); ++i)
		m_setting->m_keyboard.addSubstitute(*i, mkey);
}

// <DEFINE_OPTION>
void SettingLoader::load_DEFINE_OPTION()
{
	Token *t = getToken();
	if (*t == _T("KL-"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage() << _T("there must be `=' after `def option KL-'.");
		}

		load_ARGUMENT(&m_setting->m_correctKanaLockHandling);
	}
	else if (*t == _T("delay-of"))
	{
		if (*getToken() != _T("!!!"))
		{
			throw ErrorMessage()
				<< _T("there must be `!!!' after `def option delay-of'.");
		}

		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option delay-of !!!'.");
		}

		load_ARGUMENT(&m_setting->m_oneShotRepeatableDelay);
	}
	else if (*t == _T("sts4nodoka"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option sts4nodoka'.");
		}

		load_ARGUMENT(&m_setting->m_sts4nodoka);
		Registry reg(NODOKA_REGISTRY_ROOT);
		int CenterVal = *(&m_setting->m_CenterVal);
		reg.write(_T("CenterVal"), CenterVal);
	}
	else if (*t == _T("cts4nodoka"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option cts4nodoka'.");
		}

		load_ARGUMENT(&m_setting->m_cts4nodoka);
		Registry reg(NODOKA_REGISTRY_ROOT);
		int CenterVal = *(&m_setting->m_CenterVal);
		reg.write(_T("CenterVal"), CenterVal);
	}
	else if (*t == _T("ats4nodoka"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option ats4nodoka'.");
		}

		load_ARGUMENT(&m_setting->m_ats4nodoka);
		Registry reg(NODOKA_REGISTRY_ROOT);
		int CenterVal = *(&m_setting->m_CenterVal);
		reg.write(_T("CenterVal"), CenterVal);
	}
	else if (*t == _T("gamepad"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option gamepad'.");
		}

		load_ARGUMENT(&m_setting->m_gamepad);
		load_ARGUMENT(&m_setting->m_maxVALUE);
		load_ARGUMENT(&m_setting->m_thVALUE);
		load_ARGUMENT(&m_setting->m_deadzoneVALUE);
		load_ARGUMENT(&m_setting->m_REPEAT_TIMES_1);
		load_ARGUMENT(&m_setting->m_REPEAT_TIMES_2);
		load_ARGUMENT(&m_setting->m_WAIT);
		load_ARGUMENT(&m_setting->m_REPEAT_FLAG_PAD);
		load_ARGUMENT(&m_setting->m_REPEAT_FLAG_HAT);
		load_ARGUMENT(&m_setting->m_REPEAT_FLAG_BUTTON);

		Registry reg(NODOKA_REGISTRY_ROOT);
		reg.write(_T("m_maxVALUE"), *(&m_setting->m_maxVALUE));
		reg.write(_T("m_thVALUE"), *(&m_setting->m_thVALUE));
		reg.write(_T("m_deadzoneVALUE"), *(&m_setting->m_deadzoneVALUE));
		reg.write(_T("m_REPEAT_TIMES_1"), *(&m_setting->m_REPEAT_TIMES_1));
		reg.write(_T("m_REPEAT_TIMES_2"), *(&m_setting->m_REPEAT_TIMES_2));
		reg.write(_T("m_WAIT"), *(&m_setting->m_WAIT));
		reg.write(_T("m_REPEAT_FLAG_PAD"), *(&m_setting->m_REPEAT_FLAG_PAD));
		reg.write(_T("m_REPEAT_FLAG_HAT"), *(&m_setting->m_REPEAT_FLAG_HAT));
		reg.write(_T("m_REPEAT_FLAG_BUTTON"), *(&m_setting->m_REPEAT_FLAG_BUTTON));
	}
	else if (*t == _T("CenterVal"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option CenterVal'.");
		}

		load_ARGUMENT(&m_setting->m_CenterVal);
		Registry reg(NODOKA_REGISTRY_ROOT);
		int CenterVal = *(&m_setting->m_CenterVal);
		reg.write(_T("CenterVal"), CenterVal);
	}
#ifndef FOR_LIMIT
	else if (*t == _T("SendTextDelay"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option SendTextDelay'.");
		}

		load_ARGUMENT(&m_setting->m_SendTextDelay);
	}
#endif
	else if (*t == _T("CaretBlinkTime"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option CaretBlinkTime'.");
		}

		load_ARGUMENT(&m_setting->m_CaretBlinkTime);
		load_ARGUMENT(&m_setting->m_BlinkTimeOff);
		load_ARGUMENT(&m_setting->m_BlinkTimeOn);
	}
	else if (*t == _T("mouse-event"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option mouse-event'.");
		}

		load_ARGUMENT(&m_setting->m_mouseEvent);
	}
	else if (*t == _T("drag-threshold"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option drag-threshold'.");
		}

		load_ARGUMENT(&m_setting->m_dragThreshold);
	}

#ifndef FOR_LIMIT
	else if (*t == _T("KeyboardDelay"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option KeyboardDelay'.");
		}
		load_ARGUMENT(&m_setting->m_Repeat);
		load_ARGUMENT(&m_setting->m_DelayA);
		load_ARGUMENT(&m_setting->m_DelayB);

		int i = 0;
		int j = 0;
		int k = 0;
		for (i = 0; i < 7; i++)		// K0からK7
			for (j = 0; j < 4; j++) // none, E0, E1, E0E1
				for (k = 0; k < 256; k++)
				{
					int index = (i * 1024) + (j * 256) + k;
					m_setting->m_keyState[index].state = 0;
					m_setting->m_keyState[index].st1 = 0;
					m_setting->m_keyState[index].st2 = 0;
					m_setting->m_keyState[index].st3 = 0;
					m_setting->m_keyState[index].delayA = m_setting->m_DelayA;
					m_setting->m_keyState[index].delayB = m_setting->m_DelayB;
					m_setting->m_keyState[index].delayI = 0; //未実装
				}
	}

	else if (*t == _T("KeyboardDelayMax"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option KeyboardDelayMax'.");
		}
		load_ARGUMENT(&m_setting->m_DelayMaxFlag);
		load_ARGUMENT(&m_setting->m_DelayMax);
	}
	else if (*t == _T("KeyboardDelayKey"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option KeyboardDelayKey'.");
		}
		// def option KeyboardDelayKey = keyName DelayA DelayB DelayI
		// keyNameを取り出し、DelayA,B,Iをkeytableに入れる。
		USHORT m_flags = 0;
		USHORT m_scan = 0;
		DWORD m_DelayA = 0;
		DWORD m_DelayB = 0;
		DWORD m_DelayI = 0;
		INT m_UnitId = 0;
		while (true)
		{
			Token *t1 = getToken();
			if (t1->isNumber())
			{
				m_scan = (u_char)t1->getNumber();
				break;
			}
			if (*t1 == _T("E0-"))
				m_flags = 1;
			else if (*t1 == _T("E1-"))
				m_flags = 2;
			else if (*t1 == _T("E0E1-"))
				m_flags = 3;
			else
				throw ErrorMessage() << _T("`") << *t1
									 << _T("': invalid ScanCode");
		}
		load_ARGUMENT(&m_DelayA);
		load_ARGUMENT(&m_DelayB);
		load_ARGUMENT(&m_DelayI);
		load_ARGUMENT(&m_UnitId);
		m_setting->m_keyState[(m_UnitId * 1024) + (m_flags * 256) + m_scan].delayA = m_DelayA;
		m_setting->m_keyState[(m_UnitId * 1024) + (m_flags * 256) + m_scan].delayB = m_DelayB;
		m_setting->m_keyState[(m_UnitId * 1024) + (m_flags * 256) + m_scan].delayI = 0; // 未実装
	}
	else if (*t == _T("UseDoublePress"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option UseDoublePress'.");
		}
		load_ARGUMENT(&m_setting->m_UseDoublePress);
		load_ARGUMENT(&m_setting->m_DoublePressPeriod);
		load_ARGUMENT(&m_setting->m_DoublePressDelay);
	}

#endif
	else if (*t == _T("FakeUp"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option FakeUp'.");
		}
		load_ARGUMENT(&m_setting->m_UseFakeUp);
		load_ARGUMENT(&m_setting->m_FakeUpDelay);
		load_ARGUMENT(&m_setting->m_FakeUpKey);
	}

	else if (*t == _T("SixPoint"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option SixPoint'.");
		}
		load_ARGUMENT(&m_setting->m_For6point);
		load_ARGUMENT(&m_setting->m_key1of6);
		load_ARGUMENT(&m_setting->m_key2of6);
		load_ARGUMENT(&m_setting->m_key3of6);
		load_ARGUMENT(&m_setting->m_key4of6);
		load_ARGUMENT(&m_setting->m_key5of6);
		load_ARGUMENT(&m_setting->m_key6of6);
	}

	else if (*t == _T("FocusChange"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option FocusChange'.");
		}
		load_ARGUMENT(&m_setting->m_FocusChange);
	}
	else if (*t == _T("DesktopListView"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option DesktopListView'.");
		}
		int m_ListView = 0;
		load_ARGUMENT(&m_ListView);

		HWND desktop;
		desktop = FindWindowEx(NULL, NULL, L"Progman", L"Program Manager");
		desktop = FindWindowEx(desktop, NULL, L"SHELLDLL_DefView", NULL);
		desktop = FindWindowEx(desktop, NULL, L"SysListView32", NULL);

		if (m_ListView >= 0 && m_ListView <= 4)
			SendMessage(desktop, LVM_SETVIEW, m_ListView, 0);
	}
	else if (*t == _T("CheckModifier"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option CheckModifier'.");
		}
		load_ARGUMENT(&m_setting->m_CheckModifier);
		load_ARGUMENT(&m_setting->m_CheckModifierTime);
	}
	else if (*t == _T("SyncModifierGracePeriod"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option SyncModifierGracePeriod'.");
		}
		load_ARGUMENT(&m_setting->m_SyncModifierGracePeriod);
	}
	else if (*t == _T("ModifierAutoClear"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option ModifierAutoClear'.");
		}
		load_ARGUMENT(&m_setting->m_ModifierAutoClear);
	}
	else if (*t == _T("UseTSF"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option UseTSF'.");
		}
		load_ARGUMENT(&m_setting->m_UseTSF);
	}
	else if (*t == _T("UnitID"))
	{
		if (*getToken() != _T("="))
		{
			throw ErrorMessage()
				<< _T("there must be `=' after `def option UnitID'.");
		}
		// 初回ならRawInputDeviceListを取得し、hDeviceとDeviceIDを得る。
		RAWINPUTDEVICE Rid[4];
		UINT nDevices;
		PRAWINPUTDEVICELIST pRawInputDeviceList;
		UINT i = 0;
		UINT j = 0;

		if (m_setting->m_KEY_first == 1)
		{
			m_setting->m_KEY_first = 2;

			// m_keyboard_tableの初期化
			UINT k = 0;
			while (k < 8)
			{ // キーボードは8個までサポート
				m_setting->m_keyboard_table[k].hDevice = NULL;
				m_setting->m_keyboard_table[k].UnitID = 0;
				m_setting->m_keyboard_table[k].dwVendorId = 0;
				m_setting->m_keyboard_table[k].dwProductId = 0;
				m_setting->m_keyboard_table[k].dwRevisionId = 0;
				m_setting->m_keyboard_table[k].dwExtraInfo = 0;
				k++;
			}

			// kbdaddid.sys の有効確認 (License == 1 のみ有効)
			DWORD kbdAddIdDeviceIdMode = 0; // 0=DeviceInstanceId (default), 1=HardwareId
			{
				HKEY hKey;
				DWORD kbdAddIdLicense = 0;
				if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					_T("SYSTEM\\CurrentControlSet\\Services\\kbdaddid\\Parameters"),
					0, KEY_READ, &hKey) == ERROR_SUCCESS)
				{
					DWORD cbData = sizeof(DWORD);
					RegQueryValueEx(hKey, _T("License"), NULL, NULL,
						(LPBYTE)&kbdAddIdLicense, &cbData);
					cbData = sizeof(DWORD);
					RegQueryValueEx(hKey, _T("DeviceIdMode"), NULL, NULL,
						(LPBYTE)&kbdAddIdDeviceIdMode, &cbData);
					RegCloseKey(hKey);
				}
				if (kbdAddIdLicense == 1)
				{
					m_setting->m_UseKbdAddId = 1;
					*m_log << _T("kbdaddid: active (License=1), ExtraInfo mode enabled.") << std::endl;
				}
			}

			if (myGetRawInputDeviceList == NULL)
			{
				goto ending_UnitID; // rawinputが使えない環境なので、スルー。
			}

			// kbdaddidが有効な場合はExtraInfoモードを優先し、hDevice照合は使わない
			if (m_setting->m_UseKbdAddId == 0)
				m_setting->m_UseUnitID = 1; // rawinputが使えるので 1にする。

			// まずデバイス数を取得する。
			if (myGetRawInputDeviceList(NULL, &nDevices, sizeof(RAWINPUTDEVICELIST)) != 0)
			{
				goto ending_UnitID;
			}

			// デバイスリストを取得する。
			pRawInputDeviceList = (RAWINPUTDEVICELIST *)malloc(sizeof(RAWINPUTDEVICELIST) * nDevices);
			myGetRawInputDeviceList(pRawInputDeviceList, &nDevices, sizeof(RAWINPUTDEVICELIST));

			UINT nSize = 0;
			DWORD dwVendorId = 0;
			DWORD dwProductId = 0;
			DWORD dwRevisionId = 0;

			while (i < nDevices && j < 8) // キーボードは8個までサポート
			{
				if (pRawInputDeviceList[i].dwType == RIM_TYPEKEYBOARD)
				{
					// 初期化
					dwVendorId = 0;
					dwProductId = 0;
					dwRevisionId = 0;

					// hDeviceの取得
					m_setting->m_keyboard_table[j].hDevice = pRawInputDeviceList[i].hDevice;

					// DeviceIDの取得
					if (myGetRawInputDeviceInfo(pRawInputDeviceList[i].hDevice, RIDI_DEVICENAME, NULL, &nSize) != 0)
					{
						continue;
					}
					WCHAR *wcDeviceName = new WCHAR[nSize + 1];

					if (myGetRawInputDeviceInfo(pRawInputDeviceList[i].hDevice, RIDI_DEVICENAME, wcDeviceName, &nSize) < 0)
					{
						delete[] wcDeviceName;
						continue;
					}

					// VendorIDなどに分解
					// /??/HID#VID_05A4&PID_9840#6&145a460c&0&0000#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}
					//??/ACPI#PNP0303#4&7989e7a&0#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}
					//tstringi tmpDeviceName = wcDeviceName;

					// RDP_KBDは対象外
					if (!wcsncmp(L"\\??\\Root#RDP_KBD", wcDeviceName, wcslen(L"\\??\\Root#RDP_KBD"))) // XP
					{
						i++;
						continue;
					}
					if (!wcsncmp(L"\\\\?\\Root#RDP_KBD", wcDeviceName, wcslen(L"\\\\?\\Root#RDP_KBD"))) // Vista以降
					{
						i++;
						continue;
					}

					// とりあえず検出したデバイス名を表示する。
					*m_log << _T("current device name:") << wcDeviceName << std::endl;
					// kbdaddid 有効時: kbdaddid と同じ計算式で ExtraInfo 値を表示する
					if (m_setting->m_UseKbdAddId == 1)
					{
						ULONG extraInfo = 0;
						if (kbdAddIdDeviceIdMode == 0)
						{
							// DeviceInstanceId モード: USB ポートが異なれば別値。同一製品でも区別できる。
							extraInfo = KbdAddId_ComputeExtraInfoByInstanceId(wcDeviceName);
						}
						else if (kbdAddIdDeviceIdMode == 1)
						{
							// HardwareId モード: 同一製品は USB ポートに依らず同じ値になる。
							extraInfo = KbdAddId_ComputeExtraInfoByHardwareId(wcDeviceName);
						}

						if (extraInfo != 0)
						{
							const TCHAR* modeName = (kbdAddIdDeviceIdMode == 0)
								? _T("DeviceInstanceId mode")
								: _T("HardwareId mode");
							*m_log << _T("  kbdaddid ExtraInfo 0x")
								<< std::hex << std::uppercase << extraInfo
								<< std::dec << _T(" (") << modeName << _T(")")
								<< std::endl;
						}
						else
						{
							// 0 = 計算失敗、またはフォールバックパスが発動している可能性。
							// kbdaddid のフォールバック (TryContainerId / TryHardwareIdWithUniq 等) は
							// カーネル内カウンタを使うため user-mode から再現不可。
							// nagi DriverManager で実際の ExtraInfo 値を確認してください。
							*m_log << _T("  kbdaddid ExtraInfo: cannot compute")
								<< _T(" (DeviceIdMode=") << kbdAddIdDeviceIdMode << _T(")")
								<< _T("; check nagi DriverManager for actual value")
								<< std::endl;
						}
					}
					WCHAR *tmpStopIndex = NULL;

					// ACPIで始まる場合、PS/2キーボード
					if (!wcsncmp(L"\\??\\ACPI#", wcDeviceName, wcslen(L"\\??\\ACPI#")) || !wcsncmp(L"\\\\?\\ACPI#", wcDeviceName, wcslen(L"\\\\?\\ACPI#")))
					{
						// PNP番号の取得
						WCHAR *pIndexPNP = wcsstr(wcDeviceName, L"PNP"); // PNPで始まるところを見つける。
						WCHAR tmpPNP_NUM[5] = {NULL, NULL, NULL, NULL, NULL};
						if (pIndexPNP != NULL)
						{
							// 後ろの4文字をコピーする。
							wcsncpy_s(tmpPNP_NUM, _countof(tmpPNP_NUM), pIndexPNP + 3, 4);
						}
						// 取得した4文字を数値に変換する。
						dwProductId = wcstol(tmpPNP_NUM, &tmpStopIndex, 16);
						goto set_vendor_productID;
					}

					// HIDで始まる場合、HIDキーボード
					if (!wcsncmp(L"\\??\\HID#", wcDeviceName, wcslen(L"\\??\\HID#")) || !wcsncmp(L"\\\\?\\HID#", wcDeviceName, wcslen(L"\\\\?\\HID#")))
					{
						if (wcsstr(wcDeviceName, L"{00001124-0000-1000-8000-00805f9b34fb}") || wcsstr(wcDeviceName, L"{00001124-0000-1000-8000-00805F9B34FB}"))
						{
							// Bluetooth keyboad
							// VenderIdの取得
							WCHAR *pIndexVID = wcsstr(wcDeviceName, L"_VID"); // _VIDで始まるところを見つける。
							WCHAR tmpVID_NUM[9] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
							if (pIndexVID != NULL)
							{
								// 後ろの8文字をコピーする。
								wcsncpy_s(tmpVID_NUM, _countof(tmpVID_NUM), pIndexVID + 5, 8);
							}
							else
							{
								WCHAR *pIndexMFG = wcsstr(wcDeviceName, L"_LOCALMFG"); // _LOCALMFGで始まるところを見つける。
								if (pIndexMFG != NULL)
								{
									// 後ろの4文字をコピーする。
									wcsncpy_s(tmpVID_NUM, _countof(tmpVID_NUM), pIndexMFG + 10, 4);
								}
							}
							// 取得した4or8文字を数値に変換する。
							dwVendorId = wcstoul(tmpVID_NUM, &tmpStopIndex, 16);

							// ProductIdの取得
							WCHAR *pIndexPID = wcsstr(wcDeviceName, L"_PID"); // _PIDで始まるところを見つける。
							WCHAR tmpPID_NUM[5] = {NULL, NULL, NULL, NULL, NULL};
							if (pIndexPID != NULL)
							{
								// 後ろの4文字をコピーする。
								wcsncpy_s(tmpPID_NUM, _countof(tmpPID_NUM), pIndexPID + 5, 4);
							}
							// 取得した4文字を数値に変換する。
							dwProductId = wcstol(tmpPID_NUM, &tmpStopIndex, 16);
							goto set_vendor_productID;
						}
						else
						{
							// USB keyboard
							// VenderIdの取得
							WCHAR *pIndexVid = wcsstr(wcDeviceName, L"Vid_"); // Vid_で始まるところを見つける。
							WCHAR tmpVID_NUM[5] = {NULL, NULL, NULL, NULL, NULL};
							if (pIndexVid != NULL)
							{
								// 後ろの4文字をコピーする。
								wcsncpy_s(tmpVID_NUM, _countof(tmpVID_NUM), pIndexVid + 4, 4);
							}
							else
							{
								WCHAR *pIndexVID = wcsstr(wcDeviceName, L"VID_"); // VID_で始まるところを見つける。
								if (pIndexVID != NULL)
								{
									// 後ろの4文字をコピーする。
									wcsncpy_s(tmpVID_NUM, _countof(tmpVID_NUM), pIndexVID + 4, 4);
								}
							}
							// 取得した4文字を数値に変換する。
							dwVendorId = wcstol(tmpVID_NUM, &tmpStopIndex, 16);

							// ProductIdの取得
							WCHAR *pIndexPid = wcsstr(wcDeviceName, L"Pid_"); // Pid_で始まるところを見つける。
							WCHAR tmpPID_NUM[5] = {NULL, NULL, NULL, NULL, NULL};
							if (pIndexPid != NULL)
							{
								// 後ろの4文字をコピーする。
								wcsncpy_s(tmpPID_NUM, _countof(tmpPID_NUM), pIndexPid + 4, 4);
							}
							else
							{
								WCHAR *pIndexPID = wcsstr(wcDeviceName, L"PID_"); // PID_で始まるところを見つける。
								if (pIndexPID != NULL)
								{
									// 後ろの4文字をコピーする。
									wcsncpy_s(tmpPID_NUM, _countof(tmpPID_NUM), pIndexPID + 4, 4);
								}
							}
							// 取得した4文字を数値に変換する。
							dwProductId = wcstol(tmpPID_NUM, &tmpStopIndex, 16);

							// RevisionIdの取得
							WCHAR *pIndexRID = wcsstr(wcDeviceName, L"MI_"); // MI_で始まるところを見つける。
							WCHAR tmpRID_NUM[3] = {NULL, NULL, NULL};
							if (pIndexRID != NULL)
							{
								// 後ろの2文字をコピーする。
								wcsncpy_s(tmpRID_NUM, _countof(tmpRID_NUM), pIndexRID + 3, 2);
							}
							// 取得したMI_2文字も数値にする。なお正確にはRevision番号ではない。
							dwRevisionId = wcstol(tmpRID_NUM, &tmpStopIndex, 16);
							goto set_vendor_productID;
						}
					}
				set_vendor_productID:

					m_setting->m_keyboard_table[j].dwVendorId = dwVendorId;
					m_setting->m_keyboard_table[j].dwProductId = dwProductId;
					m_setting->m_keyboard_table[j].dwRevisionId = dwRevisionId;
					if (m_setting->m_UseKbdAddId == 0)
						*m_log << _T("def option UnitID = Kx ") << dwVendorId << _T(" ") << dwProductId << _T(" ") << dwRevisionId << std::endl;

					// キーボードの個数
					j++;
					delete[] wcDeviceName;
				}
				i++;
			}
			free(pRawInputDeviceList);
		}
		if (m_setting->m_KEY_first == 2)
		{
			HWND i_hwnd = FindWindow(_T("nodokaTasktray"), NULL);

			if (i_hwnd != NULL) // のどかの通知領域アイコンをTargetにする。
			{
				Rid[0].usUsagePage = 0x01;
				Rid[0].usUsage = 0x06; // KEYBOARD
				Rid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
				Rid[0].hwndTarget = i_hwnd;

				Rid[1].usUsagePage = 0x01;
				Rid[1].usUsage = 0x80; // Generic desktop Keyboard
				Rid[1].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
				Rid[1].hwndTarget = i_hwnd;

				Rid[2].usUsagePage = 0x000C;
				Rid[2].usUsage = 0x01; // Consumer controls
				Rid[2].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
				Rid[2].hwndTarget = i_hwnd;

				Rid[3].usUsagePage = 0xFFBC;
				Rid[3].usUsage = 0x88; // Vendor-defined (MCE)
				Rid[3].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
				Rid[3].hwndTarget = i_hwnd;

				// kbdaddidモードはhDevice列挙をしないためj==0になるが登録は必要
				if ((j > 0 || m_setting->m_UseKbdAddId == 1) && myRegisterRawInputDevices != NULL)
				{
					m_setting->m_KEY_first = 0;
					myRegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));
				}
			}
		}
	ending_UnitID:

		// まず最初の引数キー番号を取得する。

		// 読み換え先のキーID 1～7あるいは、K1～K7が使える。keyNumに格納する。
		Token *t1 = getToken();
		USHORT keyNum = 0;
		if (t1->isNumber())
		{
			keyNum = (u_char)t1->getNumber();
			if (keyNum < 1 || keyNum > 7)
				throw ErrorMessage() << _T("`") << *t1 << _T("': invalid KeyNumber. Use 1 to 7.");
		}
		if (t1->isString())
		{
			if (*t1 == _T("K1"))
				keyNum = 1;
			else if (*t1 == _T("K2"))
				keyNum = 2;
			else if (*t1 == _T("K3"))
				keyNum = 3;
			else if (*t1 == _T("K4"))
				keyNum = 4;
			else if (*t1 == _T("K5"))
				keyNum = 5;
			else if (*t1 == _T("K6"))
				keyNum = 6;
			else if (*t1 == _T("K7"))
				keyNum = 7;
			else
				throw ErrorMessage() << _T("`") << *t1 << _T("': invalid KeyNumber. Use K1 to K7.");
		}

		// キーボードを特定するデバイスIDの指定を現在のキーボード一覧と比較する。

		Token *t2 = getToken();

		if (*t2 == _T("ExtraInfo"))
		{
			// kbdaddidモード: def option UnitID = Kx ExtraInfo 0xXXXX0000 (上位16bitがキーボード識別子)
			Token *t3 = getToken();
			// getNumber()はint(符号付き)を返すため0xF5210000等が0x7FFFFFFFにクランプされる。
			// tstringstream経由でm_stringValue(元テキスト)を_tcstoullで再パースする。
			DWORD dwExtraInfo = 0;
			if (t3->isNumber())
			{
				tstringstream ss;
				ss << *t3;
				dwExtraInfo = (DWORD)_tcstoul(ss.str().c_str(), NULL, 0);
			}

			// .nodoka記述のdump
			*m_log << _T("current K") << keyNum
				   << _T(" ExtraInfo 0x") << std::hex << std::uppercase
				   << dwExtraInfo << std::dec << std::endl;

			if (m_setting->m_UseKbdAddId == 0)
			{
				*m_log << _T("def option UnitID = K") << keyNum
					   << _T(" ExtraInfo 0x") << std::hex << std::uppercase
					   << dwExtraInfo << std::dec
					   << _T(": kbdaddid not active, skipped.") << std::endl;
			}
			else if ((dwExtraInfo >> 16) == 0)
			{
				*m_log << _T("def option UnitID = K") << keyNum
					   << _T(" ExtraInfo 0x") << std::hex << std::uppercase
					   << dwExtraInfo << std::dec
					   << _T(": invalid ExtraInfo value (upper 16 bits must be non-zero).") << std::endl;
			}
			else
			{
				// 空きスロットに登録
				bool bRegistered = false;
				for (UINT uSlot = 0; uSlot < 8; uSlot++)
				{
					if (m_setting->m_keyboard_table[uSlot].dwExtraInfo == 0)
					{
						m_setting->m_keyboard_table[uSlot].dwExtraInfo = dwExtraInfo;
						m_setting->m_keyboard_table[uSlot].UnitID = keyNum;
						*m_log << _T("def option UnitID = K") << keyNum
							   << _T(" ExtraInfo 0x") << std::hex << std::uppercase
							   << dwExtraInfo << std::dec << std::endl;
						bRegistered = true;
						break;
					}
				}
				if (!bRegistered)
				{
					*m_log << _T("def option UnitID = K") << keyNum
						   << _T(" ExtraInfo 0x") << std::hex << std::uppercase
						   << dwExtraInfo << std::dec
						   << _T(": no empty slot in keyboard_table.") << std::endl;
				}
			}
		}
		else
		{
			// 既存: def option UnitID = Kx VID PID REV
			Token *t3 = getToken();
			Token *t4 = getToken();

			UINT uCount = 0;
			UINT uFlag = FALSE;
			DWORD dwArg2;
			DWORD dwArg3;
			DWORD dwArg4;

			if (t2->isNumber() && t3->isNumber() && t4->isNumber())
			{
				uCount = 0;
				uFlag = FALSE;

				dwArg2 = t2->getNumber();
				dwArg3 = t3->getNumber();
				dwArg4 = t4->getNumber();

				// .nodoka記述のdump
				*m_log << _T("current K") << keyNum << _T(" ") << dwArg2 << _T(" ") << dwArg3 << _T(" ") << dwArg4 << std::endl;

				while (uCount < 8)
				{
					// t2, t3, t4が一致するキーボードがある場合、Kxを設定する。
					if (m_setting->m_keyboard_table[uCount].dwVendorId == dwArg2 &&
						m_setting->m_keyboard_table[uCount].dwProductId == dwArg3 &&
						m_setting->m_keyboard_table[uCount].dwRevisionId == dwArg4)
					{
						m_setting->m_keyboard_table[uCount].UnitID = keyNum;
						*m_log << _T("def option UnitID = K") << keyNum << _T(" ")
							   << dwArg2 << _T(" ") << dwArg3 << _T(" ") << dwArg4 << std::endl;
						uFlag = TRUE;
						break;
					}
					uCount++;
				}
				if (uFlag == FALSE)
				{
					*m_log << _T("def option UnitID = K0")
						   << _T(" ")
						   << dwArg2 << _T(" ") << dwArg3 << _T(" ") << dwArg4 << std::endl;
				}
			}
			else
			{
				*m_log << _T("def option UnitID = K") << keyNum << _T(" ") << *t2 << _T(" ") << *t3 << _T(" ") << *t4 << _T(": not number") << std::endl;
			}
		}
	}
	else if (*t == _T("ComboWindow"))
	{
		if (*getToken() != _T("="))
			throw ErrorMessage() << _T("there must be `=' after `def option ComboWindow'.");
		load_ARGUMENT(&m_setting->m_ComboWindow);
	}
	else if (*t == _T("TapHoldThreshold"))
	{
		if (*getToken() != _T("="))
			throw ErrorMessage() << _T("there must be `=' after `def option TapHoldThreshold'.");
		load_ARGUMENT(&m_setting->m_TapHoldThreshold);
	}
	else if (*t == _T("TapHoldInterrupt"))
	{
		if (*getToken() != _T("="))
			throw ErrorMessage() << _T("there must be `=' after `def option TapHoldInterrupt'.");
		Token *val = getToken();
		if (*val == _T("tap"))
			m_setting->m_TapHoldInterruptIsTap = true;
		else if (*val == _T("hold"))
			m_setting->m_TapHoldInterruptIsTap = false;
		else
			throw ErrorMessage() << _T("'tap' or 'hold' expected after `def option TapHoldInterrupt ='.");
	}
	else if (*t == _T("TapHoldPermissiveHold"))
	{
		if (*getToken() != _T("="))
			throw ErrorMessage() << _T("there must be `=' after `def option TapHoldPermissiveHold'.");
		Token *val = getToken();
		if (*val == _T("on"))        m_setting->m_TapHoldPermissiveHold = true;
		else if (*val == _T("off"))  m_setting->m_TapHoldPermissiveHold = false;
		else throw ErrorMessage() << _T("'on' or 'off' expected after `def option TapHoldPermissiveHold ='.");
	}
	else if (*t == _T("TapHoldOnOtherKeyPress"))
	{
		if (*getToken() != _T("="))
			throw ErrorMessage() << _T("there must be `=' after `def option TapHoldOnOtherKeyPress'.");
		Token *val = getToken();
		if (*val == _T("on"))        m_setting->m_TapHoldOnOtherKeyPress = true;
		else if (*val == _T("off"))  m_setting->m_TapHoldOnOtherKeyPress = false;
		else throw ErrorMessage() << _T("'on' or 'off' expected after `def option TapHoldOnOtherKeyPress ='.");
	}
	else if (*t == _T("TapHoldQuickTapTerm"))
	{
		if (*getToken() != _T("="))
			throw ErrorMessage() << _T("there must be `=' after `def option TapHoldQuickTapTerm'.");
		load_ARGUMENT(&m_setting->m_TapHoldQuickTapTerm);
	}
	else if (*t == _T("TapDanceTimeout"))
	{
		if (*getToken() != _T("="))
			throw ErrorMessage() << _T("there must be `=' after `def option TapDanceTimeout'.");
		load_ARGUMENT(&m_setting->m_TapDanceTimeout);
	}
	else if (*t == _T("ComboDetector"))
	{
		if (*getToken() != _T("="))
			throw ErrorMessage() << _T("there must be `=' after `def option ComboDetector'.");
		Token *val = getToken();
		if      (*val == _T("timeout"))      m_setting->m_ComboDetectorMode = Setting::CD_TIMEOUT;
		else if (*val == _T("immediate"))    m_setting->m_ComboDetectorMode = Setting::CD_IMMEDIATE;
		else if (*val == _T("rollover"))     m_setting->m_ComboDetectorMode = Setting::CD_ROLLOVER;
		else if (*val == _T("strict-order")) m_setting->m_ComboDetectorMode = Setting::CD_STRICT_ORDER;
		else if (*val == _T("zero-latency")) m_setting->m_ComboDetectorMode = Setting::CD_ZERO_LATENCY;
		else
			throw ErrorMessage() << _T("ComboDetector: unknown mode '") << *val << _T("'.");
	}
	else if (*t == _T("ComboIdleThreshold"))
	{
		if (*getToken() != _T("="))
			throw ErrorMessage() << _T("there must be `=' after `def option ComboIdleThreshold'.");
		load_ARGUMENT(&m_setting->m_ComboIdleThreshold);
	}
	else if (*t == _T("ComboOverlapRatio"))
	{
		if (*getToken() != _T("="))
			throw ErrorMessage() << _T("there must be `=' after `def option ComboOverlapRatio'.");
		load_ARGUMENT(&m_setting->m_ComboOverlapRatio);
		if (m_setting->m_ComboOverlapRatio < 0 || m_setting->m_ComboOverlapRatio > 100)
			throw ErrorMessage() << _T("ComboOverlapRatio must be 0-100.");
	}
	else if (*t == _T("ComboNestedAlwaysMatch"))
	{
		if (*getToken() != _T("="))
			throw ErrorMessage() << _T("there must be `=' after `def option ComboNestedAlwaysMatch'.");
		Token *val = getToken();
		if      (*val == _T("on"))  m_setting->m_ComboNestedAlwaysMatch = true;
		else if (*val == _T("off")) m_setting->m_ComboNestedAlwaysMatch = false;
		else throw ErrorMessage() << _T("ComboNestedAlwaysMatch: expected on/off.");
	}
	else
	{
		throw ErrorMessage() << _T("syntax error `def option ") << *t << _T("'.");
	}
}

// <KEYBOARD_DEFINITION>
void SettingLoader::load_KEYBOARD_DEFINITION()
{
	Token *t = getToken();

	// <DEFINE_KEY>
	if (*t == _T("key"))
		load_DEFINE_KEY();

	// <DEFINE_MODIFIER>
	else if (*t == _T("mod"))
		load_DEFINE_MODIFIER();

	// <DEFINE_SYNC_KEY>
	else if (*t == _T("sync"))
		load_DEFINE_SYNC_KEY();

	// <DEFINE_ALIAS>
	else if (*t == _T("alias"))
		load_DEFINE_ALIAS();

	// <DEFINE_SUBSTITUTE>
	else if (*t == _T("subst"))
		load_DEFINE_SUBSTITUTE();

	// <DEFINE_OPTION>
	else if (*t == _T("option"))
		load_DEFINE_OPTION();

	//
	else
		throw ErrorMessage() << _T("syntax error `") << *t << _T("'.");
}

// <..._MODIFIER>
Modifier SettingLoader::load_MODIFIER(
	Modifier::Type i_mode, Modifier i_modifier, Modifier::Type *o_mode)
{
	if (o_mode)
		*o_mode = Modifier::Type_begin;

	Modifier isModifierSpecified;
	enum
	{
		PRESS,
		RELEASE,
		DONTCARE
	} flag = PRESS;

	int i;
	for (i = i_mode; i < Modifier::Type_ASSIGN; ++i)
	{
		i_modifier.dontcare(Modifier::Type(i));
		isModifierSpecified.on(Modifier::Type(i));
	}

	Token *t = NULL;

continue_loop:
	while (!isEOL())
	{
		t = lookToken();

		const static struct
		{
			const _TCHAR *m_s;
			Modifier::Type m_mt;
		} map[] =
			{
				// <BASIC_MODIFIER>
				{_T("S-"), Modifier::Type_Shift},
				{_T("C-"), Modifier::Type_Control},
				{_T("A-"), Modifier::Type_Alt},
				{_T("M-"), Modifier::Type_Alt},
				{_T("W-"), Modifier::Type_Windows},
				// <KEYSEQ_MODIFIER>
				{_T("U-"), Modifier::Type_Up},
				{_T("D-"), Modifier::Type_Down},
				// <ASSIGN_MODIFIER>
				{_T("R-"), Modifier::Type_Repeat},
				{_T("IL-"), Modifier::Type_ImeLock},
				{_T("IC-"), Modifier::Type_ImeComp},
				{_T("I-"), Modifier::Type_ImeComp},
				{_T("NL-"), Modifier::Type_NumLock},
				{_T("CL-"), Modifier::Type_CapsLock},
				{_T("SL-"), Modifier::Type_ScrollLock},
				{_T("KL-"), Modifier::Type_KanaLock},
				{_T("MAX-"), Modifier::Type_Maximized},
				{_T("MIN-"), Modifier::Type_Minimized},
				{_T("MMAX-"), Modifier::Type_MdiMaximized},
				{_T("MMIN-"), Modifier::Type_MdiMinimized},
				{_T("T-"), Modifier::Type_Touchpad},
				{_T("TS-"), Modifier::Type_TouchpadSticky},
				{_T("TL-"), Modifier::Type_TouchpadL},
				{_T("TLS-"), Modifier::Type_TouchpadLSticky},
				{_T("TR-"), Modifier::Type_TouchpadR},
				{_T("TRS-"), Modifier::Type_TouchpadRSticky},
				{_T("M0-"), Modifier::Type_Mod0},
				{_T("M1-"), Modifier::Type_Mod1},
				{_T("M2-"), Modifier::Type_Mod2},
				{_T("M3-"), Modifier::Type_Mod3},
				{_T("M4-"), Modifier::Type_Mod4},
				{_T("M5-"), Modifier::Type_Mod5},
				{_T("M6-"), Modifier::Type_Mod6},
				{_T("M7-"), Modifier::Type_Mod7},
				{_T("M8-"), Modifier::Type_Mod8},
				{_T("M9-"), Modifier::Type_Mod9},
				{_T("L0-"), Modifier::Type_Lock0},
				{_T("L1-"), Modifier::Type_Lock1},
				{_T("L2-"), Modifier::Type_Lock2},
				{_T("L3-"), Modifier::Type_Lock3},
				{_T("L4-"), Modifier::Type_Lock4},
				{_T("L5-"), Modifier::Type_Lock5},
				{_T("L6-"), Modifier::Type_Lock6},
				{_T("L7-"), Modifier::Type_Lock7},
				{_T("L8-"), Modifier::Type_Lock8},
				{_T("L9-"), Modifier::Type_Lock9},
				{_T("LA-"), Modifier::Type_LockA},
				{_T("LB-"), Modifier::Type_LockB},
				{_T("LC-"), Modifier::Type_LockC},
				{_T("LD-"), Modifier::Type_LockD},
				{_T("LE-"), Modifier::Type_LockE},
				{_T("LF-"), Modifier::Type_LockF},
				{_T("K0-"), Modifier::Type_Keyboard0},
				{_T("K1-"), Modifier::Type_Keyboard1},
				{_T("K2-"), Modifier::Type_Keyboard2},
				{_T("K3-"), Modifier::Type_Keyboard3},
				{_T("K4-"), Modifier::Type_Keyboard4},
				{_T("K5-"), Modifier::Type_Keyboard5},
				{_T("K6-"), Modifier::Type_Keyboard6},
				{_T("K7-"), Modifier::Type_Keyboard7},
				{_T("IW-"), Modifier::Type_ImeCandi},
				{_T("IH-"), Modifier::Type_Harf},
				{_T("IK-"), Modifier::Type_Katakana},
				{_T("IJ-"), Modifier::Type_Native},
				{_T("DP-"), Modifier::Type_DP},
			};

		for (int i = 0; i < NUMBER_OF(map); ++i)
			if (*t == map[i].m_s)
			{
				getToken();
				Modifier::Type mt = map[i].m_mt;
				if (static_cast<int>(i_mode) <= static_cast<int>(mt))
					throw ErrorMessage() << _T("`") << *t
										 << _T("': invalid modifier at this context.");
				switch (flag)
				{
				case PRESS:
					i_modifier.press(mt);
					break;
				case RELEASE:
					i_modifier.release(mt);
					break;
				case DONTCARE:
					i_modifier.dontcare(mt);
					break;
				}
				isModifierSpecified.on(mt);
				flag = PRESS;

				if (o_mode && *o_mode < mt)
				{
					if (mt < Modifier::Type_BASIC)
						*o_mode = Modifier::Type_BASIC;
					else if (mt < Modifier::Type_KEYSEQ)
						*o_mode = Modifier::Type_KEYSEQ;
					else if (mt < Modifier::Type_ASSIGN)
						*o_mode = Modifier::Type_ASSIGN;
				}
				goto continue_loop;
			}

		if (*t == _T("*"))
		{
			getToken();
			flag = DONTCARE;
			continue;
		}

		if (*t == _T("~"))
		{
			getToken();
			flag = RELEASE;
			continue;
		}

		break;
	}

	for (i = Modifier::Type_begin; i != Modifier::Type_end; ++i)
		if (!isModifierSpecified.isOn(Modifier::Type(i)))
			switch (flag)
			{
			case PRESS:
				break;
			case RELEASE:
				i_modifier.release(Modifier::Type(i));
				break;
			case DONTCARE:
				i_modifier.dontcare(Modifier::Type(i));
				break;
			}

	// fix up and down
	bool isDontcareUp = i_modifier.isDontcare(Modifier::Type_Up);
	bool isDontcareDown = i_modifier.isDontcare(Modifier::Type_Down);
	bool isOnUp = i_modifier.isOn(Modifier::Type_Up);
	bool isOnDown = i_modifier.isOn(Modifier::Type_Down);
	if (isDontcareUp && isDontcareDown)
		;
	else if (isDontcareUp)
		i_modifier.on(Modifier::Type_Up, !isOnDown);
	else if (isDontcareDown)
		i_modifier.on(Modifier::Type_Down, !isOnUp);
	else if (isOnUp == isOnDown)
	{
		i_modifier.dontcare(Modifier::Type_Up);
		i_modifier.dontcare(Modifier::Type_Down);
	}

	// fix repeat
	if (!isModifierSpecified.isOn(Modifier::Type_Repeat))
		i_modifier.dontcare(Modifier::Type_Repeat);
	return i_modifier;
}

// <KEY_NAME>
Key *SettingLoader::load_KEY_NAME()
{
	Token *t = getToken();
	Key *key = m_setting->m_keyboard.searchKey(t->getString());
	if (!key)
		throw ErrorMessage() << _T("`") << *t << _T("': invalid key name @ load_key_name");
	return key;
}

// <KEYMAP_DEFINITION>
void SettingLoader::load_KEYMAP_DEFINITION(const Token *i_which)
{
	Keymap::Type type = Keymap::Type_keymap;
	Token *name = getToken(); // <KEYMAP_NAME>
	tstringi windowClassName;
	tstringi windowTitleName;
	KeySeq *keySeq = NULL;
	Keymap *parentKeymap = NULL;
	bool isKeymap2 = false;
	bool doesLoadDefaultKeySeq = false;

	if (!isEOL())
	{
		Token *t = lookToken();
		if (*i_which == _T("window")) // <WINDOW>
		{
			if (t->isOpenParen())
			// "(" <WINDOW_CLASS_NAME> "&&" <WINDOW_TITLE_NAME> ")"
			// "(" <WINDOW_CLASS_NAME> "||" <WINDOW_TITLE_NAME> ")"
			{
				getToken();
				windowClassName = getToken()->getRegexp();
				t = getToken();
				if (*t == _T("&&"))
					type = Keymap::Type_windowAnd;
				else if (*t == _T("||"))
					type = Keymap::Type_windowOr;
				else
					throw ErrorMessage() << _T("`") << *t << _T("': unknown operator.");
				windowTitleName = getToken()->getRegexp();
				if (!getToken()->isCloseParen())
					throw ErrorMessage() << _T("there must be `)'.");
			}
			else if (t->isRegexp()) // <WINDOW_CLASS_NAME>
			{
				getToken();
				type = Keymap::Type_windowAnd;
				windowClassName = t->getRegexp();
			}
		}
		else if (*i_which == _T("keymap"))
			;
		else if (*i_which == _T("keymap2"))
			isKeymap2 = true;
		else
			ASSERT(false);

		if (!isEOL())
			doesLoadDefaultKeySeq = true;
	}

	m_currentKeymap = m_setting->m_keymaps.add(
		Keymap(type, name->getString(), windowClassName, windowTitleName,
			   NULL, NULL));

	if (doesLoadDefaultKeySeq)
	{
		Token *t = lookToken();
		// <KEYMAP_PARENT>
		if (*t == _T(":"))
		{
			getToken();
			t = getToken();
			parentKeymap = m_setting->m_keymaps.searchByName(t->getString());
			if (!parentKeymap)
				throw ErrorMessage() << _T("`") << *t
									 << _T("': unknown keymap name.");
		}
		if (!isEOL())
		{
			t = getToken();
			if (!(*t == _T("=>") || *t == _T("=")))
				throw ErrorMessage() << _T("`") << *t << _T("': syntax error.");
			keySeq = SettingLoader::load_KEY_SEQUENCE();
		}
	}
	if (keySeq == NULL)
	{
		FunctionData *fd;
		if (type == Keymap::Type_keymap && !isKeymap2)
			fd = createFunctionData(_T("KeymapParent"));
		else if (type == Keymap::Type_keymap && !isKeymap2)
			fd = createFunctionData(_T("Undefined"));
		else // (type == Keymap::Type_windowAnd || type == Keymap::Type_windowOr)
			fd = createFunctionData(_T("KeymapParent"));
		ASSERT(fd);
		keySeq = m_setting->m_keySeqs.add(
			KeySeq(name->getString()).add(ActionFunction(fd)));
	}

	m_currentKeymap->setIfNotYet(keySeq, parentKeymap);
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(bool *o_arg)
{
	Token *t = getToken();
	*o_arg = !((*t == _T("false")) || (*t == _T("disable")));
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(int *o_arg)
{
	*o_arg = getToken()->getNumber();
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(unsigned int *o_arg)
{
	*o_arg = getToken()->getNumber();
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(unsigned __int64 *o_arg)
{
	*o_arg = getToken()->getNumber();
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(__int64 *o_arg)
{
	*o_arg = getToken()->getNumber();
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(long *o_arg)
{
	*o_arg = getToken()->getNumber();
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(DWORD *o_arg)
{
	*o_arg = getToken()->getNumber();
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(tstringq *o_arg)
{
	*o_arg = getToken()->getString();
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(tstring *o_arg)
{
	*o_arg = getToken()->getString();
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(std::list<tstringq> *o_arg)
{
	while (true)
	{
		if (!lookToken()->isString())
			return;
		o_arg->push_back(getToken()->getString());

		if (!lookToken()->isComma())
			return;
		getToken();
	}
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(tregex *o_arg)
{
	*o_arg = getToken()->getRegexp();
}

// &lt;ARGUMENT_VK&gt;
void SettingLoader::load_ARGUMENT(VKey *o_arg)
{
	Token *t = getToken();
	int vkey = 0;
	while (true)
	{
		if (t->isNumber())
		{
			vkey |= static_cast<BYTE>(t->getNumber());
			break;
		}
		else if (*t == _T("E-"))
			vkey |= VKey_extended;
		else if (*t == _T("U-"))
			vkey |= VKey_released;
		else if (*t == _T("D-"))
			vkey |= VKey_pressed;
		else
		{
			const VKeyTable *vkt;
			for (vkt = g_vkeyTable; vkt->m_name; ++vkt)
				if (*t == vkt->m_name)
					break;
			if (!vkt->m_name)
				throw ErrorMessage() << _T("`") << *t
									 << _T("': unknown virtual key name.");
			vkey |= vkt->m_code;
			break;
		}
		t = getToken();
	}
	if (!(vkey & VKey_released) && !(vkey & VKey_pressed))
		vkey |= VKey_released | VKey_pressed;
	*o_arg = static_cast<VKey>(vkey);
}

// &lt;ARGUMENT_WINDOW&gt;
void SettingLoader::load_ARGUMENT(ToWindowType *o_arg)
{
	Token *t = getToken();
	if (t->isNumber())
	{
		if (ToWindowType_toBegin <= t->getNumber())
		{
			*o_arg = static_cast<ToWindowType>(t->getNumber());
			return;
		}
	}
	else if (getTypeValue(o_arg, t->getString()))
		return;
	throw ErrorMessage() << _T("`") << *t << _T("': invalid target window.");
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(GravityType *o_arg)
{
	Token *t = getToken();
	if (getTypeValue(o_arg, t->getString()))
		return;
	throw ErrorMessage() << _T("`") << *t << _T("': unknown gravity symbol.");
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(MouseHookType *o_arg)
{
	Token *t = getToken();
	if (getTypeValue(o_arg, t->getString()))
		return;
	throw ErrorMessage() << _T("`") << *t << _T("': unknown MouseHookType symbol.");
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(NodokaDialogType *o_arg)
{
	Token *t = getToken();
	if (getTypeValue(o_arg, t->getString()))
		return;
	throw ErrorMessage() << _T("`") << *t << _T("': unknown dialog box.");
}

// &lt;ARGUMENT_LOCK&gt;
void SettingLoader::load_ARGUMENT(ModifierLockType *o_arg)
{
	Token *t = getToken();
	if (getTypeValue(o_arg, t->getString()))
		return;
	throw ErrorMessage() << _T("`") << *t << _T("': unknown lock name.");
}

// &lt;ARGUMENT_LOCK&gt;
void SettingLoader::load_ARGUMENT(ToggleType *o_arg)
{
	Token *t = getToken();
	if (getTypeValue(o_arg, t->getString()))
		return;
	throw ErrorMessage() << _T("`") << *t << _T("': unknown toggle name.");
}

// &lt;ARGUMENT_SHOW_WINDOW&gt;
void SettingLoader::load_ARGUMENT(ShowCommandType *o_arg)
{
	Token *t = getToken();
	if (getTypeValue(o_arg, t->getString()))
		return;
	throw ErrorMessage() << _T("`") << *t << _T("': unknown show command.");
}

// &lt;ARGUMENT_TARGET_WINDOW&gt;
void SettingLoader::load_ARGUMENT(TargetWindowType *o_arg)
{
	Token *t = getToken();
	if (getTypeValue(o_arg, t->getString()))
		return;
	throw ErrorMessage() << _T("`") << *t
						 << _T("': unknown target window type.");
}

// &lt;bool&gt;
void SettingLoader::load_ARGUMENT(BooleanType *o_arg)
{
	Token *t = getToken();
	if (getTypeValue(o_arg, t->getString()))
		return;
	throw ErrorMessage() << _T("`") << *t << _T("': must be true or false.");
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(LogicalOperatorType *o_arg)
{
	Token *t = getToken();
	if (getTypeValue(o_arg, t->getString()))
		return;
	throw ErrorMessage() << _T("`") << *t << _T("': must be 'or' or 'and'.");
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(Modifier *o_arg)
{
	Modifier modifier;
	for (int i = Modifier::Type_begin; i != Modifier::Type_end; ++i)
		modifier.dontcare(static_cast<Modifier::Type>(i));
	*o_arg = load_MODIFIER(Modifier::Type_ASSIGN, modifier);
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(const Keymap **o_arg)
{
	Token *t = getToken();
	const Keymap *&keymap = *o_arg;
	keymap = m_setting->m_keymaps.searchByName(t->getString());
	if (!keymap)
		throw ErrorMessage() << _T("`") << *t << _T("': unknown keymap name.");
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(const KeySeq **o_arg)
{
	Token *t = getToken();
	const KeySeq *&keySeq = *o_arg;
	if (t->isOpenParen())
	{
		keySeq = load_KEY_SEQUENCE(_T(""), true);
		getToken(); // close paren
	}
	else if (*t == _T("$"))
	{
		t = getToken();
		keySeq = m_setting->m_keySeqs.searchByName(t->getString());
		if (!keySeq)
			throw ErrorMessage() << _T("`$") << *t << _T("': unknown keyseq name.");
	}
	else
		throw ErrorMessage() << _T("`") << *t << _T("': it is not keyseq.");
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(StrExprArg *o_arg)
{
	Token *t = getToken();
	StrExprArg::Type type = StrExprArg::Literal;
	if (*t == _T("$") && t->isQuoted() == false && lookToken()->getType() == Token::Type_string)
	{
		type = StrExprArg::Builtin;
		t = getToken();
	}
	*o_arg = StrExprArg(t->getString(), type);
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(WindowMonitorFromType *o_arg)
{
	Token *t = getToken();
	if (getTypeValue(o_arg, t->getString()))
		return;
	throw ErrorMessage() << _T("`") << *t
						 << _T("': unknown monitor from type.");
}

// &lt;ARGUMENT&gt;
void SettingLoader::load_ARGUMENT(Setting::ComboDetectorMode *o_arg)
{
	Token *t = getToken();
	if      (*t == _T("timeout"))      *o_arg = Setting::CD_TIMEOUT;
	else if (*t == _T("immediate"))    *o_arg = Setting::CD_IMMEDIATE;
	else if (*t == _T("rollover"))     *o_arg = Setting::CD_ROLLOVER;
	else if (*t == _T("strict-order")) *o_arg = Setting::CD_STRICT_ORDER;
	else if (*t == _T("zero-latency")) *o_arg = Setting::CD_ZERO_LATENCY;
	else throw ErrorMessage() << _T("'") << *t << _T("': unknown ComboDetector mode.");
}

// <KEY_SEQUENCE>
KeySeq *SettingLoader::load_KEY_SEQUENCE(
	const tstringi &i_name, bool i_isInParen, Modifier::Type i_mode)
{
	KeySeq keySeq(i_name);
	while (!isEOL())
	{
		Modifier::Type mode;
		Modifier modifier = load_MODIFIER(i_mode, m_defaultKeySeqModifier, &mode);
		keySeq.setMode(mode);
		Token *t = lookToken();
		if (t->isCloseParen() && i_isInParen)
			break;
		else if (t->isOpenParen())
		{
			getToken(); // open paren
			KeySeq *ks = load_KEY_SEQUENCE(_T(""), true, i_mode);
			getToken(); // close paren
			keySeq.add(ActionKeySeq(ks));
		}
		else if (*t == _T("$")) // <KEYSEQ_NAME>
		{
			getToken();
			t = getToken();
			KeySeq *ks = m_setting->m_keySeqs.searchByName(t->getString());
			if (ks == NULL)
				throw ErrorMessage() << _T("`$") << *t
									 << _T("': unknown keyseq name.");
			if (!ks->isCorrectMode(i_mode))
				throw ErrorMessage()
					<< _T("`$") << *t
					<< _T("': Some of R-, IL-, IC-, NL-, CL-, SL-, KL-, IH-, IK-, IJ-, IW-, MAX-, MIN-, MMAX-, MMIN-, T-, TS-, TL-, TLS-, TR-, TRS-, M0...M9- and L0...LF-, K0...K7-, DP- are used in the keyseq.  They are prohibited in this context.");
			keySeq.setMode(ks->getMode());
			keySeq.add(ActionKeySeq(ks));
		}
		else if (*t == _T("&")) // <FUNCTION_NAME>
		{
			getToken();
			t = getToken();

			// search function
			ActionFunction af(createFunctionData(t->getString()), modifier);
			if (af.m_functionData == NULL)
				throw ErrorMessage() << _T("`&") << *t
									 << _T("': unknown function name.");
			af.m_functionData->load(this);
			keySeq.add(af);
		}
		else // <KEYSEQ_MODIFIED_KEY_NAME>
		{
			ModifiedKey mkey;
			mkey.m_modifier = modifier;
			mkey.m_key = load_KEY_NAME();
			keySeq.add(ActionKey(mkey));
		}
	}
	return m_setting->m_keySeqs.add(keySeq);
}

// <KEY_ASSIGN>
void SettingLoader::load_KEY_ASSIGN()
{
	typedef std::list<ModifiedKey> AssignedKeys;
	AssignedKeys assignedKeys;

	ModifiedKey mkey;
	mkey.m_modifier =
		load_MODIFIER(Modifier::Type_ASSIGN, m_defaultAssignModifier);
	if (*lookToken() == _T("="))
	{
		getToken();
		m_defaultKeySeqModifier = load_MODIFIER(Modifier::Type_KEYSEQ,
												m_defaultKeySeqModifier);
		m_defaultAssignModifier = mkey.m_modifier;
		return;
	}

	while (true)
	{
		mkey.m_key = load_KEY_NAME();
		assignedKeys.push_back(mkey);
		if (*lookToken() == _T("=>") || *lookToken() == _T("="))
			break;
		mkey.m_modifier =
			load_MODIFIER(Modifier::Type_ASSIGN, m_defaultAssignModifier);
	}
	getToken();

	ASSERT(m_currentKeymap);
	KeySeq *keySeq = load_KEY_SEQUENCE();

#ifdef FOR_LIMIT
	m_currentKeymap->addAssignment(*(assignedKeys.begin()), keySeq); // LIMITモードでは、1個しか定義できない。
#else
	for (AssignedKeys::iterator i = assignedKeys.begin();
		 i != assignedKeys.end(); ++i)
		m_currentKeymap->addAssignment(*i, keySeq);
#endif
}

// <EVENT_ASSIGN>
void SettingLoader::load_EVENT_ASSIGN()
{
	std::list<ModifiedKey> assignedKeys;

	ModifiedKey mkey;
	mkey.m_modifier.dontcare(); //set all modifiers to dontcare

	Token *t = getToken();
	Key **e;
	for (e = Event::events; *e; ++e)
		if (*t == (*e)->getName())
		{
			mkey.m_key = *e;
			break;
		}
	if (!*e)
		throw ErrorMessage() << _T("`") << *t << _T("': invalid event name.");

	t = getToken();
	if (!(*t == _T("=>") || *t == _T("=")))
		throw ErrorMessage() << _T("`=' is expected.");

	ASSERT(m_currentKeymap);
	KeySeq *keySeq = load_KEY_SEQUENCE();
	m_currentKeymap->addAssignment(mkey, keySeq);
}

// <MODIFIER_ASSIGNMENT>
void SettingLoader::load_MODIFIER_ASSIGNMENT()
{
	// <MODIFIER_NAME>
	Token *t = getToken();
	Modifier::Type mt;

	while (true)
	{
		Keymap::AssignMode am = Keymap::AM_notModifier;
		if (*t == _T("!"))
			am = Keymap::AM_true, t = getToken();
		else if (*t == _T("!!"))
			am = Keymap::AM_oneShot, t = getToken();
		else if (*t == _T("!!!"))
			am = Keymap::AM_oneShotRepeatable, t = getToken();
		else if (*t == _T("!!!!"))
			am = Keymap::AM_oneShot2, t = getToken();

		if (*t == _T("shift") ||
			*t == _T("S"))
			mt = Modifier::Type_Shift;
		else if (*t == _T("alt") ||
				 *t == _T("meta") ||
				 *t == _T("menu") ||
				 *t == _T("A") ||
				 *t == _T("M"))
			mt = Modifier::Type_Alt;
		else if (*t == _T("control") ||
				 *t == _T("ctrl") ||
				 *t == _T("C"))
			mt = Modifier::Type_Control;
		else if (*t == _T("windows") ||
				 *t == _T("win") ||
				 *t == _T("W"))
			mt = Modifier::Type_Windows;
		else if (*t == _T("mod0") ||
				 *t == _T("M0"))
			mt = Modifier::Type_Mod0;
		else if (*t == _T("mod1") ||
				 *t == _T("M1"))
			mt = Modifier::Type_Mod1;
		else if (*t == _T("mod2") ||
				 *t == _T("M2"))
			mt = Modifier::Type_Mod2;
		else if (*t == _T("mod3") ||
				 *t == _T("M3"))
			mt = Modifier::Type_Mod3;
		else if (*t == _T("mod4") ||
				 *t == _T("M4"))
			mt = Modifier::Type_Mod4;
		else if (*t == _T("mod5") ||
				 *t == _T("M5"))
			mt = Modifier::Type_Mod5;
		else if (*t == _T("mod6") ||
				 *t == _T("M6"))
			mt = Modifier::Type_Mod6;
		else if (*t == _T("mod7") ||
				 *t == _T("M7"))
			mt = Modifier::Type_Mod7;
		else if (*t == _T("mod8") ||
				 *t == _T("M8"))
			mt = Modifier::Type_Mod8;
		else if (*t == _T("mod9") ||
				 *t == _T("M9"))
			mt = Modifier::Type_Mod9;
		else
			throw ErrorMessage() << _T("`") << *t
								 << _T("': invalid modifier name @ assign mode.");

		if (am == Keymap::AM_notModifier)
			break;

		m_currentKeymap->addModifier(mt, Keymap::AO_overwrite, am, NULL);
		if (isEOL())
			return;
		t = getToken();
	}

	// <ASSIGN_OP>
	t = getToken();
	Keymap::AssignOperator ao;
	if (*t == _T("="))
		ao = Keymap::AO_new;
	else if (*t == _T("+="))
		ao = Keymap::AO_add;
	else if (*t == _T("-="))
		ao = Keymap::AO_sub;
	else
		throw ErrorMessage() << _T("`") << *t << _T("': is unknown operator.");

	// <ASSIGN_MODE>? <KEY_NAME>
	while (!isEOL())
	{
		// <ASSIGN_MODE>?
		t = getToken();
		Keymap::AssignMode am = Keymap::AM_normal;
		if (*t == _T("!"))
			am = Keymap::AM_true, t = getToken();
		else if (*t == _T("!!"))
			am = Keymap::AM_oneShot, t = getToken();
		else if (*t == _T("!!!"))
			am = Keymap::AM_oneShotRepeatable, t = getToken();
		else if (*t == _T("!!!!"))
			am = Keymap::AM_oneShot2, t = getToken();

		// <KEY_NAME>
		Key *key = m_setting->m_keyboard.searchKey(t->getString());
		if (!key)
			throw ErrorMessage() << _T("`") << *t << _T("': invalid key name @ assign mode key");

		// we can ignore warning C4701
		m_currentKeymap->addModifier(mt, ao, am, key);
		if (ao == Keymap::AO_new)
			ao = Keymap::AO_add;
	}
}

// <KEYSEQ_DEFINITION>
void SettingLoader::load_KEYSEQ_DEFINITION()
{
	if (*getToken() != _T("$"))
		throw ErrorMessage() << _T("there must be `$' after `keyseq'");
	Token *name = getToken();
	if (*getToken() != _T("="))
		throw ErrorMessage() << _T("there must be `=' after keyseq name");
	load_KEY_SEQUENCE(name->getString(), false, Modifier::Type_ASSIGN);
}

// parse a single-element key sequence (modifier* + key/function) for combo/taphold/tapdance
KeySeq *SettingLoader::loadSingleKeySequence()
{
	KeySeq keySeq(_T(""));
	Modifier::Type mode;
	// BASIC modifiers (Shift, Control, Alt, Windows) are set to dontcare so that
	// generateModifierEvents does not release physically-held modifiers before the
	// action key fires (e.g., Shift+TapHold key must emit Shift+B, not bare B).
	// Users who need care+not-pressed explicitly write the modifier prefix (e.g., "S-B").
	Modifier base = m_defaultKeySeqModifier;
	for (int i = Modifier::Type_begin; i < Modifier::Type_BASIC; ++i)
		base.dontcare(static_cast<Modifier::Type>(i));
	Modifier modifier = load_MODIFIER(Modifier::Type_KEYSEQ, base, &mode);
	keySeq.setMode(mode);

	if (isEOL())
		throw ErrorMessage() << _T("action expected.");

	Token *t = lookToken();
	if (t->isOpenParen())
	{
		getToken(); // open paren
		KeySeq *ks = load_KEY_SEQUENCE(_T(""), true);
		getToken(); // close paren
		keySeq.add(ActionKeySeq(ks));
	}
	else if (*t == _T("&"))
	{
		getToken();
		t = getToken();
		ActionFunction af(createFunctionData(t->getString()), modifier);
		if (af.m_functionData == NULL)
			throw ErrorMessage() << _T("`&") << *t << _T("': unknown function name.");
		af.m_functionData->load(this);
		keySeq.add(af);
	}
	else
	{
		ModifiedKey mkey;
		mkey.m_modifier = modifier;
		mkey.m_key = load_KEY_NAME();
		keySeq.add(ActionKey(mkey));
	}
	return m_setting->m_keySeqs.add(keySeq);
}

// Parse an optional modifier prefix for combo/taphold/tapdance rules.
// If the next token(s) are modifier prefixes (e.g., "K0-", "S-"), they are
// consumed and their constraints applied.  If no prefix is present, all
// Type_ASSIGN modifiers are left as dontcare so the rule fires regardless
// of keyboard / layer / user-modifier state (backward-compatible default).
Modifier SettingLoader::load_MODIFIER_for_rule()
{
	Modifier mod;
	mod.dontcare(); // all bits → dontcare; override only those explicitly specified
	return load_MODIFIER(Modifier::Type_ASSIGN, mod);
}

// combo KEY KEY... [window=N] = ACTION
void SettingLoader::loadComboDefinition()
{
	if (!m_currentKeymap)
		throw ErrorMessage() << _T("combo definition requires an active keymap.");

	ComboRule rule;
	rule.m_window = -1;

	// parse optional modifier prefix (e.g., "K0-", "S-"); no prefix → all dontcare
	rule.m_modifier = load_MODIFIER_for_rule();

	// read keys until '=' (possibly preceded by 'window=N')
	while (!isEOL())
	{
		Token *t = lookToken();
		if (*t == _T("="))
		{
			getToken(); // consume '='
			break;
		}
		if (*t == _T("window"))
		{
			getToken(); // consume "window"
			if (*getToken() != _T("="))
				throw ErrorMessage() << _T("'=' expected after 'window' in combo definition.");
			load_ARGUMENT(&rule.m_window);
			continue;
		}
		rule.m_keys.push_back(load_KEY_NAME());
	}

	if (rule.m_keys.size() < 2 || rule.m_keys.size() > 6)
		throw ErrorMessage() << _T("combo requires 2 to 6 keys.");

	rule.m_orderedKeys = rule.m_keys;   // save original definition order for strict-order mode
	std::sort(rule.m_keys.begin(), rule.m_keys.end());

	rule.m_action = load_KEY_SEQUENCE();
	m_currentKeymap->addComboRule(rule);
}

// taphold KEY tap=ACTION hold=ACTION [threshold=N] [interrupt=tap|hold]
void SettingLoader::loadTapHoldDefinition()
{
	if (!m_currentKeymap)
		throw ErrorMessage() << _T("taphold definition requires an active keymap.");

	TapHoldRule rule;
	rule.m_tapAction      = NULL;
	rule.m_holdAction     = NULL;
	rule.m_threshold      = -1;
	rule.m_interrupt      = -1;
	rule.m_permissiveHold = -1;
	rule.m_holdOnOtherKey = -1;
	rule.m_quickTapTerm   = -1;

	// parse optional modifier prefix (e.g., "K0-"); no prefix → all dontcare
	rule.m_modifier = load_MODIFIER_for_rule();

	rule.m_key = load_KEY_NAME();

	{ Token *eq = lookToken(); if (*eq == _T("=")) getToken(); }

	while (!isEOL())
	{
		Token *t = lookToken();
		if (*t == _T("tap"))
		{
			getToken();
			if (*getToken() != _T("="))
				throw ErrorMessage() << _T("'=' expected after 'tap' in taphold definition.");
			rule.m_tapAction = loadSingleKeySequence();
		}
		else if (*t == _T("hold"))
		{
			getToken();
			if (*getToken() != _T("="))
				throw ErrorMessage() << _T("'=' expected after 'hold' in taphold definition.");
			rule.m_holdAction = loadSingleKeySequence();
		}
		else if (*t == _T("threshold"))
		{
			getToken();
			if (*getToken() != _T("="))
				throw ErrorMessage() << _T("'=' expected after 'threshold' in taphold definition.");
			load_ARGUMENT(&rule.m_threshold);
		}
		else if (*t == _T("interrupt"))
		{
			getToken();
			if (*getToken() != _T("="))
				throw ErrorMessage() << _T("'=' expected after 'interrupt' in taphold definition.");
			Token *val = getToken();
			if (*val == _T("tap"))        rule.m_interrupt = 1;
			else if (*val == _T("hold"))  rule.m_interrupt = 0;
			else throw ErrorMessage() << _T("'tap' or 'hold' expected after 'interrupt=' in taphold definition.");
		}
		else if (*t == _T("permissive_hold"))
		{
			getToken();
			if (*getToken() != _T("="))
				throw ErrorMessage() << _T("'=' expected after 'permissive_hold' in taphold definition.");
			Token *val = getToken();
			if (*val == _T("on"))        rule.m_permissiveHold = 1;
			else if (*val == _T("off"))  rule.m_permissiveHold = 0;
			else throw ErrorMessage() << _T("'on' or 'off' expected after 'permissive_hold=' in taphold definition.");
		}
		else if (*t == _T("hold_on_other_key"))
		{
			getToken();
			if (*getToken() != _T("="))
				throw ErrorMessage() << _T("'=' expected after 'hold_on_other_key' in taphold definition.");
			Token *val = getToken();
			if (*val == _T("on"))        rule.m_holdOnOtherKey = 1;
			else if (*val == _T("off"))  rule.m_holdOnOtherKey = 0;
			else throw ErrorMessage() << _T("'on' or 'off' expected after 'hold_on_other_key=' in taphold definition.");
		}
		else if (*t == _T("quick_tap_term"))
		{
			getToken();
			if (*getToken() != _T("="))
				throw ErrorMessage() << _T("'=' expected after 'quick_tap_term' in taphold definition.");
			load_ARGUMENT(&rule.m_quickTapTerm);
		}
		else
		{
			throw ErrorMessage() << _T("unexpected token `") << *t << _T("' in taphold definition.");
		}
	}

	if (rule.m_tapAction == NULL)
		throw ErrorMessage() << _T("taphold: 'tap=' is required.");
	if (rule.m_holdAction == NULL)
		throw ErrorMessage() << _T("taphold: 'hold=' is required.");

	m_currentKeymap->addTapHoldRule(rule);
}

// tapdance KEY tap1=ACTION [tap2=ACTION] [tap3=ACTION] [timeout=N]
void SettingLoader::loadTapDanceDefinition()
{
	if (!m_currentKeymap)
		throw ErrorMessage() << _T("tapdance definition requires an active keymap.");

	TapDanceRule rule;
	rule.m_tap[0] = NULL;
	rule.m_tap[1] = NULL;
	rule.m_tap[2] = NULL;
	rule.m_timeout = -1;

	// parse optional modifier prefix (e.g., "K0-"); no prefix → all dontcare
	rule.m_modifier = load_MODIFIER_for_rule();

	rule.m_key = load_KEY_NAME();

	{ Token *eq = lookToken(); if (*eq == _T("=")) getToken(); }

	while (!isEOL())
	{
		Token *t = lookToken();
		if (*t == _T("tap1"))
		{
			getToken();
			if (*getToken() != _T("="))
				throw ErrorMessage() << _T("'=' expected after 'tap1' in tapdance definition.");
			rule.m_tap[0] = loadSingleKeySequence();
		}
		else if (*t == _T("tap2"))
		{
			getToken();
			if (*getToken() != _T("="))
				throw ErrorMessage() << _T("'=' expected after 'tap2' in tapdance definition.");
			rule.m_tap[1] = loadSingleKeySequence();
		}
		else if (*t == _T("tap3"))
		{
			getToken();
			if (*getToken() != _T("="))
				throw ErrorMessage() << _T("'=' expected after 'tap3' in tapdance definition.");
			rule.m_tap[2] = loadSingleKeySequence();
		}
		else if (*t == _T("timeout"))
		{
			getToken();
			if (*getToken() != _T("="))
				throw ErrorMessage() << _T("'=' expected after 'timeout' in tapdance definition.");
			load_ARGUMENT(&rule.m_timeout);
		}
		else
		{
			throw ErrorMessage() << _T("unexpected token `") << *t << _T("' in tapdance definition.");
		}
	}

	if (rule.m_tap[0] == NULL)
		throw ErrorMessage() << _T("tapdance: 'tap1=' is required.");

	m_currentKeymap->addTapDanceRule(rule);
}

// <DEFINE>
void SettingLoader::load_DEFINE()
{
	m_setting->m_symbols.insert(getToken()->getString());
}

// <IF>
void SettingLoader::load_IF()
{
	if (!getToken()->isOpenParen())
		throw ErrorMessage() << _T("there must be `(' after `if'.");
	Token *t = getToken(); // <SYMBOL> or !
	bool not = false;
	if (*t == _T("!"))
	{
		not = true;
		t = getToken(); // <SYMBOL>
	}

	bool doesSymbolExist = (m_setting->m_symbols.find(t->getString()) != m_setting->m_symbols.end());
	bool doesRead = ((doesSymbolExist && !not) ||
					 (!doesSymbolExist && not));
	if (0 < m_canReadStack.size())
		doesRead = doesRead && m_canReadStack.back();

	if (!getToken()->isCloseParen())
		throw ErrorMessage() << _T("there must be `)'.");

	m_canReadStack.push_back(doesRead);
	if (!isEOL())
	{
		size_t len = m_canReadStack.size();
		load_LINE();
		if (len < m_canReadStack.size())
		{
			bool r = m_canReadStack.back();
			m_canReadStack.pop_back();
			m_canReadStack[len - 1] = r && doesRead;
		}
		else if (len == m_canReadStack.size())
			m_canReadStack.pop_back();
		else
			; // `end' found
	}
}

// <ELSE> <ELSEIF>
void SettingLoader::load_ELSE(bool i_isElseIf, const tstringi &i_token)
{
	bool doesRead = !load_ENDIF(i_token);
	if (0 < m_canReadStack.size())
		doesRead = doesRead && m_canReadStack.back();
	m_canReadStack.push_back(doesRead);
	if (!isEOL())
	{
		size_t len = m_canReadStack.size();
		if (i_isElseIf)
			load_IF();
		else
			load_LINE();
		if (len < m_canReadStack.size())
		{
			bool r = m_canReadStack.back();
			m_canReadStack.pop_back();
			m_canReadStack[len - 1] = doesRead && r;
		}
		else if (len == m_canReadStack.size())
			m_canReadStack.pop_back();
		else
			; // `end' found
	}
}

// <ENDIF>
bool SettingLoader::load_ENDIF(const tstringi &i_token)
{
	if (m_canReadStack.size() == 0)
		throw ErrorMessage() << _T("unbalanced `") << i_token << _T("'");
	bool r = m_canReadStack.back();
	m_canReadStack.pop_back();
	return r;
}

// <LINE>
void SettingLoader::load_LINE()
{
	Token *i_token = getToken();

	// <COND_SYMBOL>
	if (*i_token == _T("if") ||
		*i_token == _T("and"))
		load_IF();
	else if (*i_token == _T("else"))
		load_ELSE(false, i_token->getString());
	else if (*i_token == _T("elseif") ||
			 *i_token == _T("elsif") ||
			 *i_token == _T("elif") ||
			 *i_token == _T("or"))
		load_ELSE(true, i_token->getString());
	else if (*i_token == _T("endif"))
		load_ENDIF(_T("endif"));
	else if (0 < m_canReadStack.size() && !m_canReadStack.back())
	{
		while (!isEOL())
			getToken();
	}
	else if (*i_token == _T("define"))
		load_DEFINE();
	// <INCLUDE>
	else if (*i_token == _T("include"))
		load_INCLUDE();
	// <KEYBOARD_DEFINITION>
	else if (*i_token == _T("def"))
		load_KEYBOARD_DEFINITION();
	// <KEYMAP_DEFINITION>
	else if (*i_token == _T("keymap") ||
			 *i_token == _T("keymap2") ||
			 *i_token == _T("window"))
		load_KEYMAP_DEFINITION(i_token);
	// <KEY_ASSIGN>
	else if (*i_token == _T("key"))
		load_KEY_ASSIGN();
	// <EVENT_ASSIGN>
	else if (*i_token == _T("event"))
		load_EVENT_ASSIGN();
	// <MODIFIER_ASSIGNMENT>
	else if (*i_token == _T("mod"))
		load_MODIFIER_ASSIGNMENT();
	// <KEYSEQ_DEFINITION>
	else if (*i_token == _T("keyseq"))
		load_KEYSEQ_DEFINITION();
	// <COMBO_DEFINITION>
	else if (*i_token == _T("combo"))
		loadComboDefinition();
	// <TAPHOLD_DEFINITION>
	else if (*i_token == _T("taphold"))
		loadTapHoldDefinition();
	// <TAPDANCE_DEFINITION>
	else if (*i_token == _T("tapdance"))
		loadTapDanceDefinition();
	else
		throw ErrorMessage() << _T("syntax error `") << *i_token << _T("'.");
}

// prefix sort predicate used in load(const string &)
static bool prefixSortPred(const tstringi &i_a, const tstringi &i_b)
{
	return i_b.size() < i_a.size();
}

/*
_UNICODE: read file (UTF-16 LE/BE, UTF-8, locale specific multibyte encoding)
_MBCS: read file
*/
bool readFile(tstring *o_data, const tstringi &i_filename)
{
	// get size of file
#if 0
	// bcc's _wstat cannot obtain file size
	struct _stat sbuf;
	if (_tstat(i_filename.c_str(), &sbuf) < 0 || sbuf.st_size == 0)
		return false;
#else
	// so, we use _wstati64 for bcc
	struct stati64_t sbuf;

	// _tstati64で取得されたファイルサイズが0でもGetCompressedFileSizeを取得して
	// シンボリックリンクの場合を救う。
	if (_tstati64(i_filename.c_str(), &sbuf) < 0 || sbuf.st_size == 0)
	{
		DWORD dwLow = (DWORD)-1, dwHigh = 0;

		if (myGetCompressedFileSizeW)
			dwLow = myGetCompressedFileSizeW(i_filename.c_str(), &dwHigh);

		if (dwLow != (DWORD)-1 && GetLastError() == 0)
			sbuf.st_size = ((__int64)dwHigh << 32) + dwLow;
	}

	if (sbuf.st_size == 0)
		return false;

	// following check is needed to cast sbuf.st_size to size_t safely
	// this cast occurs because of above workaround for bcc
	if (sbuf.st_size > UINT_MAX)
		return false;
#endif

	// open
	FILE *fp;
	errno_t tmp_error;
	tmp_error = _tfopen_s(&fp, i_filename.c_str(), _T("rb"));
	if (!fp)
		return false;

	// read file
	Array<BYTE> buf(static_cast<size_t>(sbuf.st_size) + 1);
	if (fread(buf.get(), static_cast<size_t>(sbuf.st_size), 1, fp) != 1)
	{
		fclose(fp);
		return false;
	}
	buf.get()[sbuf.st_size] = 0; // mbstowcs() requires null
								 // terminated string

#ifdef _UNICODE
	//
	// UTF-16 Little Endien BOM (0xFF, 0xFE)
	if (buf.get()[0] == 0xffU && buf.get()[1] == 0xfeU &&
		sbuf.st_size % 2 == 0)
	{
		size_t size = static_cast<size_t>(sbuf.st_size) / 2 - 1; // BOM分減らす
		o_data->resize(size);
		BYTE *p = buf.get() + 2; // skip BOM
		for (size_t i = 0; i < size; ++i)
		{
			wchar_t c = static_cast<wchar_t>(*p++);
			c |= static_cast<wchar_t>(*p++) << 8;
			(*o_data)[i] = c;
		}
		fclose(fp);
		return true;
	}

	//
	// UTF-16 Big Endien BOM (0xFE,0xFF)
	if (buf.get()[0] == 0xfeU && buf.get()[1] == 0xffU &&
		sbuf.st_size % 2 == 0)
	{
		size_t size = static_cast<size_t>(sbuf.st_size) / 2 - 1; // BOM分減らす
		o_data->resize(size);
		BYTE *p = buf.get() + 2; // skip BOM
		for (size_t i = 0; i < size; ++i)
		{
			wchar_t c = static_cast<wchar_t>(*p++) << 8;
			c |= static_cast<wchar_t>(*p++);
			(*o_data)[i] = c;
		}
		fclose(fp);
		return true;
	}
	//
	// UTF-8 with BOM (0xEF,0xBB,0xBF)
	if (buf.get()[0] == 0xefU && buf.get()[1] == 0xbbU && buf.get()[2] == 0xbfU) {
		// Use CP_UTF8 (locale-independent) to correctly convert UTF-8 regardless of system locale
		const char *utf8 = reinterpret_cast<char *>(buf.get() + 3);
		int utf8len = static_cast<int>(sbuf.st_size) - 3;
		int wsize = MultiByteToWideChar(CP_UTF8, 0, utf8, utf8len, NULL, 0);
		if (wsize > 0) {
			Array<wchar_t> wbuf(wsize + 1);
			MultiByteToWideChar(CP_UTF8, 0, utf8, utf8len, wbuf.get(), wsize);
			wbuf.get()[wsize] = L'\0';
			o_data->assign(wbuf.get(), wbuf.get() + wsize);
			fclose(fp);
			return true;
		}
	}
	//
	// try multibyte charset (ASCII / Shift-JIS on Japanese locale)
	{
		const char *mbsrc = reinterpret_cast<char *>(buf.get());
		size_t wcount = 0; // mbstowcs_s returns count including null terminator
		if (mbstowcs_s(&wcount, nullptr, 0, mbsrc, _TRUNCATE) == 0 && wcount > 0) {
			Array<wchar_t> wbuf(wcount);
			mbstowcs_s(nullptr, wbuf.get(), wcount, mbsrc, _TRUNCATE);
			o_data->assign(wbuf.get(), wbuf.get() + wcount - 1); // exclude null terminator
			fclose(fp);
			return true;
		}
	}

	// try UTF-8
	{
		Array<wchar_t> wbuf(static_cast<size_t>(sbuf.st_size));
		BYTE *f = buf.get();
		BYTE *end = buf.get() + sbuf.st_size;
		// skip UTF-8 BOM if present (in case the BOM block above fell through)
		if (end - f >= 3 && f[0] == 0xefU && f[1] == 0xbbU && f[2] == 0xbfU)
			f += 3;
		wchar_t *d = wbuf.get();
		enum
		{
			STATE_1,
			STATE_2of2,
			STATE_2of3,
			STATE_3of3
		} state = STATE_1;

		while (f != end)
		{
			switch (state)
			{
			case STATE_1:
				if (!(*f & 0x80)) // 0xxxxxxx: 00-7F
					*d++ = static_cast<wchar_t>(*f++);
				else if ((*f & 0xe0) == 0xc0) // 110xxxxx 10xxxxxx: 0080-07FF
				{
					*d = ((static_cast<wchar_t>(*f++) & 0x1f) << 6);
					state = STATE_2of2;
				}
				else if ((*f & 0xf0) == 0xe0) // 1110xxxx 10xxxxxx 10xxxxxx:
				// 0800 - FFFF
				{
					*d = ((static_cast<wchar_t>(*f++) & 0x0f) << 12);
					state = STATE_2of3;
				}
				else
					goto not_UTF_8;
				break;

			case STATE_2of2:
			case STATE_3of3:
				if ((*f & 0xc0) != 0x80)
					goto not_UTF_8;
				*d++ |= (static_cast<wchar_t>(*f++) & 0x3f);
				state = STATE_1;
				break;

			case STATE_2of3:
				if ((*f & 0xc0) != 0x80)
					goto not_UTF_8;
				*d |= ((static_cast<wchar_t>(*f++) & 0x3f) << 6);
				state = STATE_3of3;
				break;
			}
		}
		o_data->assign(wbuf.get(), d);
		fclose(fp);
		return true;

	not_UTF_8:;
	}
#endif // _UNICODE

	// assume ascii
	o_data->resize(static_cast<size_t>(sbuf.st_size));
	for (off_t i = 0; i < sbuf.st_size; ++i)
		(*o_data)[i] = buf.get()[i];
	// NULL終端を追加
	o_data->push_back(0);
	fclose(fp);
	return true;
}

// load (called from load(Setting *, const tstringi &) only)
void SettingLoader::load(const tstringi &i_filename)
{
	m_currentFilename = i_filename;

	tstring data;
	if (!readFile(&data, m_currentFilename))
	{
		Acquire a(m_soLog);
		*m_log << m_currentFilename << _T(" : error: file not found") << std::endl;
		{
#ifdef UNICODE
			wchar_t errbuf[256] = {};
			_wcserror_s(errbuf, 256, errno);
#else
			char errbuf[256] = {};
			strerror_s(errbuf, sizeof(errbuf), errno);
#endif
			*m_log << errbuf << std::endl;
		}
#if 1
		*m_log << data << std::endl;
#endif
		m_isThereAnyError = true;
		return;
	}

	// prefix
	if (m_prefixesRefCcount == 0)
	{
		static const _TCHAR *prefixes[] =
			{
				_T("="), _T("=>"), _T("&&"), _T("||"), _T(":"), _T("$"), _T("&"),
				_T("-="), _T("+="), _T("!!!!"), _T("!!!"), _T("!!"), _T("!"),
				_T("E0-"), _T("E1-"),					// <SCAN_CODE_EXTENTION>
				_T("S-"), _T("A-"), _T("M-"), _T("C-"), // <BASIC_MODIFIER>
				_T("W-"), _T("*"), _T("~"),
				_T("U-"), _T("D-"),						  // <KEYSEQ_MODIFIER>
				_T("R-"), _T("IL-"), _T("IC-"), _T("I-"), // <ASSIGN_MODIFIER>
				_T("NL-"), _T("CL-"), _T("SL-"), _T("KL-"),
				_T("MAX-"), _T("MIN-"), _T("MMAX-"), _T("MMIN-"),
				_T("T-"), _T("TS-"),
				_T("TL-"), _T("TLS-"),
				_T("TR-"), _T("TRS-"),
				_T("M0-"), _T("M1-"), _T("M2-"), _T("M3-"), _T("M4-"),
				_T("M5-"), _T("M6-"), _T("M7-"), _T("M8-"), _T("M9-"),
				_T("L0-"), _T("L1-"), _T("L2-"), _T("L3-"), _T("L4-"),
				_T("L5-"), _T("L6-"), _T("L7-"), _T("L8-"), _T("L9-"),
				_T("LA-"), _T("LB-"), _T("LC-"), _T("LD-"), _T("LE-"),
				_T("LF-"),
				_T("K0-"), _T("K1-"), _T("K2-"), _T("K3-"),
				_T("K4-"), _T("K5-"), _T("K6-"), _T("K7-"),
				_T("IH-"), _T("IK-"), _T("IJ-"), _T("IW-"), _T("DP-"), // <ASSIGN_MODIFIER>
			};
		m_prefixes = new std::vector<tstringi>;
		for (size_t i = 0; i < NUMBER_OF(prefixes); ++i)
			m_prefixes->push_back(prefixes[i]);
		std::sort(m_prefixes->begin(), m_prefixes->end(), prefixSortPred);
	}
	m_prefixesRefCcount++;

	// create parser
	Parser parser(data.c_str(), data.size());
	parser.setPrefixes(m_prefixes);

	while (true)
	{
		try
		{
			if (!parser.getLine(&m_tokens))
				break;
			m_ti = m_tokens.begin();
		}
		catch (ErrorMessage &e)
		{
			if (m_log && m_soLog)
			{
				Acquire a(m_soLog);
				*m_log << m_currentFilename << _T("(") << parser.getLineNumber()
					   << _T(") : error: ") << e << std::endl;
			}
			m_isThereAnyError = true;
			continue;
		}

		try
		{
			load_LINE();
			if (!isEOL())
				throw WarningMessage() << _T("back garbage is ignored.");
		}
		catch (WarningMessage &w)
		{
			if (m_log && m_soLog)
			{
				Acquire a(m_soLog);
				*m_log << i_filename << _T("(") << parser.getLineNumber()
					   << _T(") : warning: ") << w << std::endl;
			}
		}
		catch (ErrorMessage &e)
		{
			if (m_log && m_soLog)
			{
				Acquire a(m_soLog);
				*m_log << i_filename << _T("(") << parser.getLineNumber()
					   << _T(") : error: ") << e << std::endl;
			}
			m_isThereAnyError = true;
		}
	}
	

	// m_prefixes
	--m_prefixesRefCcount;
	if (m_prefixesRefCcount == 0)
		delete m_prefixes;

	if (0 < m_canReadStack.size())
	{
		Acquire a(m_soLog);
		*m_log << m_currentFilename << _T("(") << parser.getLineNumber()
			   << _T(") : error: unbalanced `if'.  ")
			   << _T("you forget `endif', didn'i_token you?")
			   << std::endl;
		m_isThereAnyError = true;
	}
}

// is the filename readable ?
bool SettingLoader::isReadable(const tstringi &i_filename,
							   int i_debugLevel) const
{
	if (i_filename.empty())
		return false;

	/*
	#ifdef UNICODE
	tifstream ist(to_string(i_filename).c_str());
	#else
	tifstream ist(i_filename.c_str());
	#endif
	if (ist.good())
	*/
	bool ret = false;
	WIN32_FIND_DATA wfd;
	HANDLE handleFile = FindFirstFile(i_filename.c_str(), &wfd);
	if (handleFile != INVALID_HANDLE_VALUE)
	{
		if (m_log && m_soLog)
		{
			Acquire a(m_soLog, 0);
			*m_log << _T("  loading: ") << i_filename << std::endl;
		}
		ret = true;
	}
	else
	{
		if (m_log && m_soLog)
		{
			Acquire a(m_soLog, i_debugLevel);
			*m_log << _T("not found: ") << i_filename << std::endl;
		}
		ret = false;
	}
	FindClose(handleFile);
	return ret;
}

#if 0
// get filename from registry
bool SettingLoader::getFilenameFromRegistry(tstringi *o_path) const
	{
	// get from registry
	Registry reg(NODOKA_REGISTRY_ROOT);
	int index;
	reg.read(_T(".nodokaIndex"), &index, 0);
	char buf[100];
	snprintf(buf, NUMBER_OF(buf), _T(".nodoka%d"), index);
	if (!reg.read(buf, o_path))
		return false;

	// parse registry entry
	Regexp getFilename(_T("^[^;]*;([^;]*);(.*)$"));
	if (!getFilename.doesMatch(*o_path))
		return false;

	tstringi path = getFilename[1];
	tstringi options = getFilename[2];

	if (!(0 < path.size() && isReadable(path)))
		return false;
	*o_path = path;

	// set symbols
	Regexp symbol(_T("-D([^;]*)"));
	while (symbol.doesMatch(options))
		{
		m_setting->symbols.insert(symbol[1]);
		options = options.substr(symbol.subBegin(1));
		}

	return true;
	}
#endif

// get filename
bool SettingLoader::getFilename(const tstringi &i_name, tstringi *o_path, int i_debugLevel) const
{
	// the default filename is "dot.nodoka"
	const tstringi &name = i_name.empty() ? tstringi(_T("dot.nodoka")) : i_name;

	bool isFirstTime = true;
	HomeDirectories pathes;
	getHomeDirectories(&pathes);

	while (true)
	{
		// find file from registry
		if (i_name.empty()) // called not from 'include'
		{
			Setting::Symbols symbols;

			if (getFilenameFromRegistry(NULL, o_path, &symbols))
			{
				if (o_path->empty()) // for ホームディレクトリから。
				{
					for (HomeDirectories::iterator i = pathes.begin(); i != pathes.end(); ++i)
					{
						*o_path = *i + _T("\\") + name;
						if (isReadable(*o_path, i_debugLevel))
							goto add_symbols;
					}
					return false;
				}
				else
				{
					tstring tmp_name = *o_path;

					tregex getPath(_T("^(.*[/\\\\])[^/\\\\]*$"));
					tsmatch getPathResult;
					if (boost::regex_match(tmp_name, getPathResult, getPath))
					{
						// フルパスが書いてあった
						if (isReadable(tmp_name, i_debugLevel))
						{
							*o_path = tmp_name;
							goto add_symbols;
						}
					}
					else
					{
						// フルパスで無かったので、ホームディレクトリから探す。
						for (HomeDirectories::iterator i = pathes.begin(); i != pathes.end(); ++i)
						{
							*o_path = *i + _T("\\") + tmp_name;
							if (isReadable(*o_path, i_debugLevel))
								goto add_symbols;
						}
					}
					return false;
				}

			add_symbols:
				for (Setting::Symbols::iterator i = symbols.begin(); i != symbols.end(); ++i)
					m_setting->m_symbols.insert(*i);
				return true;
			}
		}

		if (!isFirstTime)
			return false;

		// find file from home directory
		for (HomeDirectories::iterator i = pathes.begin(); i != pathes.end(); ++i)
		{
			*o_path = *i + _T("\\") + name;
			if (isReadable(*o_path, i_debugLevel))
				return true;
		}

		if (!i_name.empty())
			return false; // called by 'include'

		if (!DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_setting), NULL, dlgSetting_dlgProc))
			return false;
	}
}

// constructor
SettingLoader::SettingLoader(SyncObject *i_soLog, tostream *i_log)
    : m_setting(NULL),
      m_isThereAnyError(false),
      m_soLog(i_soLog),
      m_log(i_log),
	  m_currentKeymap(NULL)
{
	m_defaultKeySeqModifier =
		m_defaultAssignModifier.release(Modifier::Type_ImeComp);
}

/* load m_setting
If called by "include", 'filename' describes filename.
Otherwise the 'filename' is empty.
*/
bool SettingLoader::load(Setting *i_setting, const tstringi &i_filename)
{
	m_setting = i_setting;
    m_isThereAnyError = false;

	tstringi path;
	if (!getFilename(i_filename, &path))
	{
		if (i_filename.empty())
		{
			Acquire a(m_soLog);
			getFilename(i_filename, &path, 0); // show filenames
			return false;
		}
		else
			throw ErrorMessage() << _T("`") << i_filename
								 << _T("': no such file or other error.");
	}

	// create global keymap's default keySeq
	ActionFunction af(createFunctionData(_T("OtherWindowClass")));
	KeySeq *globalDefault = m_setting->m_keySeqs.add(KeySeq(_T("")).add(af));

	// add default keymap
	m_currentKeymap = m_setting->m_keymaps.add(
		Keymap(Keymap::Type_windowOr, _T("Global"), _T(""), _T(""),
			   globalDefault, NULL));

	/*
	// add keyboard layout name
	if (filename.empty())
	{
	char keyboardLayoutName[KL_NAMELENGTH];
	if (GetKeyboardLayoutName(keyboardLayoutName))
	{
	tstringi kl = tstringi(_T("KeyboardLayout/")) + keyboardLayoutName;
	m_setting->symbols.insert(kl);
	Acquire a(m_soLog);
	*m_log << _T("KeyboardLayout: ") << kl << std::endl;
	}
	}
	*/

	// load
	load(path);

	// finalize
	if (i_filename.empty())
		m_setting->m_keymaps.adjustModifier(m_setting->m_keyboard);

    return !m_isThereAnyError;
}

std::vector<tstringi> *SettingLoader::m_prefixes; // m_prefixes terminal symbol
size_t SettingLoader::m_prefixesRefCcount;		  /* reference count of
																						m_prefixes */
