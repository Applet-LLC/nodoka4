//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// engine.cpp
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

/*
 KBDADDID_DEBUG_MINIMAL: kbdaddid ExtraInfo 調査用 最小化モード
 有効にすると keyboardHandler ループから以下をすべてスキップし、
 ReadFile() / キューから届いたデータがそのまま quit_loop に渡るようになる:
   - FocusChange イベント
   - ゲームパッド / K Mouse / WM_APP+203 メッセージ処理
   - nodoka 内部リピート (bRepeat) 処理とリピートキャンセル処理
 問題が再現しなくなったら #define を外して一つずつ機能を戻す。
*/
// #define KBDADDID_DEBUG_MINIMAL  // ← 調査時は この行のコメントを外してビルド

#include "misc.h"

#include "engine.h"
#include "errormessage.h"
#include "hook.h"
#include "nodokarc.h"
#include "windowstool.h"
#include "registry.h"
#include "nodoka.h"
#include "hookdata.h"
#include "rawinput.h"
#include "sessiontrace.h"

#include <iomanip>
#include <imm.h>
#include <process.h>
#include <vector>
#pragma comment(lib, "version.lib")

#ifndef MOUSEEVENTF_HWHEEL
#define MOUSEEVENTF_HWHEEL 0x01000 /* hwheel button rolled */
#endif

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif

typedef UINT(CALLBACK *FUNCTYPE4)(HANDLE, UINT, LPVOID, PUINT, UINT);
static FUNCTYPE4 myGetRawInputData = (FUNCTYPE4)GetProcAddress(GetModuleHandle(L"user32.dll"), "GetRawInputData");

typedef UINT(CALLBACK *FUNCTYPE6)(PVOID, PUINT, UINT);
static FUNCTYPE6 myGetRawInputBuffer = (FUNCTYPE6)GetProcAddress(GetModuleHandle(L"user32.dll"), "GetRawInputBuffer");

// check focus window
bool Engine::checkFocusWindow()
{
	int count = 0;

restart:
	count++;

	HWND hwndFore = GetForegroundWindow();
	DWORD threadId = GetWindowThreadProcessId(hwndFore, NULL);

	if (hwndFore)
	{
		{
			Acquire a(&m_cs);
			if (m_currentFocusOfThread &&
				m_currentFocusOfThread->m_threadId == threadId &&
				m_currentFocusOfThread->m_hwndFocus == m_hwndFocus)
			{
				return false;
			}

			m_emacsEditKillLine.reset();

			// erase dead thread
			if (!m_detachedThreadIds.empty())
			{
				for (DetachedThreadIds::iterator i = m_detachedThreadIds.begin();
					 i != m_detachedThreadIds.end(); i++)
				{
					FocusOfThreads::iterator j = m_focusOfThreads.find((*i));
					if (j != m_focusOfThreads.end())
					{
						FocusOfThread *fot = &((*j).second);
						Acquire a(&m_log, 1);
						m_log << _T("RemoveThread") << std::endl;
						m_log << _T("\tHWND:\t") << std::hex << (DWORD_PTR)fot->m_hwndFocus
							  << std::dec << std::endl;
						m_log << _T("\tTHREADID:") << fot->m_threadId << std::endl;
						m_log << _T("\tCLASS:\t") << fot->m_className << std::endl;
						m_log << _T("\tTITLE:\t") << fot->m_titleName << std::endl;
						m_log << std::endl;
						m_focusOfThreads.erase(j);
					}
				}
				m_detachedThreadIds.erase(m_detachedThreadIds.begin(), m_detachedThreadIds.end());
			}

			FocusOfThreads::iterator i = m_focusOfThreads.find(threadId);
			if (i != m_focusOfThreads.end())
			{
				m_currentFocusOfThread = &((*i).second);
				if (!m_currentFocusOfThread->m_isConsole || 2 <= count)
				{
					if (m_currentFocusOfThread->m_keymaps.empty())
						setCurrentKeymap(NULL);
					else
						setCurrentKeymap(*m_currentFocusOfThread->m_keymaps.begin());
					m_hwndFocus = m_currentFocusOfThread->m_hwndFocus;
					checkShow(m_hwndFocus);

					Acquire a(&m_log, 1);
					m_log << _T("FocusChanged") << std::endl;
					m_log << _T("\tHWND:\t")
						  << std::hex << (DWORD_PTR)m_currentFocusOfThread->m_hwndFocus
						  << std::dec << std::endl;
					m_log << _T("\tTHREADID:")
						  << m_currentFocusOfThread->m_threadId << std::endl;
					m_log << _T("\tCLASS:\t")
						  << m_currentFocusOfThread->m_className << std::endl;
					m_log << _T("\tTITLE:\t")
						  << m_currentFocusOfThread->m_titleName << std::endl;
					m_log << std::endl;
					return true;
				}
			}
		}

		_TCHAR className[GANA_MAX_ATOM_LENGTH];
		if (GetClassName(hwndFore, className, NUMBER_OF(className)))
		{
			if (_tcsicmp(className, _T("ConsoleWindowClass")) == 0)
			{
				_TCHAR titleName[1024];
				if (GetWindowText(hwndFore, titleName, NUMBER_OF(titleName)) == 0)
					titleName[0] = _T('\0');
				setFocus(hwndFore, threadId, className, titleName, true);
				Acquire a(&m_log, 1);
				m_log << _T("HWND:\t") << std::hex << reinterpret_cast<DWORD_PTR>(hwndFore)
					  << std::dec << std::endl;
				m_log << _T("THREADID:") << threadId << std::endl;
				m_log << _T("CLASS:\t") << className << std::endl;
				m_log << _T("TITLE:\t") << titleName << std::endl
					  << std::endl;
				goto restart;
			}
		}
	}

	Acquire a(&m_cs);
	if (m_globalFocus.m_keymaps.empty())
	{
		Acquire a(&m_log, 0);
		m_log << _T("NO GLOBAL FOCUS") << std::endl;
		m_currentFocusOfThread = NULL;
		setCurrentKeymap(NULL);
	}
	else
	{
		if (m_currentFocusOfThread != &m_globalFocus)
		{
			Acquire a(&m_log, 1);
			m_log << _T("GLOBAL FOCUS") << std::endl;
			m_currentFocusOfThread = &m_globalFocus;
			setCurrentKeymap(m_globalFocus.m_keymaps.front());
		}
	}
	m_hwndFocus = NULL;

	return false;
}

// rawinput for 複数キーボード
/*
int Engine::getUnitID()
{
    while(myGetRawInputBuffer != NULL)
    {
        // GetRawInputBufferを呼んで pRawInputにデータを、またnInputに個数を取得する
        UINT cbSizeT = rawcbSize;
        UINT nInput = myGetRawInputBuffer(pRawInput, &cbSizeT, sizeof(RAWINPUTHEADER));

        // 空なら抜ける
        if (nInput <= 0) 
        {
            break;
        }

        // nInput分の領域を準備する
        PRAWINPUT* paRawInput = (PRAWINPUT*)malloc(sizeof(PRAWINPUT) * nInput);
        if (paRawInput == NULL) 
        {
            //Log(_T("paRawInput NULL"));
            break; 
        }

        // nInput分だけ回して、pRawInputからデータを取り出し、paRawInputに格納する
        PRAWINPUT pri = pRawInput;
        for (UINT i = 0; i < nInput; ++i) 
        { 
            //Log(_T(" input[%d] = @%p"), i, pri);
            paRawInput[i] = pri;
            pri = NEXTRAWINPUTBLOCK(pri);
        }
        // アプリで処理しないものはdefaultに任せます。
        DefRawInputProc(paRawInput, nInput, sizeof(RAWINPUTHEADER));

        // メモリを初期化します。
        free(paRawInput);
    }
    free(pRawInput);
	return 0;
} 
*/

// get KeyState[]
int Engine::getKeyState(u_int8 e0e1_flag, USHORT key, USHORT UnitId)
{
	// e0e1_flag is 0:none, 1:E0, 2:E1, 3: E0E1
	if (key > 255 || e0e1_flag > 3 || UnitId > 7)
		return 0; // 引数が不正なので0を返す。

	int index = (UnitId * 1024) + (e0e1_flag * 256) + key;
	return m_setting->m_keyState[index].state;

	/*
	if(e0e1_flag == 0)				
	return m_setting->m_keyState[key].state;
	if(e0e1_flag == 1)
	return m_setting->m_keyState[key + 256].state;
	if(e0e1_flag == 2)
	return m_setting->m_keyState[key + 512].state;
	if(e0e1_flag == 3)
	return m_setting->m_keyState[key + 768].state;
	*/

	return 0;
}

// set KeyState[]
void Engine::setKeyState(u_int8 e0e1_flag, USHORT key, USHORT UnitId, int state)
{
	if (key > 255 || e0e1_flag > 3 || state < -1 || state > 6 || UnitId > 7)
		return; // 引数が不正なので何もしない。

	int index = (UnitId * 1024) + (e0e1_flag * 256) + key;
	m_setting->m_keyState[index].state = state;

	/*
	if(e0e1_flag == 0)				
	m_setting->m_keyState[key].state = state;
	if(e0e1_flag == 1)
	m_setting->m_keyState[key + 256].state = state;
	if(e0e1_flag == 2)
	m_setting->m_keyState[key + 512].state = state;
	if(e0e1_flag == 3)
	m_setting->m_keyState[key + 768].state = state;
	*/

	return;
}

// get KeyState[] 0:st1, 1:st2, 2:st3
DWORD Engine::getKeyStateTime(u_int8 e0e1_flag, USHORT key, USHORT UnitId, int num)
{
	DWORD st = 0;
	if (key > 255 || e0e1_flag > 3 || UnitId > 7 || num > 2)
		return st; // 引数が不正なので終了。

	int index = (UnitId * 1024) + (e0e1_flag * 256) + key;

	switch (num)
	{
	case 0:
		return m_setting->m_keyState[index].st1;
		break;
	case 1:
		return m_setting->m_keyState[index].st2;
		break;
	case 2:
		return m_setting->m_keyState[index].st3;
		break;
	}

	return st;
}

// set KeyState[] 0:st1, 1:st2, 2:st3
void Engine::setKeyStateTime(u_int8 e0e1_flag, USHORT key, USHORT UnitId, int num, DWORD st)
{
	if (key > 255 || e0e1_flag > 3 || UnitId > 7 || num > 2)
		return; // 引数が不正なので終了

	int index = (UnitId * 1024) + (e0e1_flag * 256) + key;

	switch (num)
	{
	case 0:
		m_setting->m_keyState[index].st1 = st;
		break;
	case 1:
		m_setting->m_keyState[index].st2 = st;
		break;
	case 2:
		m_setting->m_keyState[index].st3 = st;
		break;
	}

	return;
}

// get KeyState[] dealyA, delayB, delayI
DWORD Engine::getKeyStateDelay(u_int8 e0e1_flag, USHORT key, USHORT UnitId, int delay)
{
	if (key > 255 || e0e1_flag > 3 || UnitId > 7 || delay > 2)
		return 0; // 引数が不正なので終了。

	int index = (UnitId * 1024) + (e0e1_flag * 256) + key;

	switch (delay)
	{
	case 0:
		return m_setting->m_keyState[index].delayA;
		break;
	case 1:
		return m_setting->m_keyState[index].delayB;
		break;
	case 2:
		return m_setting->m_keyState[index].delayI;
		break;
	default:
		return 0;
		break;
	}
	return 0;
}

// is modifier pressed ?
bool Engine::isPressed(Modifier::Type i_mt)
{
	const Keymap::ModAssignments &ma = m_currentKeymap->getModAssignments(i_mt);

	for (Keymap::ModAssignments::const_iterator i = ma.begin();
		 i != ma.end(); ++i)
		if ((*i).m_key->m_isPressed)
			return true;
	return false;
}

// fix modifier key (if fixed, return true)
bool Engine::fixModifierKey(ModifiedKey *io_mkey, Keymap::AssignMode *o_am)
{
	// for all modifier ...

	for (int i = Modifier::Type_begin; i != Modifier::Type_end; ++i)
	{
		// get modifier assignments (list of modifier keys)
		const Keymap::ModAssignments &ma =
			m_currentKeymap->getModAssignments(static_cast<Modifier::Type>(i));

		for (Keymap::ModAssignments::const_iterator
				 j = ma.begin();
			 j != ma.end(); ++j)
			if (io_mkey->m_key == (*j).m_key) // is io_mkey a modifier ?
			{
				{
					Acquire a(&m_log, 1);
					m_log << _T("* Modifier Key") << std::endl;
				}
				// set dontcare for this modifier
				io_mkey->m_modifier.dontcare(static_cast<Modifier::Type>(i));
				*o_am = (*j).m_assignMode;
				return true;
			}
	}
	*o_am = Keymap::AM_notModifier;
	return false;
}

// output to m_log
void Engine::outputToLog(const Key *i_key, const ModifiedKey &i_mkey,
						 int i_debugLevel)
{
	size_t i;
	Acquire a(&m_log, i_debugLevel);

	// output scan codes
	for (i = 0; i < i_key->getScanCodesSize(); ++i)
	{
		if (i_key->getScanCodes()[i].m_flags & ScanCode::E0)
			m_log << _T("E0-");
		if (i_key->getScanCodes()[i].m_flags & ScanCode::E1)
			m_log << _T("E1-");
		m_log << _T("0x") << std::hex << std::setw(2) << std::setfill(_T('0'))
			  << static_cast<int>(i_key->getScanCodes()[i].m_scan)
			  << std::dec << _T(" ");
	}

	if (!i_mkey.m_key) // key corresponds to no phisical key
	{
		m_log << std::endl;
		return;
	}

	m_log << _T("  ") << i_mkey << std::endl;
}

// describe bindings
void Engine::describeBindings()
{
	Acquire a(&m_log, 0);

	Keymap::DescribeParam dp;
	for (KeymapPtrList::iterator i = m_currentFocusOfThread->m_keymaps.begin();
		 i != m_currentFocusOfThread->m_keymaps.end(); ++i)
		(*i)->describe(m_log, &dp);
	m_log << std::endl;
}

// update m_lastPressedKey
void Engine::updateLastPressedKey(Key *i_key)
{
	m_lastPressedKey[1] = m_lastPressedKey[0];
	m_lastPressedKey[0] = i_key;
}

// set current keymap
void Engine::setCurrentKeymap(const Keymap *i_keymap, bool i_doesAddToHistory)
{
	if (i_doesAddToHistory)
	{
		m_keymapPrefixHistory.push_back(const_cast<Keymap *>(m_currentKeymap));
		if (MAX_KEYMAP_PREFIX_HISTORY < m_keymapPrefixHistory.size())
			m_keymapPrefixHistory.pop_front();
	}
	else
		m_keymapPrefixHistory.clear();
	m_currentKeymap = i_keymap;
}

// get current modifiers
Modifier Engine::getCurrentModifiers(Key *i_key, bool i_isPressed)
{
	Modifier cmods;
	cmods.add(m_currentLock);

	cmods.press(Modifier::Type_Shift, isPressed(Modifier::Type_Shift));
	cmods.press(Modifier::Type_Alt, isPressed(Modifier::Type_Alt));
	cmods.press(Modifier::Type_Control, isPressed(Modifier::Type_Control));
	cmods.press(Modifier::Type_Windows, isPressed(Modifier::Type_Windows));
	cmods.press(Modifier::Type_Up, !i_isPressed);
	cmods.press(Modifier::Type_Down, i_isPressed);

	cmods.press(Modifier::Type_Repeat, false);
	if (m_lastPressedKey[0] == i_key)
	{
		if (i_isPressed) // D-R-x
			cmods.press(Modifier::Type_Repeat, true);
		else // U-R-x
			if (m_lastPressedKey[1] == i_key)
			cmods.press(Modifier::Type_Repeat, true);
	}

	for (int i = Modifier::Type_Mod0; i <= Modifier::Type_Mod9; ++i)
		cmods.press(static_cast<Modifier::Type>(i),
					isPressed(static_cast<Modifier::Type>(i)));

	return cmods;
}

// generate keyboard event for a key
void Engine::generateKeyEvent(Key *i_key, bool i_doPress, bool i_isByAssign)
{
	//SYSTEMTIME st;

	// check if key is event
	bool isEvent = false;
	for (Key **e = Event::events; *e; ++e)
		if (*e == i_key)
		{
			isEvent = true;
			break;
		}

	bool isAlreadyReleased = false;

	if (!isEvent)
	{
		if (i_doPress && !i_key->m_isPressedOnWin32)
			++m_currentKeyPressCountOnWin32;
		else if (!i_doPress)
		{
			if (i_key->m_isPressedOnWin32)
				--m_currentKeyPressCountOnWin32;
			else
				isAlreadyReleased = true;
		}
		i_key->m_isPressedOnWin32 = i_doPress;

		if (i_isByAssign)
			i_key->m_isPressedByAssign = i_doPress;

		Key *sync = m_setting->m_keyboard.getSyncKey();

		if (!isAlreadyReleased || i_key == sync)
		{
			KEYBOARD_INPUT_DATA kid = {0, 0, 0, 0, 0};
			const ScanCode *sc = i_key->getScanCodes();
			for (size_t i = 0; i < i_key->getScanCodesSize(); ++i)
			{
				kid.MakeCode = sc[i].m_scan;
				kid.Flags = sc[i].m_flags;
				if (!i_doPress)
					kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
				injectInputOut(&kid);
			}
			m_lastGeneratedKey = i_doPress ? i_key : NULL;
		}
	}

	{
		Acquire a(&m_log, 1);
		m_log << _T("\t\t    =>\t");
		if (isAlreadyReleased)
			m_log << _T("(already released) ");
	}

	ModifiedKey mkey(i_key);
	mkey.m_modifier.on(Modifier::Type_Up, !i_doPress);
	mkey.m_modifier.on(Modifier::Type_Down, i_doPress);
	outputToLog(i_key, mkey, 1);
}

// genete event
void Engine::generateEvents(Current i_c, const Keymap *i_keymap, Key *i_event)
{
	// generate
	i_c.m_keymap = i_keymap;
	i_c.m_mkey.m_key = i_event;
	if (const Keymap::KeyAssignment *keyAssign =
			i_c.m_keymap->searchAssignment(i_c.m_mkey))
	{
		{
			Acquire a(&m_log, 1);
			m_log << std::endl
				  << _T("           ")
				  << i_event->getName() << std::endl;
		}
		generateKeySeqEvents(i_c, keyAssign->m_keySeq, Part_all);
	}
}

// genete modifier events
void Engine::generateModifierEvents(const Modifier &i_mod)
{
	{
		Acquire a(&m_log, 1);
		m_log << _T("* Gen Modifiers\t{") << std::endl;
	}

	for (int i = Modifier::Type_begin; i < Modifier::Type_BASIC; ++i)
	{
		Keyboard::Mods &mods =
			m_setting->m_keyboard.getModifiers(static_cast<Modifier::Type>(i));

		if (i_mod.isDontcare(static_cast<Modifier::Type>(i)))
			// no need to process
			;
		else if (i_mod.isPressed(static_cast<Modifier::Type>(i)))
		// we have to press this modifier
		{
			bool noneIsPressed = true;
			bool noneIsPressedByAssign = true;
			for (Keyboard::Mods::iterator i = mods.begin(); i != mods.end(); ++i)
			{
				if ((*i)->m_isPressedOnWin32)
					noneIsPressed = false;
				if ((*i)->m_isPressedByAssign)
					noneIsPressedByAssign = false;
			}
			if (noneIsPressed)
			{
				if (noneIsPressedByAssign)
					generateKeyEvent(mods.front(), true, false);
				else
					for (Keyboard::Mods::iterator
							 i = mods.begin();
						 i != mods.end(); ++i)
						if ((*i)->m_isPressedByAssign)
							generateKeyEvent((*i), true, false);
			}
		}

		else
		// we have to release this modifier
		{
			// avoid such sequences as  "Alt U-ALt" or "Windows U-Windows"
			if (i == Modifier::Type_Alt || i == Modifier::Type_Windows)
			{
				for (Keyboard::Mods::iterator j = mods.begin(); j != mods.end(); ++j)
					if ((*j) == m_lastGeneratedKey)
					{
						Keyboard::Mods *mods =
							&m_setting->m_keyboard.getModifiers(Modifier::Type_Shift);
						if (mods->size() == 0)
							mods = &m_setting->m_keyboard.getModifiers(
								Modifier::Type_Control);
						if (0 < mods->size())
						{
							generateKeyEvent(mods->front(), true, false);
							generateKeyEvent(mods->front(), false, false);
						}
						break;
					}
			}

			for (Keyboard::Mods::iterator j = mods.begin(); j != mods.end(); ++j)
			{
				if ((*j)->m_isPressedOnWin32)
					generateKeyEvent((*j), false, false);
			}
		}
	}

	{
		Acquire a(&m_log, 1);
		m_log << _T("\t\t}") << std::endl;
	}
}

// generate keyboard events for action
void Engine::generateActionEvents(const Current &i_c, const Action *i_a,
								  bool i_doPress)
{
	switch (i_a->getType())
	{
		// key
	case Action::Type_key:
	{
#ifndef FOR_LIMIT
		// &WaitKeyによる Wait処理
		DWORD dwWaitKey = g_hookDataExe->m_WaitKey;
		if (dwWaitKey >= 0 && dwWaitKey <= 5000)
			Sleep(dwWaitKey);
#endif
		const ModifiedKey &mkey = reinterpret_cast<ActionKey *>(
									  const_cast<Action *>(i_a))
									  ->m_modifiedKey;

		// release
		if (!i_doPress &&
			(mkey.m_modifier.isOn(Modifier::Type_Up) ||
			 mkey.m_modifier.isDontcare(Modifier::Type_Up)))
			generateKeyEvent(mkey.m_key, false, true);

		// press
		else if (i_doPress &&
				 (mkey.m_modifier.isOn(Modifier::Type_Down) ||
				  mkey.m_modifier.isDontcare(Modifier::Type_Down)))
		{
			Modifier modifier = mkey.m_modifier;
			modifier.add(i_c.m_mkey.m_modifier);
			generateModifierEvents(modifier);
			generateKeyEvent(mkey.m_key, true, true);
		}
		break;
	}

		// keyseq
	case Action::Type_keySeq:
	{
		const ActionKeySeq *aks = reinterpret_cast<const ActionKeySeq *>(i_a);
		generateKeySeqEvents(i_c, aks->m_keySeq,
							 i_doPress ? Part_down : Part_up);
		break;
	}

		// function
	case Action::Type_function:
	{
#ifndef FOR_LIMIT
		// &WaitKeyによる Wait処理
		DWORD dwWaitKey = g_hookDataExe->m_WaitKey;
		if (dwWaitKey >= 0 && dwWaitKey <= 5000)
			Sleep(dwWaitKey);
#endif
		const ActionFunction *af = reinterpret_cast<const ActionFunction *>(i_a);
		bool is_up = (!i_doPress &&
					  (af->m_modifier.isOn(Modifier::Type_Up) ||
					   af->m_modifier.isDontcare(Modifier::Type_Up)));
		bool is_down = (i_doPress &&
						(af->m_modifier.isOn(Modifier::Type_Down) ||
						 af->m_modifier.isDontcare(Modifier::Type_Down)));

		if (!is_down && !is_up)
			break;

		{
			Acquire a(&m_log, 1);
			m_log << _T("\t\t     >\t") << af->m_functionData;
		}

		FunctionParam param;
		param.m_isPressed = i_doPress;
		param.m_hwnd = m_currentFocusOfThread->m_hwndFocus;
		param.m_c = i_c;
		param.m_doesNeedEndl = true;
		param.m_af = af;

		param.m_c.m_mkey.m_modifier.on(Modifier::Type_Up, !i_doPress);
		param.m_c.m_mkey.m_modifier.on(Modifier::Type_Down, i_doPress);

		af->m_functionData->exec(this, &param);

		if (param.m_doesNeedEndl)
		{
			Acquire a(&m_log, 1);
			m_log << std::endl;
		}
		break;
	}
	}
}

// generate keyboard events for keySeq
void Engine::generateKeySeqEvents(const Current &i_c, const KeySeq *i_keySeq,
								  Part i_part)
{
	const KeySeq::Actions &actions = i_keySeq->getActions();
	if (actions.empty())
		return;
	if (i_part == Part_up)
	{
		generateActionEvents(i_c, actions[actions.size() - 1], false);
	}
	else
	{
		size_t i;
		for (i = 0; i < actions.size() - 1; ++i)
		{
			generateActionEvents(i_c, actions[i], true);
			generateActionEvents(i_c, actions[i], false);
		}
		generateActionEvents(i_c, actions[i], true);
		if (i_part == Part_all)
			generateActionEvents(i_c, actions[i], false);
	}
}

// generate keyboard events for current key
void Engine::generateKeyboardEvents(const Current &i_c)
{
	if (++m_generateKeyboardEventsRecursionGuard ==
		MAX_GENERATE_KEYBOARD_EVENTS_RECURSION_COUNT)
	{
		Acquire a(&m_log);
		m_log << _T("error: too deep keymap recursion.  there may be a loop.")
			  << std::endl;
		return;
	}

	const Keymap::KeyAssignment *keyAssign = i_c.m_keymap->searchAssignment(i_c.m_mkey);
	if (!keyAssign)
	{
		const KeySeq *keySeq = i_c.m_keymap->getDefaultKeySeq();
		ASSERT(keySeq);
		generateKeySeqEvents(i_c, keySeq, i_c.isPressed() ? Part_down : Part_up);
	}
	else
	{
		if (keyAssign->m_modifiedKey.m_modifier.isOn(Modifier::Type_Up) ||
			keyAssign->m_modifiedKey.m_modifier.isOn(Modifier::Type_Down))
		{
			generateKeySeqEvents(i_c, keyAssign->m_keySeq, Part_all); // down or up
		}
		else
		{
			generateKeySeqEvents(i_c, keyAssign->m_keySeq,
								 i_c.isPressed() ? Part_down : Part_up); // down, up 以外
		}
	}
	m_generateKeyboardEventsRecursionGuard--;
}

// generate keyboard events for current key
void Engine::beginGeneratingKeyboardEvents(
	const Current &i_c, bool i_isModifier, bool oneShot2)
{
	//             (1)             (2)             (3)  (4)   (1)
	// up/down:    D-              U-              D-   U-    D-
	// keymap:     m_currentKeymap m_currentKeymap X    X     m_currentKeymap
	// memo:       &Prefix(X)      ...             ...  ...   ...
	// m_isPrefix: false           true            true false false

	Current cnew(i_c);

	bool isPhysicallyPressed = cnew.m_mkey.m_modifier.isPressed(Modifier::Type_Down); // モディファイヤーが押されたか?

	// substitute
	ModifiedKey mkey = m_setting->m_keyboard.searchSubstitute(cnew.m_mkey);
	if (mkey.m_key)
	{
		cnew.m_mkey = mkey;
		if (isPhysicallyPressed)
		{
			cnew.m_mkey.m_modifier.off(Modifier::Type_Up);
			cnew.m_mkey.m_modifier.on(Modifier::Type_Down);
		}
		else
		{
			cnew.m_mkey.m_modifier.on(Modifier::Type_Up);
			cnew.m_mkey.m_modifier.off(Modifier::Type_Down);
		}
		for (int i = Modifier::Type_begin; i != Modifier::Type_end; ++i)
		{
			Modifier::Type type = static_cast<Modifier::Type>(i);
			if (cnew.m_mkey.m_modifier.isDontcare(type) &&
				!i_c.m_mkey.m_modifier.isDontcare(type))
				cnew.m_mkey.m_modifier.press(
					type, i_c.m_mkey.m_modifier.isPressed(type));
		}

		{
			Acquire a(&m_log, 1);
			m_log << _T("* substitute") << std::endl;
		}
		outputToLog(mkey.m_key, cnew.m_mkey, 1);
	}

	// for prefix key
	const Keymap *tmpKeymap = m_currentKeymap;
	if (i_isModifier || !m_isPrefix)
		;															 // 通常キー処理(モディファイヤーの有無付き) で、プレフィックスが無い時
	else if (isPhysicallyPressed)									 // when (3)			// ではなくて、モディファイヤー付きでキーが押されているとき
		m_isPrefix = false;											 // m_isPrefixは falseに。
	else if (!isPhysicallyPressed)									 // when (2)				// あるいは キーが押されていない時
		m_currentKeymap = m_currentFocusOfThread->m_keymaps.front(); // カレントキーマップを変える。

	// for m_emacsEditKillLine function
	m_emacsEditKillLine.m_doForceReset = !i_isModifier;

	// generate key event !
	m_generateKeyboardEventsRecursionGuard = 0;
	if (isPhysicallyPressed)
		generateEvents(cnew, cnew.m_keymap, &Event::before_key_down);
	generateKeyboardEvents(cnew);
	if (!isPhysicallyPressed)
		generateEvents(cnew, cnew.m_keymap, &Event::after_key_up);
	if (oneShot2)
	{
		{
			Acquire a(&m_log, 1);
			m_log << _T("oneShot2 efect") << std::endl;
		}
		const KeySeq *keySeq = cnew.m_keymap->getDefaultKeySeq();
		generateKeySeqEvents(cnew, keySeq, Part_up);
	}
	// for m_emacsEditKillLine function
	if (m_emacsEditKillLine.m_doForceReset)
		m_emacsEditKillLine.reset();

	// for prefix key
	if (i_isModifier)
		;
	else if (!m_isPrefix) // when (1), (4)
		m_currentKeymap = m_currentFocusOfThread->m_keymaps.front();
	else if (!isPhysicallyPressed) // when (2)
		m_currentKeymap = tmpKeymap;
}

void Engine::injectInputIn(KEYBOARD_INPUT_DATA *i_kid)
{

	KEYBOARD_PAST delay_data;

	if (m_setting && m_isEnabled && (m_setting->m_UseDoublePress == true))
	{
		// Mutexが取れ次第、Queueに入れる
		WaitForSingleObject(m_pastQueueMutex, INFINITE);

		delay_data.kid = *i_kid;
		delay_data.time_stamp = timeGetTime();
		delay_data.number = m_setting->m_number++;

		m_pastQueue->push_back(delay_data);
		ReleaseMutex(m_pastQueueMutex);

		// DP検出のためにSleepを入れる。
		int dwDelay = m_setting->m_DoublePressDelay;

		if (dwDelay < 0)
			dwDelay = 0;
		if (dwDelay > 100)
			dwDelay = 100;

		Sleep(dwDelay);
	}

	if (m_setting && m_isEnabled && (m_setting->m_For6point == true))
	{
		// Mutexが取れ次第、Queueに入れる
		WaitForSingleObject(m_for6pointQueueMutex, INFINITE);

		delay_data.kid = *i_kid;
		delay_data.time_stamp = 0;
		delay_data.number = 0;

		m_for6pointQueue->push_back(delay_data);
		ReleaseMutex(m_for6pointQueueMutex);
	}

	return;
}

void Engine::injectInputOut(KEYBOARD_INPUT_DATA *i_kid)
{
	if ((i_kid->Flags & KEYBOARD_INPUT_DATA::E1) && (i_kid->MakeCode < 0x0A))
	{
		INPUT kid[2];
		int count = 1;

		kid[0].type = INPUT_MOUSE;
		kid[0].mi.dx = 0;
		kid[0].mi.dy = 0;
		kid[0].mi.time = 0;
		kid[0].mi.mouseData = 0;
		kid[0].mi.dwExtraInfo = 0;
		switch (i_kid->MakeCode)
		{
		case 1:
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK)
			{
				kid[0].mi.dwFlags = MOUSEEVENTF_LEFTUP;
			}
			else
			{
				kid[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
			}
			break;
		case 2:
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK)
			{
				kid[0].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
			}
			else
			{
				kid[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
			}
			break;
		case 3:
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK)
			{
				kid[0].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
			}
			else
			{
				kid[0].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
			}
			break;
		case 4:
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK)
			{
				return;
			}
			else
			{
				kid[0].mi.mouseData = WHEEL_DELTA;
				kid[0].mi.dwFlags = MOUSEEVENTF_WHEEL;
			}
			break;
		case 5:
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK)
			{
				return;
			}
			else
			{
				kid[0].mi.mouseData = -WHEEL_DELTA;
				kid[0].mi.dwFlags = MOUSEEVENTF_WHEEL;
			}
			break;
		case 6:
			kid[0].mi.mouseData = XBUTTON1;
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK)
			{
				kid[0].mi.dwFlags = MOUSEEVENTF_XUP;
			}
			else
			{
				kid[0].mi.dwFlags = MOUSEEVENTF_XDOWN;
			}
			break;
		case 7:
			kid[0].mi.mouseData = XBUTTON2;
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK)
			{
				kid[0].mi.dwFlags = MOUSEEVENTF_XUP;
			}
			else
			{
				kid[0].mi.dwFlags = MOUSEEVENTF_XDOWN;
			}
			break;
		case 8:
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK)
			{
				return;
			}
			else
			{
				kid[0].mi.mouseData = WHEEL_DELTA;
				kid[0].mi.dwFlags = MOUSEEVENTF_HWHEEL;
			}
			break;
		case 9:
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK)
			{
				return;
			}
			else
			{
				kid[0].mi.mouseData = -WHEEL_DELTA;
				kid[0].mi.dwFlags = MOUSEEVENTF_HWHEEL;
			}
			break;
		default:
			return;
			break;
		}
		if (!(i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK) &&
			i_kid->MakeCode != 4 && i_kid->MakeCode != 5 &&
			i_kid->MakeCode != 8 && i_kid->MakeCode != 9)
		{
			HWND hwnd;
			POINT pt;

			if (GetCursorPos(&pt) && (hwnd = WindowFromPoint(pt)))
			{
				_TCHAR className[GANA_MAX_ATOM_LENGTH];
				if (GetClassName(hwnd, className, NUMBER_OF(className)))
				{
					if (_tcsicmp(className, _T("ConsoleWindowClass")) == 0)
					{
						SetForegroundWindow(hwnd);
					}
				}
			}
			if (m_dragging)
			{
				kid[0].mi.dx = 65535 * m_msllHookCurrent.pt.x / GetSystemMetrics(SM_CXVIRTUALSCREEN);
				kid[0].mi.dy = 65535 * m_msllHookCurrent.pt.y / GetSystemMetrics(SM_CYVIRTUALSCREEN);
				kid[0].mi.dwFlags |= MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;

				kid[1].type = INPUT_MOUSE;
				kid[1].mi.dx = 65535 * pt.x / GetSystemMetrics(SM_CXVIRTUALSCREEN);
				kid[1].mi.dy = 65535 * pt.y / GetSystemMetrics(SM_CYVIRTUALSCREEN);
				kid[1].mi.time = 0;
				kid[1].mi.mouseData = 0;
				kid[1].mi.dwExtraInfo = 0;
				kid[1].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;

				count = 2;
			}
		}
		SendInput(count, &kid[0], sizeof(kid[0]));
	}
	else
	{
		if (m_device != INVALID_HANDLE_VALUE && m_keyboard_hook == 0)
		{
			DWORD m_len = 0;
			i_kid->Reserved = (USHORT)0;
			i_kid->ExtraInformation = (ULONG)0;
			// nodokad's output queue (FilterDeviceExtension::readQue, capacity ~100,
			// see nodokad.h KeyQueSize) silently truncates the write and still returns
			// STATUS_SUCCESS when full (see detourWrite()/KqEnque() in nodokad.c).
			// A burst of many keys injected without any wait can fill it faster than
			// kbdclass/the foreground app drains it, silently dropping a key (which,
			// if it is the &Sync marker, causes funcSync()'s 5s wait to time out with
			// no way to tell from the write call alone). Retry on short writes so a
			// momentarily full queue doesn't translate into a silently lost key.
			for (int retry = 0; retry < 50; ++retry)
			{
				WriteFile(m_device, i_kid, sizeof(*i_kid), &m_len, &m_writeOl);
				CHECK_TRUE(GetOverlappedResult(m_device, &m_writeOl, &m_len, TRUE));
				if (m_len >= sizeof(*i_kid))
					break;
				Sleep(2);
			}
			if (m_len < sizeof(*i_kid))
			{
				//Acquire a(&m_log, 0);
				//m_log << _T("generateKeyEvent: WriteFile short write, key may have been dropped (driver queue full)") << std::endl;
			}
			// Pace consecutive synthetic key writes in driver mode. nodokad delivers an
			// injected key by waking exactly one pending IRP_MJ_READ from kbdclass; once
			// woken, that slot is empty again until kbdclass re-issues its next read
			// (an asynchronous round trip through the OS). Back-to-back WriteFile calls
			// with no gap can outrun that round trip, leaving later keys queued with
			// nothing to wake them until the next real hardware key event arrives —
			// observed as a key (e.g. the &Sync marker) silently not showing up for
			// seconds. A short sleep here gives the round trip time to keep pace; this
			// was confirmed empirically (enabling verbose logging, which adds its own
			// per-key delay, made the same burst flow through with no drops).
			Sleep(1);
		}
		else
		{
			INPUT kid;

			kid.type = INPUT_KEYBOARD;
			kid.ki.wVk = 0;
			kid.ki.wScan = i_kid->MakeCode;
			kid.ki.dwFlags = KEYEVENTF_SCANCODE;
			kid.ki.time = 0;
			kid.ki.dwExtraInfo = 0;
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::BREAK)
			{
				kid.ki.dwFlags |= KEYEVENTF_KEYUP;
			}
			if (i_kid->Flags & KEYBOARD_INPUT_DATA::E0)
			{
				kid.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
			}

			if (m_keyboard_hook == 1)
			{
				// LL フックモード: 自己注入を識別するための魔法値を設定
				// uiAccess=true 時、自プロセスの uiAccess ウィンドウへの注入で
				// LLKHF_INJECTED が設定されないことがあるため、dwExtraInfo で識別する
				kid.ki.dwExtraInfo = NODOKA_SELF_INJECT_EXTRA_INFO;
			}
			else if (m_keyboard_hook == 2 && !(g_hookDataExe->m_RDP))
			{
				kid.ki.dwExtraInfo = 0x100; // rawinput hook do not loop
			}

			SendInput(1, &kid, sizeof(kid));
		}
	}
	return;
}

// pop all pressed key on win32
void Engine::keyboardResetOnWin32()
{
	for (Keyboard::KeyIterator
			 i = m_setting->m_keyboard.getKeyIterator();
		 *i; ++i)
	{
		if ((*i)->m_isPressedOnWin32)
			generateKeyEvent((*i), false, true);
	}
}

// Sync modifier key internal state with physical keyboard state.
// Clears m_isPressed for physical modifier keys that GetAsyncKeyState reports
// as released, fixing "stuck modifier" scenarios where key-up events were
// missed by the hook (e.g., during lock screen, UAC dialogs).
// Disabled by default (def option SyncModifierGracePeriod not set).
// When enabled, a grace period prevents false positives caused by the race between
// nodoka's key re-injection and Win32 key state update (LL hook and kernel driver modes).
void Engine::syncModifiersFromGetAsyncKeyState()
{
	if (!m_setting) return;
	if (m_setting->m_SyncModifierGracePeriod <= 0) return;

	const SHORT KEY_PRESSED_BIT = static_cast<SHORT>(0x8000);
	const DWORD GRACE_PERIOD_MS = static_cast<DWORD>(m_setting->m_SyncModifierGracePeriod);

	struct PhysicalModifier {
		USHORT scan;    // hardware scan code (m_scan)
		USHORT e0mask;  // ScanCode::E0 or 0
		int    vk;      // VK code for GetAsyncKeyState
	};

	static const PhysicalModifier physMods[] = {
		{ 0x2A, 0,             VK_LSHIFT   },  // L Shift
		{ 0x36, 0,             VK_RSHIFT   },  // R Shift
		{ 0x1D, 0,             VK_LCONTROL },  // L Ctrl
		{ 0x1D, ScanCode::E0,  VK_RCONTROL },  // R Ctrl
		{ 0x38, 0,             VK_LMENU    },  // L Alt
		{ 0x38, ScanCode::E0,  VK_RMENU    },  // R Alt
		{ 0x5B, ScanCode::E0,  VK_LWIN     },  // L Win
		{ 0x5C, ScanCode::E0,  VK_RWIN     },  // R Win
	};

	for (int pmIdx = 0; pmIdx < NUMBER_OF(physMods); ++pmIdx)
	{
		const auto &pm = physMods[pmIdx];
		bool physPressed = (GetAsyncKeyState(pm.vk) & KEY_PRESSED_BIT) != 0;

		// Find the key in nodoka's internal state
		Key *foundKey = nullptr;
		for (Keyboard::KeyIterator i = m_setting->m_keyboard.getKeyIterator(); *i; ++i)
		{
			Key *key = *i;
			if (!key->m_isPressed) continue;
			if (key->getScanCodesSize() != 1) continue;
			const ScanCode &sc = key->getScanCodes()[0];
			if (sc.m_scan == pm.scan &&
				(sc.m_flags & ScanCode::E0E1) == pm.e0mask)
			{
				foundKey = key;
				break;
			}
		}
		bool nodokaPressed = (foundKey != nullptr);

		// Detect not-pressed→pressed transition and record the time.
		// Used for the grace period below.
		if (!m_physModWasPressed[pmIdx] && nodokaPressed)
			m_physModFirstPressedTime[pmIdx] = timeGetTime();
		m_physModWasPressed[pmIdx] = nodokaPressed;

		// Only act when GetAsyncKeyState says released but nodoka says pressed
		if (physPressed || !nodokaPressed) continue;

		// Grace period: skip clearing if the modifier was pressed very recently.
		// After nodoka re-injects a key, Win32 key state may not update for
		// tens of milliseconds (async WriteFile in driver mode, or LL hook
		// callback latency).
		if (timeGetTime() - m_physModFirstPressedTime[pmIdx] < GRACE_PERIOD_MS) continue;

		// Genuinely stuck modifier — clear it
		{
			Acquire a(&m_log, 1);
			m_log << _T("syncModifiersFromGetAsyncKeyState: clearing stuck modifier, vk=")
				  << pm.vk << std::endl;
		}
		foundKey->m_isPressed = false;
		if (m_currentKeyPressCount > 0)
			--m_currentKeyPressCount;
	}
}

// Clear stuck modifiers after inactivity timeout.
// Called from CheckModifier thread under m_cs lock.
// No grace period needed: at ModifierAutoClear timeout (seconds), any injection
// race condition has long resolved.
//
// Pass 1: physical modifier keys (physMods table — 8 hardcoded scan codes).
//   Type B: if OS has modifier pressed AND nodoka has m_isPressedOnWin32, send Key-Up.
//   Type A: if OS has modifier released AND nodoka has m_isPressed, clear internal state.
//
// Pass 2: non-physical modifier keys remapped via "mod X += !Key" in keymaps.
//   For each basic modifier type, when the OS reports it fully released, clear m_isPressed
//   for any key that appears in a Keymap's effective modifier assignment for that type
//   (populated by Keymaps::adjustModifier at setting load time).
void Engine::modifierAutoClear()
{
	if (!m_setting) return;

	const SHORT KEY_PRESSED_BIT = static_cast<SHORT>(0x8000);

	// Physical modifier keys — scan code + VK pairs
	static const struct { USHORT scan; USHORT e0mask; int vk; } physMods[] = {
		{ 0x2A, 0,             VK_LSHIFT   },
		{ 0x36, 0,             VK_RSHIFT   },
		{ 0x1D, 0,             VK_LCONTROL },
		{ 0x1D, ScanCode::E0,  VK_RCONTROL },
		{ 0x38, 0,             VK_LMENU    },
		{ 0x38, ScanCode::E0,  VK_RMENU    },
		{ 0x5B, ScanCode::E0,  VK_LWIN     },
		{ 0x5C, ScanCode::E0,  VK_RWIN     },
	};

	// Returns true if the key's scan code matches any physMods entry.
	// Used in Pass 2 to avoid double-processing keys already handled in Pass 1.
	auto isPhysModKey = [&](const Key *key) -> bool {
		if (key->getScanCodesSize() != 1) return false;
		const ScanCode &sc = key->getScanCodes()[0];
		for (int i = 0; i < NUMBER_OF(physMods); ++i)
			if (sc.m_scan == physMods[i].scan &&
				(sc.m_flags & ScanCode::E0E1) == physMods[i].e0mask)
				return true;
		return false;
	};

	// --- Pass 1: physical modifier keys ---
	for (int pmIdx = 0; pmIdx < NUMBER_OF(physMods); ++pmIdx)
	{
		const auto &pm = physMods[pmIdx];
		bool physPressed = (GetAsyncKeyState(pm.vk) & KEY_PRESSED_BIT) != 0;

		Key *foundKey = nullptr;   // key with m_isPressed == true matching scan code
		Key *win32Key = nullptr;   // key with m_isPressedOnWin32 == true matching scan code
		for (Keyboard::KeyIterator i = m_setting->m_keyboard.getKeyIterator(); *i; ++i)
		{
			Key *key = *i;
			if (key->getScanCodesSize() != 1) continue;
			const ScanCode &sc = key->getScanCodes()[0];
			if (sc.m_scan != pm.scan || (sc.m_flags & ScanCode::E0E1) != pm.e0mask) continue;
			if (!foundKey && key->m_isPressed)        foundKey = key;
			if (!win32Key && key->m_isPressedOnWin32) win32Key = key;
		}

		if (physPressed)
		{
			// Type B: OS has modifier stuck — only act when nodoka injected the Key-Down
			if (!win32Key) continue;
			{
				Acquire a(&m_log, 1);
				m_log << _T("modifierAutoClear: sending Key-Up to OS (Type B), vk=") << pm.vk << std::endl;
			}
			generateKeyEvent(win32Key, false, false); // clears m_isPressedOnWin32, sends Key-Up
			if (foundKey)
			{
				foundKey->m_isPressed = false;
				if (m_currentKeyPressCount > 0) --m_currentKeyPressCount;
			}
		}
		else if (foundKey)
		{
			// Type A: OS released, nodoka still pressed — clear internal state only
			{
				Acquire a(&m_log, 1);
				m_log << _T("modifierAutoClear: clearing stuck modifier (Type A), vk=") << pm.vk << std::endl;
			}
			foundKey->m_isPressed = false;
			if (m_currentKeyPressCount > 0) --m_currentKeyPressCount;
		}
	}

	// --- Pass 2: non-physical modifier keys (e.g. CapsLock remapped as Ctrl) ---
	// For each basic modifier type, if the OS reports the modifier fully released,
	// clear m_isPressed for any key found in a keymap's effective modifier assignment.
	// Keymaps::adjustModifier() has already resolved add/sub/overwrite into the final lists.
	static const struct { Modifier::Type mt; int vk1; int vk2; } basicModGroups[] = {
		{ Modifier::Type_Shift,   VK_SHIFT,   0       },
		{ Modifier::Type_Control, VK_CONTROL, 0       },
		{ Modifier::Type_Alt,     VK_MENU,    0       },
		{ Modifier::Type_Windows, VK_LWIN,    VK_RWIN },
	};

	for (int miIdx = 0; miIdx < NUMBER_OF(basicModGroups); ++miIdx)
	{
		const auto &bm = basicModGroups[miIdx];

		bool physPressed = (GetAsyncKeyState(bm.vk1) & KEY_PRESSED_BIT) != 0;
		if (bm.vk2)
			physPressed = physPressed || ((GetAsyncKeyState(bm.vk2) & KEY_PRESSED_BIT) != 0);
		if (physPressed) continue; // OS still has this modifier type active

		// Collect candidate keys from all keymaps' effective modifier assignment lists.
		// Use a set to avoid processing the same key pointer twice across multiple keymaps.
		std::set<Key *> seen;

		for (Keymaps::const_iterator ki = m_setting->m_keymaps.begin();
			 ki != m_setting->m_keymaps.end(); ++ki)
		{
			const Keymap::ModAssignments &assigns = ki->getModAssignments(bm.mt);
			for (Keymap::ModAssignments::const_iterator mai = assigns.begin();
				 mai != assigns.end(); ++mai)
			{
				Key *key = mai->m_key;
				if (!key) continue;
				if (!key->m_isPressed) continue;
				if (isPhysModKey(key)) continue;   // already handled in Pass 1
				if (!seen.insert(key).second) continue; // duplicate across keymaps
				{
					Acquire a(&m_log, 1);
					m_log << _T("modifierAutoClear: clearing remapped modifier key, name=")
						  << key->getName() << std::endl;
				}
				key->m_isPressed = false;
				if (m_currentKeyPressCount > 0) --m_currentKeyPressCount;
			}
		}
	}
}

// Post to KeyboardHandler()
void Engine::SendtoKeyboardHandler(int mode, int flag, USHORT MakeCode, ULONG_PTR extraInfo)
{
	if (mode == 0)
	{
		PostThreadMessage(m_threadId, WM_APP + 203, (WPARAM)flag, (LPARAM)MakeCode);
	}
	else
	{
		WaitForSingleObject(m_queueMutex, INFINITE);

		KEYBOARD_INPUT_DATA kid;
		kid.UnitId = (USHORT)0;
		kid.MakeCode = MakeCode;
		kid.Flags = flag;
		kid.Reserved = (USHORT)0;
		kid.ExtraInformation = (ULONG)extraInfo; // ULONG_PTR → ULONG (upper 32 bits used only by kbdaddid)

		m_inputQueue->push_back(kid);
		SetEvent(m_readEvent); // keyboardHandler を即座に起床させる
		ReleaseMutex(m_queueMutex);
	}
}

unsigned int WINAPI Engine::keyboardDetour(Engine *i_this, WPARAM i_wParam, LPARAM i_lParam)
{
	return i_this->keyboardDetour(reinterpret_cast<KBDLLHOOKSTRUCT *>(i_lParam));
}

unsigned int Engine::keyboardDetour(KBDLLHOOKSTRUCT *i_kid)
{
	if (!m_isEnabled || m_keyboard_hook == 0)
	{
		NODOKA_TRACE(_T("keyboardDetour: disabled or device mode, returning 0\n"));
		return 0;
	}
	else
	{
		KEYBOARD_INPUT_DATA kid;

		kid.UnitId = 0;
		kid.MakeCode = (USHORT)(i_kid->scanCode & 0xFFFF);
		kid.Flags = 0;
		if (i_kid->flags & LLKHF_UP)
		{
			kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
		}
		if (i_kid->flags & LLKHF_EXTENDED)
		{
			kid.Flags |= KEYBOARD_INPUT_DATA::E0;
		}

		kid.Reserved = 0;
		kid.ExtraInformation = 0;

		NODOKA_TRACE(_T("keyboardDetour: sc=0x%x flags=0x%x, queueing\n"), kid.MakeCode, kid.Flags);

		// kbdaddid の ExtraInfo を keyboardHandler まで引き渡す
		SendtoKeyboardHandler(1, kid.Flags, kid.MakeCode, i_kid->dwExtraInfo);

		return 1;
	}
}

unsigned int WINAPI Engine::mouseDetour(Engine *i_this, WPARAM i_wParam, LPARAM i_lParam)
{
	return i_this->mouseDetour(i_wParam, reinterpret_cast<MSLLHOOKSTRUCT *>(i_lParam));
}

unsigned int Engine::mouseDetour(WPARAM i_message, MSLLHOOKSTRUCT *i_mid)
{
	if (!m_isEnabled || !m_setting || !m_setting->m_mouseEvent)
	{
		return 0;
	}
	else
	{
		KEYBOARD_INPUT_DATA kid;

		kid.UnitId = (USHORT)0;
		kid.Flags = KEYBOARD_INPUT_DATA::E1;
		kid.Reserved = (USHORT)0;
		kid.ExtraInformation = (ULONG)0;
		switch (i_message)
		{
		case WM_LBUTTONUP:
			kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
		case WM_LBUTTONDOWN:
			kid.MakeCode = (USHORT)1;
			break;
		case WM_RBUTTONUP:
			kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
		case WM_RBUTTONDOWN:
			kid.MakeCode = (USHORT)2;
			break;
		case WM_MBUTTONUP:
			kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
		case WM_MBUTTONDOWN:
			kid.MakeCode = (USHORT)3;
			break;
		case WM_MOUSEWHEEL:
			if (i_mid->mouseData & (1 << 31))
			{
				kid.MakeCode = (USHORT)5;
			}
			else
			{
				kid.MakeCode = (USHORT)4;
			}
			break;
		case WM_XBUTTONUP:
			kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
		case WM_XBUTTONDOWN:
			switch ((i_mid->mouseData >> 16) & 0xFFFFU)
			{
			case XBUTTON1:
				kid.MakeCode = (USHORT)6;
				break;
			case XBUTTON2:
				kid.MakeCode = (USHORT)7;
				break;
			default:
				return 0;
				break;
			}
			break;
		case WM_MOUSEHWHEEL:
			if (i_mid->mouseData & (1 << 31))
			{
				kid.MakeCode = (USHORT)9;
			}
			else
			{
				kid.MakeCode = (USHORT)8;
			}
			break;
		case WM_MOUSEMOVE:
		{
			LONG dx = i_mid->pt.x - g_hookDataExe->m_mousePos.x;
			LONG dy = i_mid->pt.y - g_hookDataExe->m_mousePos.y;
			HWND target = reinterpret_cast<HWND>((UINT_PTR)g_hookDataExe->m_hwndMouseHookTarget);

			LONG dr = 0;
			dr += (i_mid->pt.x - m_msllHookCurrent.pt.x) * (i_mid->pt.x - m_msllHookCurrent.pt.x);
			dr += (i_mid->pt.y - m_msllHookCurrent.pt.y) * (i_mid->pt.y - m_msllHookCurrent.pt.y);
			if (m_buttonPressed && !m_dragging && m_setting->m_dragThreshold &&
				(m_setting->m_dragThreshold * m_setting->m_dragThreshold < dr))
			{
				kid.MakeCode = (USHORT)0;
				m_dragging = true;
				WaitForSingleObject(m_queueMutex, INFINITE);
				m_inputQueue->push_back(kid);
				ReleaseMutex(m_queueMutex);
			}

			switch (g_hookDataExe->m_mouseHookType)
			{
			case MouseHookType_Wheel:
				// For this type, g_hookDataExe->m_mouseHookParam means
				// translate rate mouse move to wheel.
				mouse_event(MOUSEEVENTF_WHEEL, 0, 0,
							g_hookDataExe->m_mouseHookParam * dy, 0);
				return 1;
				break;
			case MouseHookType_WindowMove:
			{
				RECT curRect;

				if (!GetWindowRect(target, &curRect))
					return 0;

				// g_hookDataExe->m_mouseHookParam < 0 means
				// target window to move is MDI.
				if (g_hookDataExe->m_mouseHookParam < 0)
				{
					HWND parent = GetParent(target);
					POINT p = {curRect.left, curRect.top};

					if (parent == NULL || !ScreenToClient(parent, &p))
						return 0;

					curRect.left = p.x;
					curRect.top = p.y;
				}

				SetWindowPos(target, NULL,
							 curRect.left + dx,
							 curRect.top + dy,
							 0, 0,
							 SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE |
								 SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER);
				g_hookDataExe->m_mousePos = i_mid->pt;
				return 0;
				break;
			}
			case MouseHookType_None:
			default:
				return 0;
				break;
			}
		}
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_XBUTTONDBLCLK:
		default:
			return 0;
			break;
		}

		WaitForSingleObject(m_queueMutex, INFINITE);

		if (kid.Flags & KEYBOARD_INPUT_DATA::BREAK)
		{
			m_buttonPressed = false;
			if (m_dragging)
			{
				KEYBOARD_INPUT_DATA kid2;

				m_dragging = false;
				kid2.UnitId = (USHORT)0;
				kid2.Flags = KEYBOARD_INPUT_DATA::E1 | KEYBOARD_INPUT_DATA::BREAK;
				kid2.Reserved = (USHORT)0;
				kid2.ExtraInformation = (ULONG)0;
				kid2.MakeCode = 0;
				m_inputQueue->push_back(kid2);
			}
		}
		else if (i_message != WM_MOUSEWHEEL && i_message != WM_MOUSEHWHEEL)
		{
			m_buttonPressed = true;
			m_msllHookCurrent = *i_mid;
		}

		m_inputQueue->push_back(kid);

		if (i_message == WM_MOUSEWHEEL || i_message == WM_MOUSEHWHEEL)
		{
			kid.UnitId = 0;
			kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
			kid.Reserved = (USHORT)0;
			kid.ExtraInformation = (ULONG)0;
			m_inputQueue->push_back(kid);
		}
		ReleaseMutex(m_queueMutex);
		return 1;
	}
}

// Push a list of buffered KEYBOARD_INPUT_DATA events to the front of m_inputQueue
// (in original order, so the first event in the vector is the first to be processed).
void Engine::reInjectKeys(const std::vector<KEYBOARD_INPUT_DATA> &i_events)
{
	if (i_events.empty()) return;
	WaitForSingleObject(m_queueMutex, INFINITE);
	for (int i = (int)i_events.size() - 1; i >= 0; --i)
	{
		KEYBOARD_INPUT_DATA tagged = i_events[i];
		tagged.Reserved = 2;  // mark as SM re-injected; detected at quit_loop
		m_inputQueue->push_front(tagged);
	}
	SetEvent(m_reInjectEvent);  // dedicated event; works in both LL-Hook and driver mode
	ReleaseMutex(m_queueMutex);
}

// Return true if i_key appears in any ComboRule of the current focus keymaps
// whose modifier condition matches the current modifier state.
bool Engine::isComboCandidate(const Key *i_key)
{
	if (!m_currentFocusOfThread) return false;
	// Build current modifier state (K0-K7 from m_currentLock, S/C/A/W from physical keys, etc.)
	Modifier curMod = getCurrentModifiers(const_cast<Key *>(i_key), true);
	curMod.dontcare(Modifier::Type_Up);
	curMod.dontcare(Modifier::Type_Down);
	curMod.dontcare(Modifier::Type_Repeat);

	for (const Keymap *km : m_currentFocusOfThread->m_keymaps)
		for (const auto &rule : km->getComboRules())
			for (const Key *k : rule.m_keys)
				if (k == i_key && rule.m_modifier.doesMatch(curMod))
					return true;
	return false;
}

// keyboard handler thread
unsigned int WINAPI Engine::keyboardHandler(void *i_this)
{
	reinterpret_cast<Engine *>(i_this)->keyboardHandler();
	_endthreadex(0);
	return 0;
}
void Engine::keyboardHandler()
{
	//SYSTEMTIME st;
	MSG message;
	Key key;
	KEYBOARD_INPUT_DATA kid;
	DWORD len;
	HANDLE handles[] = {m_readEvent, m_interruptThreadEvent,
	                    m_tapHoldExpiredEvent, m_tapDanceExpiredEvent, m_comboExpiredEvent,
	                    m_reInjectEvent};

	// initialize ok
	CHECK_TRUE(SetEvent(m_threadEvent));
	PeekMessage(&message, NULL, 0, 0, PM_NOREMOVE); // メッセージキューの生成

	// combo/tapdance re-injection marker (set from kid.Reserved == 2 at quit_loop)
	bool isSmReinjected = false;

	// repeat用
	bool volatile bRepeat = false;
	u_int8 volatile Repeat_e0e1;
	USHORT volatile Repeat_flags;
	USHORT volatile Repeat_key;
	USHORT volatile Repeat_UnitId;
	bool volatile bbi = true;
	DWORD volatile dwWaitTime;
	DWORD volatile currentTime;
	DWORD dwSystemDelay031;
	DWORD volatile dwSystemDelay;
	DWORD volatile dwRepeat_time;

	if (SystemParametersInfo(SPI_GETKEYBOARDSPEED, NULL, &dwSystemDelay031, 0))
	{
		dwSystemDelay = 2 * (10 * 1000) / ((dwSystemDelay031 * 10) + 25); // 0～31 to 1/2.5 * 2
	}
	else
	{
		dwSystemDelay = 0;
	}

	NODOKA_TRACE(_T("keyboardHandler: loop start\n"));

	// loop
	while (!m_doForceTerminate)
	{
	start_loop:
		// for LL Hook, raw input Hook,  Mouse Hook
		// m_setting が NULL でも LL/RawInput/Mouse hook のキューは処理する（NULLの場合は quit_loop でパススルーされる）
		if ((m_keyboard_hook == 1) || (m_keyboard_hook == 2) || (m_mouse_hook == 1) || (m_setting && m_isEnabled && m_setting->m_CheckModifier) || (m_setting && m_isEnabled && m_setting->m_For6point))
			{
				// m_queueMutexを確認し、m_inputQueueからデータを取り出す。
				switch (WaitForSingleObject(m_queueMutex, 5))
				{
				case WAIT_OBJECT_0:
					if ((m_inputQueue != NULL) && !(m_inputQueue->empty()))
					{
						kid = m_inputQueue->front();
						m_inputQueue->pop_front();
						if (m_inputQueue->empty())
						{
							if (bbi)
								ResetEvent(m_readEvent);
						}
						ReleaseMutex(m_queueMutex);
						goto quit_loop;
					}
					if (bbi)
						ResetEvent(m_readEvent);
					ReleaseMutex(m_queueMutex); // 取得成功時のみ解放
					break;
				default:
					break;
				}
				// タイムアウト時は ReleaseMutex を呼ばない（未所有のMutexの解放を防ぐ）
			}

#ifndef KBDADDID_DEBUG_MINIMAL
		// Focusチェンジしていたら、E1-0x1f upを送る。
		if (m_setting && m_isEnabled)
			if (m_setting->m_FocusChange)
			{
				if (checkFocusWindow())
				{
					kid.UnitId = (USHORT)0;
					kid.MakeCode = 0x1f;
					kid.Flags = KEYBOARD_INPUT_DATA::E1;
					kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
					kid.Reserved = (USHORT)0;
					kid.ExtraInformation = (USHORT)0;
					goto quit_loop;
				}
			}

		// Device Driverからのキーボード情報よりも先に、Gamepad他を確認。
		if (m_setting && m_isEnabled)
			if (PeekMessage(&message, NULL, 0, 0, PM_NOREMOVE))
			{
				if (message.message == WM_APP + 201) //タッチパッド
				{
					PeekMessage(&message, NULL, 0, 0, PM_REMOVE);
					if (message.wParam)
					{
						m_currentLock.on(Modifier::Type_Touchpad);
						m_currentLock.on(Modifier::Type_TouchpadSticky);
						if ((unsigned int)(message.lParam & 0xffff) > (unsigned int)m_setting->m_CenterVal)
						{
							m_currentLock.on(Modifier::Type_TouchpadR);
							m_currentLock.on(Modifier::Type_TouchpadRSticky);
						}
						else
						{
							m_currentLock.on(Modifier::Type_TouchpadL);
							m_currentLock.on(Modifier::Type_TouchpadLSticky);
						}
					}
					else
					{
						m_currentLock.off(Modifier::Type_Touchpad);
						if ((unsigned int)(message.lParam & 0xffff) > (unsigned int)m_setting->m_CenterVal)
							m_currentLock.off(Modifier::Type_TouchpadR);
						else
							m_currentLock.off(Modifier::Type_TouchpadL);
					}
					Acquire a(&m_log, 1);
					m_log << _T("TouchPad: Z:") << message.wParam
						  << _T(" X:") << (message.lParam & 0xffff)
						  << _T(" Y:") << (message.lParam >> 16 & 0xffff)
						  << std::endl;
				}

				if (message.message == WM_APP + 202) // ゲームパッド
				{
					PeekMessage(&message, NULL, 0, 0, PM_REMOVE);
					if (message.wParam == 1) // make
					{
						kid.UnitId = (USHORT)0;
						kid.MakeCode = (USHORT)message.lParam;
						kid.Flags = KEYBOARD_INPUT_DATA::E1;
						kid.Reserved = (USHORT)0;
						kid.ExtraInformation = (ULONG)0;
					}
					else // break
					{
						kid.UnitId = (USHORT)0;
						kid.MakeCode = (USHORT)message.lParam;
						kid.Flags = KEYBOARD_INPUT_DATA::E1;
						kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
						kid.Reserved = (USHORT)0;
						kid.ExtraInformation = (ULONG)0;
					}
					m_log << _T("Gamepad: ") << message.wParam
						  << _T(" Button:") << (message.lParam & 0xffff)
						  << std::endl;
					goto quit_loop; // キー入力扱いなので抜ける。
				}

				if (message.message == WM_APP + 203)
				{
					PeekMessage(&message, NULL, 0, 0, PM_REMOVE);
					kid.UnitId = (USHORT)0;
					kid.Flags = (USHORT)message.wParam;
					kid.MakeCode = (USHORT)message.lParam;
					m_log << _T("K Mouse: ") << message.wParam
						  << _T(" key: ") << (message.lParam & 0xffff)
						  << std::endl;
					goto quit_loop;
				}
			} // messageで飛んでくるもの確認終了。
#endif // !KBDADDID_DEBUG_MINIMAL

		if (bbi == false || m_keyboard_hook != 0)
		{ // ReadFile()の完了がまだ、あるいは LL Hook mode/RawInput hookならばReadFile実行を抑止する。
			goto rewait;
		}

		if (!ReadFile(m_device, &kid, sizeof(kid), &len, &m_ol))
		{
			// PENDING以外のエラーは、ループの先頭に戻って、再度 ReadFileを実行できるようにする。
			if (GetLastError() != ERROR_IO_PENDING)
			{
				continue; // 外側のwhile loop
			}			  // if (GetLastError()
						  // ReadFile()がPENDINGだったのでシグナル等を処理する。
		}
		else
		{ // ReadFile()が成功したので抜ける。
			goto quit_loop;
		}
	rewait:
		dwWaitTime = 5;
		if (m_doForceTerminate) { NODOKA_TRACE(_T("keyboardHandler: m_doForceTerminate at rewait, exiting\n")); goto break_while; }
		//OutputDebugString(_T("keyboardHandler: entering MsgWaitForMultipleObjects\n"));
		switch (MsgWaitForMultipleObjects(NUMBER_OF(handles), &handles[0], FALSE, dwWaitTime, QS_POSTMESSAGE))
		{
		case WAIT_TIMEOUT: // timeoutしたのでcontinueして戻る。
			bbi = false;   // ReadFile()は完了していないので、次回 ReadFile()を実行させない。
#ifndef KBDADDID_DEBUG_MINIMAL
			// delayMAXチェック
			currentTime = timeGetTime();
			if (bRepeat == true)
			{
				if (((m_setting->m_DelayMaxFlag) && (getKeyStateTime(Repeat_e0e1, Repeat_key, Repeat_UnitId, 0) + m_setting->m_DelayMax < currentTime)) ||
					// delay-MAXに達した。
					((dwRepeat_time != 0) && (dwRepeat_time < currentTime)))
				// 以前に物理的にキーダウンが来てからシステム設定値*2を超えてダウンが来ていない。
				{
					Acquire a(&m_log, 1);
					m_log << _T("Repeat Cancel !!!") << std::endl;

					setKeyState(Repeat_e0e1, Repeat_key, Repeat_UnitId, 0);
					setKeyStateTime(Repeat_e0e1, Repeat_key, Repeat_UnitId, 0, 0); // st1
					setKeyStateTime(Repeat_e0e1, Repeat_key, Repeat_UnitId, 1, 0); // st2
					dwRepeat_time = 0;
					kid.UnitId = Repeat_UnitId;
					kid.MakeCode = Repeat_key;
					kid.Flags = Repeat_flags;
					kid.Flags |= KEYBOARD_INPUT_DATA::BREAK; // 強制的にupにする。
					kid.Reserved = (USHORT)0;
					kid.ExtraInformation = (USHORT)0;
					bRepeat = false;
					Repeat_e0e1 = 0;
					Repeat_flags = 0;
					Repeat_key = 0;
					Repeat_UnitId = 0;
					goto quit_loop;
				}
			}

			// Repeat処理
			if (bRepeat == true)
			{
				kid.UnitId = Repeat_UnitId;
				kid.MakeCode = Repeat_key;
				kid.Flags = Repeat_flags;
				kid.Reserved = (USHORT)1; // 挿入したものであることを示す。
				kid.ExtraInformation = (USHORT)0;
				goto quit_loop;
			}
			else
#endif // !KBDADDID_DEBUG_MINIMAL
			{
				//bbi = true;		// ここで trueにしてはいけない。
				continue; //goto rewait;		// Repeat不要だったので、Eventを待つ。
			}
			break;
		case WAIT_OBJECT_0: // m_readEvent
			if (m_keyboard_hook != 0)
			{
				// LL フックモード：キューにデータあり → start_loop でキューを処理
				bbi = false;
				goto start_loop;
			}
			// デバイスドライバモード：overlapped I/O 完了
			bbi = true;		// ReadFile()を実行させる。
			if (!GetOverlappedResult(m_device, &m_ol, &len, FALSE))
			{
				if (GetLastError() == ERROR_IO_INCOMPLETE)
				{
				// ReadFile はまだ pending — CheckModifier/FocusChange 等の偽シグナル。
				// bbi を false に戻して ReadFile を二重発行しない。
				bbi = false;
				}
				// ERROR_OPERATION_ABORTED（FUS/RDP disconnect による IRP キャンセル）は
				// bbi = true のまま維持し、次ループで ReadFile を再発行する。
				continue;
			}
			break;		  // switch/case break;

		case WAIT_OBJECT_0 + 1: // m_interruptThreadEvent
			// Do NOT call CancelIo here.
			// CancelIo blocks during FUS because the driver's cancel routine
			// (nodokaDetourReadCancel) spins on devExt->lock while
			// NodokaSessionNotificationCallback holds it at the same time.
			// Instead, pause() calls close() after receiving the Pause ack,
			// and detourCleanup cancels the pending IRP via CloseHandle.
			// The stale overlapped event fires later and is consumed harmlessly
			// as an ABORTED result in the WAIT_OBJECT_0 case.
			SESSTRACE(m_log, _T("outer WAIT_OBJECT_0+1 reason=") << m_interruptThreadReason);
			switch (m_interruptThreadReason)
			{
			default:
			{
				SESSTRACE(m_log, _T("outer: unexpected reason=") << m_interruptThreadReason);
				ASSERT(false);
				break;
			}

			case InterruptThreadReason_Terminate:
				goto break_while;

			case InterruptThreadReason_Resume:
				// Resume received in the outer loop.
				// This happens when close() cancels the pending ReadFile IRP,
				// which fires the overlapped event and wakes the INNER pause-wait
				// (spurious wakeup). The keyboard handler exits the Pause handler
				// early and reaches here when resume() sends the Resume signal.
				// Just ack so resume() can exit its do-loop and release the mutex.
				SESSTRACE(m_log, _T("outer: Resume received (safety net), acking"));
				CHECK_TRUE(SetEvent(m_threadEvent));
				break;

			case InterruptThreadReason_Pause:
			{
				SESSTRACE(m_log, _T("outer: Pause received, acking and entering INNER wait loop"));
				CHECK_TRUE(SetEvent(m_threadEvent));
				// Loop until we get Resume (or Terminate).
				// A spurious wakeup can occur when close() cancels the pending
				// ReadFile IRP: if m_interruptThreadEvent shares its handle with
				// the overlapped event, the IRP cancellation signals it.
				// Re-waiting keeps the handler inside the Pause handler until
				// the real Resume signal arrives.
				bool doneWaiting = false;
				while (!doneWaiting)
				{
					while (WaitForMultipleObjects(1, &m_interruptThreadEvent, FALSE, INFINITE) != WAIT_OBJECT_0)
						;
					SESSTRACE(m_log, _T("INNER wait woke, reason=") << m_interruptThreadReason);
					switch (m_interruptThreadReason)
					{
					case InterruptThreadReason_Terminate:
						goto break_while;

					case InterruptThreadReason_Resume:
						doneWaiting = true;
						break;

					default:
						// Spurious wakeup (e.g., IRP cancellation signalled the
						// shared overlapped/interrupt event), OR another Pause
						// request arrived while already inside this INNER loop
						// (e.g. a second pause() call before the matching resume()
						// -- see connect()/disconnect() call-count in nodoka.cpp).
						// Either way this thread does NOT ack m_threadEvent here,
						// so a caller waiting on a second pause() will spin in its
						// do-while(WaitForSingleObject(...) != WAIT_OBJECT_0) loop
						// forever, blocking the UI thread. If this trace repeats
						// with reason=InterruptThreadReason_Pause, that is the bug.
						SESSTRACE(m_log, _T("INNER: spurious/unmatched reason=") << m_interruptThreadReason << _T(", re-waiting"));
						break;
					}
				}
				SESSTRACE(m_log, _T("INNER: Resume received, exiting INNER loop and acking"));
				CHECK_TRUE(SetEvent(m_threadEvent));
				break;
			}
			}
			break;
		case WAIT_OBJECT_0 + 2: // m_tapHoldExpiredEvent: threshold elapsed
			if (m_tapHoldState == TH_PENDING && m_tapHoldCurrentRule)
			{
				m_tapHoldState = TH_HOLDING;
				generateKeySeqEvents(m_tapHoldContext, m_tapHoldCurrentRule->m_holdAction, Part_all);
			}
			continue;

		case WAIT_OBJECT_0 + 3: // m_tapDanceExpiredEvent: tap-dance timeout
			if (m_tapDanceState == TD_COUNTING && m_tapDanceCurrentRule)
			{
				m_tapDanceState = TD_IDLE;
				int tapIdx = m_tapDanceCount - 1; // 0=tap1, 1=tap2, 2=tap3
				if (tapIdx >= 0 && tapIdx < 3 && m_tapDanceCurrentRule->m_tap[tapIdx])
				{
					generateKeySeqEvents(m_tapDanceContext, m_tapDanceCurrentRule->m_tap[tapIdx], Part_all);
				}
				else
				{
					// No action for this tap count — replay original events
					reInjectKeys(m_tapDanceBuffered);
				}
				m_tapDanceCurrentRule = NULL;
				m_tapDanceBuffered.clear();
			}
			continue;

		case WAIT_OBJECT_0 + 4: // m_comboExpiredEvent: combo window elapsed
			if (m_comboState == CO_PENDING)
			{
				// immediate / rollover: no timer was started, so this event should not fire;
				// ignore it defensively.
				if (m_comboDetectorMode == Setting::CD_IMMEDIATE || m_comboDetectorMode == Setting::CD_ROLLOVER)
				{
					// nothing to do
				}
				else
				{
					// ComboNestedAlwaysMatch: if first key is still physically held, skip timeout
					bool nested = m_comboNestedAlwaysMatchActive;
					if (nested && !m_comboPressedKeys.empty())
					{
						Key *firstKey = m_comboPressedKeys.front();
						if (firstKey && firstKey->m_isPressed)
							goto combo_expired_skip; // keep CO_PENDING, wait for key2
					}

					// timeout / strict-order / zero-latency: check for an exact match on keys pressed so far
					const ComboRule *matchedRule = NULL;
					if (m_currentFocusOfThread)
					{
						for (const Keymap *km : m_currentFocusOfThread->m_keymaps)
						{
							if (m_comboDetectorMode == Setting::CD_STRICT_ORDER)
								matchedRule = km->searchComboOrdered(m_comboPressedKeys, m_comboModifier);
							else
								matchedRule = km->searchCombo(m_comboPressedKeys, m_comboModifier);
							if (matchedRule) break;
						}
					}
					m_comboState = CO_IDLE;
					if (matchedRule)
					{
						if (m_comboZeroLatencyActive)
						{
							// key1 was already output; send Backspace to undo it, then fire combo
							keybd_event(VK_BACK, 0, 0, 0);
							keybd_event(VK_BACK, 0, KEYEVENTF_KEYUP, 0);
						}
						generateKeySeqEvents(m_comboContext, matchedRule->m_action, Part_all);
					}
					else
					{
						if (!m_comboZeroLatencyActive)
							reInjectKeys(m_comboBuffered);
						// zero-latency: key1 already output — nothing to re-inject
					}
					m_comboZeroLatencyActive = false;
					m_comboPressedKeys.clear();
					m_comboBuffered.clear();
					m_comboKeyDownTimes.clear();
					m_comboAllKeysDown = false;
				}
			}
			combo_expired_skip:
			continue;

		case WAIT_OBJECT_0 + 5: // m_reInjectEvent: re-injected keys waiting in m_inputQueue
		{
			// Works in both LL-Hook and driver mode (unlike m_readEvent which is shared
			// with the OVERLAPPED IO handle and cannot be used for manual signaling in driver mode).
			WaitForSingleObject(m_queueMutex, INFINITE);
			if (m_inputQueue != NULL && !m_inputQueue->empty())
			{
				kid = m_inputQueue->front();
				m_inputQueue->pop_front();
				if (!m_inputQueue->empty())
					SetEvent(m_reInjectEvent); // more items remain — re-signal for next iteration
				ReleaseMutex(m_queueMutex);
				goto quit_loop;
			}
			ReleaseMutex(m_queueMutex);
			continue;
		}

		default:
			ASSERT(false);
			continue;
		} // switch(MsgWaitForMultipleObjects)

		// ここでMouse Hookのkidが来ていたら、2回目を処理してしまうので戻す。
		if (m_setting)
			if (m_mouse_hook == 1)
				if ((kid.Flags & KEYBOARD_INPUT_DATA::E1) && (kid.MakeCode < 0x0A))
				{
					goto start_loop;
				}

	quit_loop:
		// Detect the re-injection marker set by reInjectKeys.
		// Do NOT clear Reserved yet — kbdaddid block below needs it to guard against
		// re-injected events being misclassified as K0.
		isSmReinjected = (kid.Reserved == 2);

		// kbdaddid: LL hookモード・nodokadモード両方で ExtraInformation に DeviceId が入る。
		// kbdaddid の ClassService フックがkbdclassバッファへの書き込み前に ExtraInfo を設定するため。
		if (m_setting && g_hookDataExe && g_hookDataExe->m_UseKbdAddId == 1)
		{
			ULONG kbdId = kid.ExtraInformation >> 16;
#ifdef DEBUG
			m_log << _T("quit_loop:  ExtraInfo 0x") << std::hex << kid.ExtraInformation
			      << _T(" Reserved=") << kid.Reserved
			      << _T(" Flags=0x") << kid.Flags << std::dec << std::endl;
#endif
			if (kbdId != 0)
			{
				// External keyboard: kbdaddid set ExtraInfo to the DeviceId.
				UINT UnitID = 0;
				bool found = false;
				for (int k = 0; k < 8; k++)
				{
					if ((g_hookDataExe->m_kbdAddId_extraInfo[k] >> 16) != 0 &&
						(g_hookDataExe->m_kbdAddId_extraInfo[k] >> 16) == kbdId)
					{
						UnitID = g_hookDataExe->m_kbdAddId_unitId[k];
						found = true;
						break;
					}
				}
				if (!found)
				{
					// Keyboard has a kbdaddid DeviceId but is not mapped to any Kx slot.
					Acquire a(&m_log, 1);
					m_log << _T("kbdaddid: unregistered hi=0x")
						<< std::hex << std::uppercase << kbdId << std::dec
						<< _T(". Add to .nodoka: def option UnitID = Kx ExtraInfo 0x")
						<< std::hex << std::uppercase << (kbdId << 16) << std::dec
						<< std::endl;
				}
				setLockState3(UnitID);
			}
			else if (!isSmReinjected
			         && kid.Reserved == 0
			         && !(kid.Flags & KEYBOARD_INPUT_DATA::E1))
			{
				// ExtraInfo==0 かつ実キーボードイベント (リピート/合成イベントを除く) → K0
				// ・kid.Reserved==1: nodoka内部リピート処理 → K状態を変えない
				// ・kid.Flags & E1: ゲームパッド/マウスキー/フォーカス変更等の合成イベント → K状態を変えない
				// ・isSmReinjected: SM再注入イベント → UnitIDを変えない (K1–K7が K0 になるバグ防止)
				setLockState3(0);
			}
		}

		// Clear the re-injection marker after kbdaddid has read Reserved.
		if (isSmReinjected) kid.Reserved = 0;

		if (m_setting)
			if (m_setting->m_Repeat && m_isEnabled)
			{ // Key State 処理

				// 一つキー入力があったので、時刻を stに保存。
				DWORD dwTime = timeGetTime();
				// 保存してあるkey stateと比較し、必要なら、再びキー入力へ戻る。
				u_int8 e0e1_flag = 0;
				int iBreak = 0;

				if (kid.Flags & KEYBOARD_INPUT_DATA::E0)
					e0e1_flag = 1;
				if (kid.Flags & KEYBOARD_INPUT_DATA::E1)
					e0e1_flag = 2;
				if (kid.Flags & KEYBOARD_INPUT_DATA::E0E1)
					e0e1_flag = 3;
				if (kid.Flags & KEYBOARD_INPUT_DATA::BREAK)
					iBreak = 1;

				int old_state = getKeyState(e0e1_flag, kid.MakeCode, kid.UnitId);

				// Re-injected key-DOWN: reset to state 0 so it's treated as a first press,
				// not as an OS auto-repeat that arrived before delay-A.
				if (isSmReinjected && iBreak == 0)
				{
					setKeyState(e0e1_flag, kid.MakeCode, kid.UnitId, 0);
					old_state = 0;
				}

				// state 0: first, 1:前はdownだった。2: repeat処理
				switch (old_state)
				{
				case 0: // as first
					if (iBreak == 0)
					{ // downが初めてきたので、保存して、key down を送る。
						if (bRepeat)
						{ // もしRepeat中ならキャンセル。
							bRepeat = false;
							setKeyState(Repeat_e0e1, Repeat_key, Repeat_UnitId, 0);
							Repeat_e0e1 = 0;
							Repeat_flags = 0;
							Repeat_key = 0;
							Repeat_UnitId = 0;
							dwRepeat_time = 0;
						}

						setKeyState(e0e1_flag, kid.MakeCode, kid.UnitId, 1);
						setKeyStateTime(e0e1_flag, kid.MakeCode, kid.UnitId, 0, dwTime); // st1
						setKeyStateTime(e0e1_flag, kid.MakeCode, kid.UnitId, 1, dwTime); // st2
						dwRepeat_time = 0;
						break;
					}
					else
					{ // upが来たので、初期化し、キーは送る。
						setKeyState(e0e1_flag, kid.MakeCode, kid.UnitId, 0);
						setKeyStateTime(e0e1_flag, kid.MakeCode, kid.UnitId, 0, 0); // st1
						setKeyStateTime(e0e1_flag, kid.MakeCode, kid.UnitId, 1, 0); // st2
						dwRepeat_time = 0;
						break;
					}

				case 1: // 前はdownだった。

					if (iBreak == 0)
					{ // down
						if (bRepeat)
						{ // もしRepeat中ならキャンセル。
							bRepeat = false;
							setKeyState(Repeat_e0e1, Repeat_key, Repeat_UnitId, 0);
							Repeat_e0e1 = 0;
							Repeat_flags = 0;
							Repeat_key = 0;
							Repeat_UnitId = 0;
							dwRepeat_time = 0;
						}
						if (getKeyStateTime(e0e1_flag, kid.MakeCode, kid.UnitId, 0) + getKeyStateDelay(e0e1_flag, kid.MakeCode, kid.UnitId, 0) > dwTime)
						{					 // delay-Aに達していない。
							goto start_loop; // next
						}
						else
						{																	 // delay-Aに達したので、keyを送る。時刻はst2に保存。flagはrepeatへ。
							setKeyState(e0e1_flag, kid.MakeCode, kid.UnitId, 2);			 // repeat
							setKeyStateTime(e0e1_flag, kid.MakeCode, kid.UnitId, 1, dwTime); // st2
							bRepeat = true;
							Repeat_e0e1 = e0e1_flag;
							Repeat_flags = kid.Flags;
							Repeat_key = kid.MakeCode;
							Repeat_UnitId = kid.UnitId;
							dwRepeat_time = 0;
							break;
						}
					}
					else
					{ // up, keyを送る。時刻は初期化。flagは0へ。
						setKeyState(e0e1_flag, kid.MakeCode, kid.UnitId, 0);
						setKeyStateTime(e0e1_flag, kid.MakeCode, kid.UnitId, 0, 0); // st1
						setKeyStateTime(e0e1_flag, kid.MakeCode, kid.UnitId, 1, 0); // st2
						bRepeat = false;
						Repeat_e0e1 = 0;
						Repeat_flags = 0;
						Repeat_key = 0;
						Repeat_UnitId = 0;
						dwRepeat_time = 0;
						break;
					}

				case 2: // repeatに入っている。

					if (iBreak == 0)
					{ // down
						// 物理的にキーダウンが来た場合、有効期間を保存し、リピート抑止に用いる。
						if (kid.Reserved == 0)
						{
							if (dwSystemDelay != 0)
							{
								dwRepeat_time = dwTime + dwSystemDelay;
							}
							else
							{
								dwRepeat_time = 0;
							}
							kid.Reserved = (USHORT)0; // Reservedを元に戻す。
						}
						// delay-Bチェック
						if (getKeyStateTime(e0e1_flag, kid.MakeCode, kid.UnitId, 1) + getKeyStateDelay(e0e1_flag, kid.MakeCode, kid.UnitId, 1) > dwTime)
						{					 // delay-Bに達していない。
							goto start_loop; // next
						}
						else
						{																	 // delay-Bに達したので、keyを送る。時刻はst2に保存。flagはrepeatのまま。
							setKeyStateTime(e0e1_flag, kid.MakeCode, kid.UnitId, 1, dwTime); // st2
							bRepeat = true;
							Repeat_e0e1 = e0e1_flag;
							Repeat_flags = kid.Flags;
							Repeat_key = kid.MakeCode;
							Repeat_UnitId = kid.UnitId;
							break;
						}
					}
					else
					{ // up, keyを送る。時刻は初期化。flagは0へ。
						setKeyState(e0e1_flag, kid.MakeCode, kid.UnitId, 0);
						setKeyStateTime(e0e1_flag, kid.MakeCode, kid.UnitId, 0, 0); // st1
						setKeyStateTime(e0e1_flag, kid.MakeCode, kid.UnitId, 1, 0); // st2
						bRepeat = false;
						Repeat_e0e1 = 0;
						Repeat_flags = 0;
						Repeat_key = 0;
						Repeat_UnitId = 0;
						dwRepeat_time = 0;
						break;
					}

				default:
					goto start_loop;
					break;
				}
			}

		// キー入力があったときの処理開始
		// フォーカスのあるWindowチェック
		NODOKA_TRACE(_T("keyboardHandler: dequeued key sc=0x%x flags=0x%x\n"), kid.MakeCode, kid.Flags);
		checkFocusWindow();

		// UseDoublePress,SixPointのために、queueに入れる。
		injectInputIn(&kid);

		// def option FakeUp処理 E1-0x16 upを送る。
		if (m_setting)
			if (m_isEnabled && m_setting->m_UseFakeUp && m_keyboard_hook == 0)									// LL Hookでは無効
				if (!((m_mouse_hook == 1) && ((kid.Flags & KEYBOARD_INPUT_DATA::E1) && (kid.MakeCode < 0x0A)))) // Mouse Hook Codeは無効
				{
					KEYBOARD_INPUT_DATA kidFake;
					int delay;
					int MakeCode;

					if (m_setting && m_setting->m_UseUnitID == 1)
					{
						m_currentLock.press(Modifier::Type_Keyboard0, true); // def option UnitID有効ならばデフォルトK0にしておく。
						m_currentLock.press(Modifier::Type_Keyboard1, false);
						m_currentLock.press(Modifier::Type_Keyboard2, false);
						m_currentLock.press(Modifier::Type_Keyboard3, false);
						m_currentLock.press(Modifier::Type_Keyboard4, false);
						m_currentLock.press(Modifier::Type_Keyboard5, false);
						m_currentLock.press(Modifier::Type_Keyboard6, false);
						m_currentLock.press(Modifier::Type_Keyboard7, false);
					}
					delay = m_setting->m_FakeUpDelay;
					if (delay < 0)
					{
						delay = 0;
					}
					if (delay > 100)
					{
						delay = 100;
					}

					MakeCode = m_setting->m_FakeUpKey;

					kidFake.UnitId = (USHORT)0;
					kidFake.MakeCode = (USHORT)MakeCode;
					kidFake.Flags = KEYBOARD_INPUT_DATA::E1;
					kidFake.Reserved = (USHORT)0;
					kidFake.ExtraInformation = (USHORT)0;
					injectInputOut(&kidFake);

					kidFake.UnitId = (USHORT)0;
					kidFake.MakeCode = (USHORT)MakeCode;
					kidFake.Flags = KEYBOARD_INPUT_DATA::E1;
					kidFake.Flags |= KEYBOARD_INPUT_DATA::BREAK;
					kidFake.Reserved = (USHORT)0;
					kidFake.ExtraInformation = (USHORT)0;
					injectInputOut(&kidFake);

					Sleep(delay);
				}

		// logの処理
		if (!m_setting || // m_setting has not been loaded
			!m_isEnabled) // disabled
		{
			NODOKA_TRACE(_T("keyboardHandler: no setting or disabled, passthrough\n"));
			if (m_isLogMode)
			{
				Key key;
				key.addScanCode(ScanCode(kid.MakeCode, kid.Flags));
				outputToLog(&key, ModifiedKey(), 0);
				if ((kid.Flags & KEYBOARD_INPUT_DATA::E1) && (kid.MakeCode < 0x0A))
				{
					// through mouse event even if log mode
					injectInputOut(&kid);
				}
			}
			else
			{
				injectInputOut(&kid);
			}
			updateLastPressedKey(NULL);
			continue;
		}

		NODOKA_TRACE(_T("keyboardHandler: processing with setting\n"));
		Acquire a(&m_cs);

		if (!m_currentFocusOfThread ||
			!m_currentKeymap)
		{
			injectInputOut(&kid);
			updateLastPressedKey(NULL);
			continue;
		}

		// Sync modifier state with physical keys to avoid stuck modifiers
		syncModifiersFromGetAsyncKeyState();
		m_prevKeyEventTime = m_lastKeyEventTime; // save previous timestamp for ComboIdleThreshold
		m_lastKeyEventTime = timeGetTime();

		Current c;
		c.m_keymap = m_currentKeymap;
		c.m_i = m_currentFocusOfThread->m_keymaps.begin();

		// search key
		key.addScanCode(ScanCode(kid.MakeCode, kid.Flags));
		c.m_mkey = m_setting->m_keyboard.searchKey(key);
		if (!c.m_mkey.m_key)
		{
			c.m_mkey.m_key = m_setting->m_keyboard.searchPrefixKey(key);
			if (c.m_mkey.m_key)
			{
				continue;
			}
		}

		// press the key and update counter
		bool volatile isPhysicallyPressed = !(key.getScanCodes()[0].m_flags & ScanCode::BREAK);
		if (c.m_mkey.m_key)
		{
			if (!c.m_mkey.m_key->m_isPressed && isPhysicallyPressed)
				++m_currentKeyPressCount;
			else if (c.m_mkey.m_key->m_isPressed && !isPhysicallyPressed)
				--m_currentKeyPressCount;
			c.m_mkey.m_key->m_isPressed = isPhysicallyPressed;
		}
		{
			Acquire a(&m_log, 1);
			m_log << _T("m_currentKeyPressCount:") << m_currentKeyPressCount
				  << std::endl;
		}

		// create modifiers
		c.m_mkey.m_modifier = getCurrentModifiers(c.m_mkey.m_key, isPhysicallyPressed);
		Keymap::AssignMode am;
		bool isModifier = fixModifierKey(&c.m_mkey, &am);
		if (m_isPrefix)
		{
			if (isModifier && m_doesIgnoreModifierForPrefix)
				am = Keymap::AM_true;
			if (m_doesEditNextModifier)
			{
				Modifier modifier = m_modifierForNextKey;
				modifier.add(c.m_mkey.m_modifier);
				c.m_mkey.m_modifier = modifier;
			}
		}

		if (m_isLogMode)
		{
			outputToLog(&key, c.m_mkey, 0);
			if ((kid.Flags & KEYBOARD_INPUT_DATA::E1) && (kid.MakeCode < 0x0A))
			{
				// through mouse event even if log mode
				injectInputOut(&kid);
			}
		}
		else if (am == Keymap::AM_true)
		{
			{
				Acquire a(&m_log, 1);
				m_log << _T("* true modifier") << std::endl;
				// true modifier doesn't generate scan code
				outputToLog(&key, c.m_mkey, 1);
			}
		}
		else if (am == Keymap::AM_oneShot || am == Keymap::AM_oneShotRepeatable || am == Keymap::AM_oneShot2)
		{
			{
				Acquire a(&m_log, 1);
				if (am == Keymap::AM_oneShot)
					m_log << _T("* one shot modifier") << std::endl;
				else if (am == Keymap::AM_oneShotRepeatable)
					m_log << _T("* one shot repeatable modifier") << std::endl;
				else if (am == Keymap::AM_oneShot2)
					m_log << _T("* one shot 2 modifier") << std::endl;
			}
			// oneShot modifier doesn't generate scan code
			outputToLog(&key, c.m_mkey, 1);
			if (isPhysicallyPressed)
			{
				if (am == Keymap::AM_oneShotRepeatable && m_oneShotKey.m_key == c.m_mkey.m_key) // the key is repeating
				{
					m_oneShot2 = false;

					if (m_oneShotRepeatableRepeatCount < m_setting->m_oneShotRepeatableDelay)
					{
						; // delay
					}
					else
					{
						Current cnew = c;
						beginGeneratingKeyboardEvents(cnew, false, false); // oneShotRepeatable なので、begin..Eventsを呼ぶ
					}

					++m_oneShotRepeatableRepeatCount;
				}
				else if (am == Keymap::AM_oneShot)
				{
					m_oneShotKey = c.m_mkey; // oneShotなので、modifiyerのみ設定。begin..Eventsは呼ばない。
					m_oneShotRepeatableRepeatCount = 0;
					m_oneShot2 = false;
				}
				else // oneShot2
				{
					m_oneShotKey = c.m_mkey;
					m_oneShotRepeatableRepeatCount = 0; // oneShot2なので、modifyerを指定し、フラグを立てる。
														//m_oneShot2 = true;						// とりあえず封印。
														//beginGeneratingKeyboardEvents(c, false, m_oneShot2);	// m_oneShot2変数は不要だが、便宜上。
														// ここで呼ぶと単に oneShotRepeatableになってしまうので、コメントアウト。
				}
			}
			else // not isPhysicallyPressed					oneShotキー単独押下時の処理。
			{
				if (m_oneShotKey.m_key)
				{
					Current cnew = c;
					cnew.m_mkey.m_modifier = m_oneShotKey.m_modifier;
					cnew.m_mkey.m_modifier.off(Modifier::Type_Up);
					cnew.m_mkey.m_modifier.on(Modifier::Type_Down);
					beginGeneratingKeyboardEvents(cnew, false, false);

					cnew = c;
					cnew.m_mkey.m_modifier = m_oneShotKey.m_modifier;
					cnew.m_mkey.m_modifier.on(Modifier::Type_Up);
					cnew.m_mkey.m_modifier.off(Modifier::Type_Down);
					beginGeneratingKeyboardEvents(cnew, false, false);
				}
				m_oneShotKey.m_key = NULL;
				m_oneShotRepeatableRepeatCount = 0;
			}
		}
		else if (c.m_mkey.m_key)
		// normal key
		{
			bool handledBySM = false;
			bool isKeyDown   = !(kid.Flags & KEYBOARD_INPUT_DATA::BREAK);
			Key *pressedKey  = c.m_mkey.m_key;

			// ---- TapHold state machine ----
			{
				// Build current modifier state for rule matching
				Modifier thMod = getCurrentModifiers(pressedKey, isKeyDown);
				thMod.dontcare(Modifier::Type_Up);
				thMod.dontcare(Modifier::Type_Down);
				thMod.dontcare(Modifier::Type_Repeat);

				const TapHoldRule *thr = NULL;
				if (m_currentFocusOfThread)
					for (const Keymap *km : m_currentFocusOfThread->m_keymaps)
					{
						thr = km->searchTapHold(pressedKey, thMod);
						if (thr) break;
					}

				if (m_tapHoldState == TH_PENDING && m_tapHoldCurrentRule)
				{
					if (!isKeyDown && m_tapHoldCurrentRule->m_key == pressedKey)
					{
						// TH key released before threshold → tap
						m_tapHoldState = TH_IDLE;
						SetEvent(m_tapHoldCancelEvent);
						generateKeySeqEvents(m_tapHoldContext, m_tapHoldCurrentRule->m_tapAction, Part_all);
						m_tapHoldLastTapKey  = m_tapHoldCurrentRule->m_key;
						m_tapHoldLastTapTime = GetTickCount();
						if (m_tapHoldHasBufferedKey)
						{
							std::vector<KEYBOARD_INPUT_DATA> v = {m_tapHoldBufferedKey};
							reInjectKeys(v);
							m_tapHoldHasBufferedKey     = false;
							m_tapHoldPermissiveOtherKey = NULL;
						}
						m_tapHoldCurrentRule = NULL;
						handledBySM = true;
					}
					else if (!isKeyDown && m_tapHoldHasBufferedKey
					         && pressedKey == m_tapHoldPermissiveOtherKey)
					{
						// Permissive hold: buffered key's UP arrived → confirm hold
						SetEvent(m_tapHoldCancelEvent);
						generateKeySeqEvents(m_tapHoldContext, m_tapHoldCurrentRule->m_holdAction, Part_down);
						std::vector<KEYBOARD_INPUT_DATA> v = {m_tapHoldBufferedKey, kid};
						reInjectKeys(v);
						m_tapHoldHasBufferedKey     = false;
						m_tapHoldPermissiveOtherKey = NULL;
						m_tapHoldNeedPartSecond     = true;
						m_tapHoldState              = TH_HOLDING;
						handledBySM = true;
					}
					else if (isKeyDown && (thr == NULL || thr != m_tapHoldCurrentRule))
					{
						// Another key pressed during PENDING → check mode
						bool permHold = (m_tapHoldCurrentRule->m_permissiveHold >= 0)
							? (m_tapHoldCurrentRule->m_permissiveHold == 1)
							: (m_setting && m_setting->m_TapHoldPermissiveHold);
						bool holdOnOther = (m_tapHoldCurrentRule->m_holdOnOtherKey >= 0)
							? (m_tapHoldCurrentRule->m_holdOnOtherKey == 1)
							: (m_setting && m_setting->m_TapHoldOnOtherKeyPress);

						if (permHold && !m_tapHoldHasBufferedKey)
						{
							// Permissive hold: buffer this key-down, wait for its UP
							m_tapHoldBufferedKey        = kid;
							m_tapHoldHasBufferedKey     = true;
							m_tapHoldPermissiveOtherKey = pressedKey;
							handledBySM = true;
						}
						else if (holdOnOther && !permHold)
						{
							// Hold on Other Key Press: fire hold Part_down immediately
							SetEvent(m_tapHoldCancelEvent);
							generateKeySeqEvents(m_tapHoldContext, m_tapHoldCurrentRule->m_holdAction, Part_down);
							m_tapHoldNeedPartSecond = true;
							m_tapHoldState          = TH_HOLDING;
							// Fall through: process the new key normally below
						}
						else
						{
							// Existing interrupt behavior
							bool interruptIsTap = (m_tapHoldCurrentRule->m_interrupt >= 0)
								? (m_tapHoldCurrentRule->m_interrupt == 1)
								: (m_setting ? m_setting->m_TapHoldInterruptIsTap : true);
							m_tapHoldState = TH_HOLDING;
							SetEvent(m_tapHoldCancelEvent);
							const KeySeq *interruptSeq = interruptIsTap
								? m_tapHoldCurrentRule->m_tapAction
								: m_tapHoldCurrentRule->m_holdAction;
							generateKeySeqEvents(m_tapHoldContext, interruptSeq, Part_all);
							// Fall through: process the new key normally below
						}
					}
				}

				if (!handledBySM && isKeyDown && thr != NULL && m_tapHoldState == TH_IDLE && !isSmReinjected)
				{
					// Quick Tap Term check: same key pressed again within QTT window → immediate tap
					int qtt = (thr->m_quickTapTerm >= 0) ? thr->m_quickTapTerm
					          : (m_setting ? m_setting->m_TapHoldQuickTapTerm : 0);
					if (qtt > 0 && m_tapHoldLastTapKey == thr->m_key
					    && (int)(GetTickCount() - m_tapHoldLastTapTime) < qtt)
					{
						generateKeySeqEvents(c, thr->m_tapAction, Part_all);
						m_tapHoldLastTapKey  = thr->m_key;
						m_tapHoldLastTapTime = GetTickCount();
						handledBySM = true;
					}
					else
					{
						// Enter PENDING state
						m_tapHoldState          = TH_PENDING;
						m_tapHoldCurrentRule    = thr;
						m_tapHoldContext        = c;
						m_tapHoldKeyData        = kid;
						m_tapHoldThreshold      = (thr->m_threshold >= 0)
							? thr->m_threshold
							: (m_setting ? m_setting->m_TapHoldThreshold : 200);
						m_tapHoldModifier       = thMod;
						m_tapHoldNeedPartSecond = false;
						m_tapHoldHasBufferedKey = false;
						m_tapHoldPermissiveOtherKey = NULL;
						SetEvent(m_tapHoldStartEvent);
						handledBySM = true;
					}
				}

				if (!handledBySM && !isKeyDown && m_tapHoldState == TH_HOLDING
					&& m_tapHoldCurrentRule && m_tapHoldCurrentRule->m_key == pressedKey)
				{
					// Key released after hold
					if (m_tapHoldNeedPartSecond)
					{
						// hold was split (Part_down already fired) → fire Part_up now
						generateKeySeqEvents(m_tapHoldContext, m_tapHoldCurrentRule->m_holdAction, Part_up);
						m_tapHoldNeedPartSecond = false;
					}
					m_tapHoldState       = TH_IDLE;
					m_tapHoldCurrentRule = NULL;
					handledBySM = true;
				}
			}

			// ---- TapDance state machine ----
			if (!handledBySM)
			{
				// Build current modifier state for rule matching
				Modifier tdMod = getCurrentModifiers(pressedKey, isKeyDown);
				tdMod.dontcare(Modifier::Type_Up);
				tdMod.dontcare(Modifier::Type_Down);
				tdMod.dontcare(Modifier::Type_Repeat);

				const TapDanceRule *tdr = NULL;
				if (m_currentFocusOfThread)
					for (const Keymap *km : m_currentFocusOfThread->m_keymaps)
					{
						tdr = km->searchTapDance(pressedKey, tdMod);
						if (tdr) break;
					}

				if (m_tapDanceState == TD_COUNTING && m_tapDanceCurrentRule
					&& m_tapDanceCurrentRule->m_key == pressedKey)
				{
					if (isKeyDown && m_tapDanceCount < 3) ++m_tapDanceCount;
					m_tapDanceBuffered.push_back(kid);
					SetEvent(m_tapDanceStartEvent); // restart (or re-signal) the timer
					handledBySM = true;
				}
				else if (isKeyDown && tdr != NULL && m_tapDanceState == TD_IDLE && !isSmReinjected)
				{
					// First tap of a TapDance sequence
					m_tapDanceState       = TD_COUNTING;
					m_tapDanceCurrentRule = tdr;
					m_tapDanceContext     = c;
					m_tapDanceCount       = 1;
					m_tapDanceBuffered.clear();
					m_tapDanceBuffered.push_back(kid);
					m_tapDanceTimeout = (tdr->m_timeout >= 0)
						? tdr->m_timeout
						: (m_setting ? m_setting->m_TapDanceTimeout : 300);
					// Capture modifier state for duration of this tap-dance sequence
					m_tapDanceModifier = tdMod;
					SetEvent(m_tapDanceStartEvent);
					handledBySM = true;
				}
				else if (m_tapDanceState == TD_COUNTING && m_tapDanceCurrentRule
					&& m_tapDanceCurrentRule->m_key != pressedKey)
				{
					// Different key while counting → fire with current count, then re-inject this key
					int tapIdx = m_tapDanceCount - 1;
					if (tapIdx >= 0 && tapIdx < 3 && m_tapDanceCurrentRule->m_tap[tapIdx])
					{
						SetEvent(m_tapDanceCancelEvent);
						m_tapDanceState = TD_IDLE;
						generateKeySeqEvents(m_tapDanceContext, m_tapDanceCurrentRule->m_tap[tapIdx], Part_all);
					}
					else
					{
						SetEvent(m_tapDanceCancelEvent);
						m_tapDanceState = TD_IDLE;
						reInjectKeys(m_tapDanceBuffered);
					}
					m_tapDanceCurrentRule = NULL;
					m_tapDanceBuffered.clear();
					// Let current key fall through to normal processing
				}
			}

			// ---- Combo state machine ----
			if (!handledBySM)
			{
				bool candidate    = isComboCandidate(pressedKey);
				bool isReInjected = isSmReinjected;

				// Use engine-level cached mode (overridable at runtime via &SetComboDetector)
				Setting::ComboDetectorMode detMode = m_comboDetectorMode;
				// zero-latency uses timer only when IME is ON; IME OFF falls back to immediate
				bool useTimer          = (detMode == Setting::CD_TIMEOUT || detMode == Setting::CD_STRICT_ORDER)
				                         || (detMode == Setting::CD_ZERO_LATENCY
				                             && m_currentLock.isOn(Modifier::Type_ImeLock));
				bool useOrdered        = (detMode == Setting::CD_STRICT_ORDER);
				bool noAbortOnNonCombo = (detMode == Setting::CD_ROLLOVER);
				int  overlapRatio      = m_comboOverlapRatioActive;

				// ---- CO_PENDING_EVAL: all keys down, waiting for first key-up to calc overlap ----
				if (m_comboAllKeysDown && m_comboState == CO_PENDING)
				{
					if (!isKeyDown)
					{
						bool isComboKey =
							std::find(m_comboPressedKeys.begin(), m_comboPressedKeys.end(), pressedKey)
							!= m_comboPressedKeys.end();

						if (isComboKey)
						{
							// Compute overlap ratio: overlap = time from all-keys-down until now
							DWORD now         = GetTickCount();
							DWORD overlapTime = now - m_comboAllKeysDownTime;
							DWORD keyDownTime = 0;
							auto it = m_comboKeyDownTimes.find(pressedKey);
							if (it != m_comboKeyDownTimes.end()) keyDownTime = it->second;
							DWORD pressDuration = (keyDownTime > 0) ? (now - keyDownTime) : overlapTime;
							int   ratio = (pressDuration > 0) ? (int)(overlapTime * 100 / pressDuration) : 0;

							const ComboRule *matchedRule = NULL;
							if (m_currentFocusOfThread)
								for (const Keymap *km : m_currentFocusOfThread->m_keymaps)
								{
									matchedRule = useOrdered
										? km->searchComboOrdered(m_comboPressedKeys, m_comboModifier)
										: km->searchCombo(m_comboPressedKeys, m_comboModifier);
									if (matchedRule) break;
								}

							m_comboAllKeysDown = false;
							m_comboState = CO_IDLE;
							m_comboKeyDownTimes.clear();

							if (ratio >= overlapRatio && matchedRule)
							{
								// Overlap sufficient — fire combo
								if (m_comboZeroLatencyActive)
								{
									keybd_event(VK_BACK, 0, 0, 0);
									keybd_event(VK_BACK, 0, KEYEVENTF_KEYUP, 0);
								}
								generateKeySeqEvents(m_comboContext, matchedRule->m_action, Part_all);
								// Re-inject key-up for the released key so apps see the release
								std::vector<KEYBOARD_INPUT_DATA> upEvent;
								upEvent.push_back(kid);
								reInjectKeys(upEvent);
							}
							else
							{
								// Overlap insufficient — re-inject all buffered events + this key-up
								if (!m_comboZeroLatencyActive)
								{
									std::vector<KEYBOARD_INPUT_DATA> toReInject = m_comboBuffered;
									toReInject.push_back(kid);
									reInjectKeys(toReInject);
								}
								else
								{
									// zero-latency: key1 already output; only re-inject the key-up
									std::vector<KEYBOARD_INPUT_DATA> upEv;
									upEv.push_back(kid);
									reInjectKeys(upEv);
								}
							}
							m_comboZeroLatencyActive = false;
							m_comboPressedKeys.clear();
							m_comboBuffered.clear();
							handledBySM = true;
						}
						else
						{
							// Key-up unrelated to combo — buffer
							m_comboBuffered.push_back(kid);
							handledBySM = true;
						}
					}
					else if (isKeyDown)
					{
						// New key pressed during CO_PENDING_EVAL — buffer (treat like rollover)
						m_comboBuffered.push_back(kid);
						handledBySM = true;
					}
				}

				if (!handledBySM && m_comboState == CO_PENDING)
				{
					if (isKeyDown && candidate)
					{
						// Additional combo key within the window — track press time
						m_comboKeyDownTimes[pressedKey] = GetTickCount();
						m_comboPressedKeys.push_back(pressedKey);
						// strict-order: preserve press order; others: sort for order-independent match
						if (!useOrdered)
							std::sort(m_comboPressedKeys.begin(), m_comboPressedKeys.end());
						m_comboBuffered.push_back(kid);

						// Check for an exact rule match
						const ComboRule *matchedRule = NULL;
						if (m_currentFocusOfThread)
							for (const Keymap *km : m_currentFocusOfThread->m_keymaps)
							{
								matchedRule = useOrdered
									? km->searchComboOrdered(m_comboPressedKeys, m_comboModifier)
									: km->searchCombo(m_comboPressedKeys, m_comboModifier);
								if (matchedRule) break;
							}

						if (matchedRule)
						{
							if (overlapRatio > 0)
							{
								// ComboOverlapRatio: defer firing until first key-up
								SetEvent(m_comboCancelEvent); // cancel timeout timer
								m_comboAllKeysDown     = true;
								m_comboAllKeysDownTime = GetTickCount();
								// stay in CO_PENDING; m_comboAllKeysDown signals CO_PENDING_EVAL
							}
							else
							{
								// All combo keys pressed — fire immediately
								SetEvent(m_comboCancelEvent);
								m_comboState = CO_IDLE;
								if (m_comboZeroLatencyActive)
								{
									keybd_event(VK_BACK, 0, 0, 0);
									keybd_event(VK_BACK, 0, KEYEVENTF_KEYUP, 0);
								}
								generateKeySeqEvents(m_comboContext, matchedRule->m_action, Part_all);
								// rollover: re-inject any non-combo-key events buffered during CO_PENDING
								if (detMode == Setting::CD_ROLLOVER)
								{
									std::vector<KEYBOARD_INPUT_DATA> residual;
									for (const auto &ev : m_comboBuffered)
									{
										bool isKeyUp = (ev.Flags & KEYBOARD_INPUT_DATA::BREAK) != 0;
										if (isKeyUp) { residual.push_back(ev); continue; }
										Key tmpKey;
										tmpKey.addScanCode(ScanCode(ev.MakeCode, ev.Flags));
										Key *evKey = m_setting
											? m_setting->m_keyboard.searchKey(tmpKey) : nullptr;
										bool isComboKey = evKey &&
											std::find(m_comboPressedKeys.begin(),
													  m_comboPressedKeys.end(), evKey)
											!= m_comboPressedKeys.end();
										if (!isComboKey)
											residual.push_back(ev);
									}
									if (!residual.empty()) reInjectKeys(residual);
								}
								m_comboZeroLatencyActive = false;
								m_comboPressedKeys.clear();
								m_comboBuffered.clear();
								m_comboKeyDownTimes.clear();
							}
						}
						else
						{
							// More keys may come — restart the window timer (timer modes only)
							if (useTimer)
								SetEvent(m_comboStartEvent);
						}
						handledBySM = true;
					}
					else if (!isKeyDown)
					{
						bool isTrackedComboKey =
							std::find(m_comboPressedKeys.begin(), m_comboPressedKeys.end(), pressedKey)
							!= m_comboPressedKeys.end();

						if (isTrackedComboKey)
						{
							// Combo key released early — abort
							SetEvent(m_comboCancelEvent);
							m_comboState = CO_IDLE;
							if (m_comboZeroLatencyActive)
							{
								// key1 already output; only re-inject the key-up for it
								std::vector<KEYBOARD_INPUT_DATA> upEv;
								upEv.push_back(kid);
								reInjectKeys(upEv);
							}
							else
							{
								std::vector<KEYBOARD_INPUT_DATA> toReInject = m_comboBuffered;
								toReInject.push_back(kid);
								reInjectKeys(toReInject);
							}
							m_comboZeroLatencyActive = false;
							m_comboPressedKeys.clear();
							m_comboBuffered.clear();
							m_comboKeyDownTimes.clear();
							m_comboAllKeysDown = false;
						}
						else
						{
							// Key-up unrelated to combo — buffer and keep CO_PENDING
							m_comboBuffered.push_back(kid);
						}
						handledBySM = true;
					}
					else
					{
						// Non-combo key pressed during CO_PENDING
						if (noAbortOnNonCombo)
						{
							// rollover: buffer the non-combo key and keep waiting
							m_comboBuffered.push_back(kid);
							handledBySM = true;
						}
						else
						{
							// timeout / immediate / strict-order / zero-latency: abort
							SetEvent(m_comboCancelEvent);
							m_comboState = CO_IDLE;
							if (!m_comboZeroLatencyActive)
								reInjectKeys(m_comboBuffered);
							m_comboZeroLatencyActive = false;
							m_comboPressedKeys.clear();
							m_comboBuffered.clear();
							m_comboKeyDownTimes.clear();
							m_comboAllKeysDown = false;
							// Fall through to normal key processing below
						}
					}
				}

				if (!handledBySM && !isReInjected && isKeyDown && candidate && m_comboState == CO_IDLE)
				{
					// idle-distance guard: only enter CO_PENDING if the user paused long enough
					// Use pending idle value (reset to def option when unspecified)
					int idleThresh = (m_comboIdleThresholdPending >= 0)
					    ? m_comboIdleThresholdPending
					    : (m_setting ? m_setting->m_ComboIdleThreshold : 0);
					if (idleThresh > 0 && m_prevKeyEventTime != 0)
					{
						DWORD elapsed = m_lastKeyEventTime - m_prevKeyEventTime;
						if (elapsed < (DWORD)idleThresh)
							goto combo_idle_skip;
					}

					// Sync mode and optional params at CO_IDLE → CO_PENDING boundary
					m_comboDetectorMode = m_comboDetectorModePending;
					detMode = m_comboDetectorMode;
					useTimer          = (detMode == Setting::CD_TIMEOUT || detMode == Setting::CD_STRICT_ORDER)
					                    || (detMode == Setting::CD_ZERO_LATENCY
					                        && m_currentLock.isOn(Modifier::Type_ImeLock));
					useOrdered        = (detMode == Setting::CD_STRICT_ORDER);
					noAbortOnNonCombo = (detMode == Setting::CD_ROLLOVER);
					// Apply optional param overrides (pending → active; -1 falls back to def option)
					m_comboWindowActive            = (m_comboWindowPending >= 0)
					    ? m_comboWindowPending
					    : (m_setting ? m_setting->m_ComboWindow : 50);
					m_comboOverlapRatioActive      = (m_comboOverlapRatioPending >= 0)
					    ? m_comboOverlapRatioPending
					    : (m_setting ? m_setting->m_ComboOverlapRatio : 0);
					m_comboNestedAlwaysMatchActive = (m_comboNestedAlwaysMatchPending >= 0)
					    ? (m_comboNestedAlwaysMatchPending != 0)
					    : (m_setting ? m_setting->m_ComboNestedAlwaysMatch : false);
					m_comboIdleThresholdActive     = (m_comboIdleThresholdPending >= 0)
					    ? m_comboIdleThresholdPending
					    : (m_setting ? m_setting->m_ComboIdleThreshold : 0);

					// First potential combo key — enter PENDING
					m_comboState = CO_PENDING;
					m_comboContext = c;
					m_comboPressedKeys.clear();
					m_comboPressedKeys.push_back(pressedKey);
					m_comboBuffered.clear();
					m_comboBuffered.push_back(kid);
					m_comboKeyDownTimes.clear();
					m_comboKeyDownTimes[pressedKey] = GetTickCount();
					m_comboAllKeysDown = false;
					// Capture modifier state for rule matching (Up/Down/Repeat are irrelevant)
					m_comboModifier = getCurrentModifiers(pressedKey, true);
					m_comboModifier.dontcare(Modifier::Type_Up);
					m_comboModifier.dontcare(Modifier::Type_Down);
					m_comboModifier.dontcare(Modifier::Type_Repeat);

					// zero-latency: immediately output key1 if IME is ON
					m_comboZeroLatencyActive = false;
					if (detMode == Setting::CD_ZERO_LATENCY
					    && m_currentLock.isOn(Modifier::Type_ImeLock))
					{
						// Output key1 now so the user sees zero latency
						outputToLog(&key, c.m_mkey, 1);
						beginGeneratingKeyboardEvents(c, isModifier, false);
						m_comboZeroLatencyActive = true;
						// Fall through: timer will be started below (ComboWindow)
					}

					if (useTimer)
					{
						// Determine effective window (use per-rule override if present, or active value)
						int effectiveWindow = m_comboWindowActive;
						if (m_currentFocusOfThread)
							for (const Keymap *km : m_currentFocusOfThread->m_keymaps)
								for (const auto &rule : km->getComboRules())
									for (const Key *k : rule.m_keys)
										if (k == pressedKey && rule.m_window >= 0)
										{
											effectiveWindow = rule.m_window;
											goto combo_window_done;
										}
					combo_window_done:
						m_comboWindow = effectiveWindow;
						SetEvent(m_comboStartEvent);
					}
					// immediate / rollover: no timer started
					// zero-latency + IME OFF: behaves like immediate (useTimer driven by CD_ZERO_LATENCY
					//   but m_comboZeroLatencyActive=false, so abort paths re-inject normally)
					handledBySM = true;
				}
			combo_idle_skip:;
			}

			// ---- Normal key processing (if not consumed by a state machine) ----
			if (!handledBySM)
			{
			outputToLog(&key, c.m_mkey, 1);
			if (isPhysicallyPressed) // もし他のキーが押されたなら oneShotは無効にする。
			{
				m_oneShotKey.m_key = NULL;
				{
					Acquire a(&m_log, 1);
					m_log << _T("one shot modifier is NULL") << std::endl;
				}
			}
			beginGeneratingKeyboardEvents(c, isModifier, false);
			}
		}
		else
		{
			// undefined key
			if ((kid.Flags & KEYBOARD_INPUT_DATA::E1) && (kid.MakeCode < 0x0A))
			{
				// through mouse event even if undefined for fail safe
				injectInputOut(&kid);
			}
		}

		// if counter is zero, reset modifiers and keys on win32
		{
			Acquire a(&m_log, 1);
			m_log << _T("m_currentKeyPressCount:") << m_currentKeyPressCount
				  << std::endl;
		}

		if (m_currentKeyPressCount <= 0)
		{
			{
				Acquire a(&m_log, 1);
				m_log << _T("* No key is pressed") << std::endl;
			}
			generateModifierEvents(Modifier());
			if (0 < m_currentKeyPressCountOnWin32)
				keyboardResetOnWin32();
			m_currentKeyPressCount = 0;
			m_currentKeyPressCountOnWin32 = 0;
			m_oneShotKey.m_key = NULL;

			for (int i = Modifier::Type_TouchpadSticky; i <= Modifier::Type_TouchpadRSticky; ++i)
				m_currentLock.release(static_cast<Modifier::Type>(i));

			for (int i = Modifier::Type_Keyboard0; i <= Modifier::Type_Keyboard7; ++i)
				m_currentLock.release(static_cast<Modifier::Type>(i));

			m_currentLock.release(Modifier::Type_DP);
		}

		key.initialize();
		updateLastPressedKey(isPhysicallyPressed ? c.m_mkey.m_key : NULL);
	} // while
break_while:
	NODOKA_TRACE(_T("keyboardHandler: break_while reached, signaling m_threadEvent\n"));
	CHECK_TRUE(SetEvent(m_threadEvent));
	NODOKA_TRACE(_T("keyboardHandler: m_threadEvent signaled, thread exiting\n"));
}

// CheckModifier thread
unsigned int WINAPI Engine::CheckModifier(void *i_this)
{
	reinterpret_cast<Engine *>(i_this)->CheckModifier();
	_endthreadex(0);
	return 0;
}
void Engine::CheckModifier()
{
	HANDLE handle = m_interruptThreadCheckModifierEvent;

	// initialize ok
#define Check_keymax 6
	CHECK_TRUE(SetEvent(m_threadCheckModifierEvent));
	DWORD dwWaitTime = 1000; // 1s
	int checkkey[Check_keymax] = {VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, VK_RWIN, VK_RETURN};
	int countkey[Check_keymax] = {0, 0, 0, 0, 0, 0};
	unsigned int sendkey[Check_keymax] = {0x1a, 0x1b, 0x1c, 0x19, 0x19, 0x1e}; // LWINとRWINは同列
	int loop_i = 0;
	KEYBOARD_INPUT_DATA kid;
	int checktime = 0;

	// loop
	while (!m_doCheckModifierForceTerminate)
	{
		switch (WaitForSingleObject(handle, dwWaitTime))
		{
		case WAIT_TIMEOUT: // timeoutした。
			if (m_setting)
			{
				if (m_isEnabled && m_setting->m_CheckModifier) // def option CheckModifier enable
				{
					loop_i = 0;
					checktime = m_setting->m_CheckModifierTime;
					if (checktime <= 1) // 値域を1から30とする。
						checktime = 1;
					if (checktime > 30)
						checktime = 30;

					while (loop_i < Check_keymax)
					{
						if (GetKeyState(checkkey[loop_i]) < 0) // down
							countkey[loop_i]++;
						else					  // up
							countkey[loop_i] = 0; // 初期化

						if (countkey[loop_i] >= checktime)
						{						  // checktimeを超えたら検出
							countkey[loop_i] = 0; // clear

							kid.MakeCode = sendkey[loop_i];
							kid.Flags = KEYBOARD_INPUT_DATA::E1;
							kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;

							SendtoKeyboardHandler(1, kid.Flags, kid.MakeCode);
						}
						loop_i++; // change checkkey
					}
				}
				if (m_isEnabled && m_setting->m_ModifierAutoClear > 0 &&
					m_currentKeyPressCount > 0 &&
					timeGetTime() - m_lastKeyEventTime >= (DWORD)(m_setting->m_ModifierAutoClear * 1000))
				{
					Acquire a(&m_cs);
					modifierAutoClear();
				}
			}
			break;
		case WAIT_OBJECT_0: // m_interruptThreadCheckModifierEvent
			switch (m_interruptThreadCheckModifierReason)
			{
			default:
			{
				ASSERT(false);
				break;
			}

			case InterruptThreadReason_Terminate:
				goto break_while;

			case InterruptThreadReason_Resume:
				// Stray/unpaired Resume (e.g. resume() called twice without an
				// intervening pause()): we are not inside the Pause-wait loop below,
				// so there is nothing to undo. Ack anyway so the caller's
				// WaitForSingleObject(m_threadCheckModifierEvent, ...) do-while loop
				// does not spin forever on the UI thread.
				SESSTRACE(m_log, _T("CheckModifier: stray Resume received (not paused), acking"));
				CHECK_TRUE(SetEvent(m_threadCheckModifierEvent));
				break;

			case InterruptThreadReason_Pause:
			{
				SESSTRACE(m_log, _T("CheckModifier: Pause received, acking"));
				CHECK_TRUE(SetEvent(m_threadCheckModifierEvent));

				while (WaitForSingleObject(handle, INFINITE) != WAIT_OBJECT_0)
					switch (m_interruptThreadCheckModifierReason)
					{
					case InterruptThreadReason_Terminate:
						goto break_while;

					case InterruptThreadReason_Resume:
						break;
					}
				SESSTRACE(m_log, _T("CheckModifier: wait done reason=") << m_interruptThreadCheckModifierReason << _T(", acking"));
				CHECK_TRUE(SetEvent(m_threadCheckModifierEvent));
			}
			}
			break;
		} // switch(WaitForSingleObject)
	}	 // while
break_while:

	CHECK_TRUE(SetEvent(m_threadCheckModifierEvent));
}

// KeyboardPast thread
unsigned int WINAPI Engine::KeyboardPast(void *i_this)
{
	reinterpret_cast<Engine *>(i_this)->KeyboardPast();
	_endthreadex(0);
	return 0;
}
void Engine::KeyboardPast()
{
	DWORD dwWaitTime = 5; // 5ms
	DWORD dwPeriod = 10;  // 10ms
	KEYBOARD_PAST last_data;
	KEYBOARD_PAST current_data;

	HANDLE handle = m_interruptThreadKeyboardPastEvent;

	CHECK_TRUE(SetEvent(m_threadKeyboardPastEvent));

	last_data.kid.MakeCode = 0x255;
	last_data.time_stamp = 0;

	// loop
	while (!m_doKeyboardPastForceTerminate)
	{
		if (m_setting)
			if (m_isEnabled && m_setting->m_UseDoublePress) // def option KeyboardPast enable
			{
				dwPeriod = m_setting->m_DoublePressPeriod;

				// まず m_pastQueueMutexを待つ。
				WaitForSingleObject(m_pastQueueMutex, INFINITE);

				// まず一つ以上あることを確認し、ひとつ取り出す。
				if (m_pastQueue->size() >= 1)
				{
					current_data = m_pastQueue->front();
					m_pastQueue->pop_front();
					ReleaseMutex(m_pastQueueMutex);
				}
				else
				{
					ReleaseMutex(m_pastQueueMutex);
					//setLockState4(false);				// このresetが無いと、キー入力がしばらくなかった後でDPが発生する。
					goto loop_next;
				}

				// current_dataがdownだったら終了
				if ((current_data.kid.Flags & 0x0001) == 0) // 0 as press, 1 as release
				{
					setLockState4(false);
					goto loop_next;
				}
				// current_dataがupだったら、last_dataと比較
				if (current_data.kid.MakeCode == last_data.kid.MakeCode)
				{
					// 同じキーで、時間がdwPeriod以下ならDoublePress検出
					if (current_data.time_stamp - last_data.time_stamp <= dwPeriod)
					{
						// モディファイヤー DP- を有効にする。
						setLockState4(true);
					}
					else
					{
						// 同じキーで時間を越えていたらlast_dataに保存
						setLockState4(false);
					}
					last_data = current_data;
					goto loop_next;
				}
				else
				{
					// 別のキーならlast_dataに保存
					last_data = current_data;
					setLockState4(false);
					goto loop_next;
				}
			}
		setLockState4(false);
	loop_next:
		// 続いて、スレッドへの割り込みを待つ。
		switch (WaitForSingleObject(handle, dwWaitTime))
		{
		case WAIT_TIMEOUT: // timeoutした。
			break;
		case WAIT_OBJECT_0: // m_interruptThreadKeyboardPastEvent
			switch (m_interruptThreadKeyboardPastReason)
			{
			default:
				ASSERT(false);
				break;

			case InterruptThreadReason_Terminate:
				goto break_while;

			case InterruptThreadReason_Resume:
				// Stray/unpaired Resume (e.g. resume() called twice without an
				// intervening pause()): we are not inside the Pause-wait loop below,
				// so there is nothing to undo. Ack anyway so the caller's
				// WaitForSingleObject(m_threadKeyboardPastEvent, ...) do-while loop
				// does not spin forever on the UI thread.
				SESSTRACE(m_log, _T("KeyboardPast: stray Resume received (not paused), acking"));
				CHECK_TRUE(SetEvent(m_threadKeyboardPastEvent));
				break;

			case InterruptThreadReason_Pause:
			{
				SESSTRACE(m_log, _T("KeyboardPast: Pause received, acking"));
				CHECK_TRUE(SetEvent(m_threadKeyboardPastEvent));
				while (WaitForMultipleObjects(1, &m_interruptThreadKeyboardPastEvent, FALSE, INFINITE) != WAIT_OBJECT_0)
				{
					switch (m_interruptThreadKeyboardPastReason)
					{
					case InterruptThreadReason_Terminate:
						goto break_while;
					case InterruptThreadReason_Resume:
						break;
					default:
						break;
					}
				}
				SESSTRACE(m_log, _T("KeyboardPast: wait done reason=") << m_interruptThreadKeyboardPastReason << _T(", acking"));
				CHECK_TRUE(SetEvent(m_threadKeyboardPastEvent));
			}
			break;
			} // switch m_interruptThreadKeyboardPastReason
			break;
		} // switch (WaitForSingleObject(handle, delayTime))
	}	 // while
	ReleaseMutex(m_pastQueueMutex);

break_while:
	ReleaseMutex(m_pastQueueMutex);
	CHECK_TRUE(SetEvent(m_threadKeyboardPastEvent));
}

// For 6 point thread
unsigned int WINAPI Engine::For6point(void *i_this)
{
	reinterpret_cast<Engine *>(i_this)->For6point();
	_endthreadex(0);
	return 0;
}

void Engine::For6point()
{
#define Check_keymax_for6point 6

	DWORD dwWaitTime = 100; // 100ms 1秒間に、最大10回のキーアップダウン検出が可能

	int loop_i = 0;
	KEYBOARD_INPUT_DATA kid;
	KEYBOARD_PAST current_data;

	unsigned int checkkey[Check_keymax_for6point] = {0, 0, 0, 0, 0, 0};
	unsigned int checkkey_state[Check_keymax_for6point] = {0, 0, 0, 0, 0, 0};
	unsigned int checksum = 0;

	HANDLE handle = m_interruptThreadFor6pointEvent;

	CHECK_TRUE(SetEvent(m_threadFor6pointEvent));

	// loop
	while (!m_doFor6pointForceTerminate)
	{

		if (m_setting)
			if (m_isEnabled && m_setting->m_For6point) // def option SixPoint enable
			{
				// チェック対象情報を取り出す。
				checkkey[0] = m_setting->m_key1of6;
				checkkey[1] = m_setting->m_key2of6;
				checkkey[2] = m_setting->m_key3of6;
				checkkey[3] = m_setting->m_key4of6;
				checkkey[4] = m_setting->m_key5of6;
				checkkey[5] = m_setting->m_key6of6;

				// まず m_for6pointQueueMutexを待つ。
				WaitForSingleObject(m_for6pointQueueMutex, INFINITE);

				// まず一つ以上あることを確認し、ひとつ取り出す。
				if (m_for6pointQueue->size() >= 1)
				{
					current_data = m_for6pointQueue->front();
					m_for6pointQueue->pop_front();
					ReleaseMutex(m_for6pointQueueMutex);

					if ((current_data.kid.Flags & KEYBOARD_INPUT_DATA::E0) || (current_data.kid.Flags & KEYBOARD_INPUT_DATA::E1))
						goto loop_next;
				}
				else
				{
					ReleaseMutex(m_for6pointQueueMutex);
					goto loop_next;
				}

				// チェック対象のキーのDown/Up状態を保存する。
				loop_i = 0;
				while (loop_i < Check_keymax_for6point)
				{
					// キーコードがチェック対象の場合
					if (current_data.kid.MakeCode == checkkey[loop_i])
					{
						//DBG_PRINT(_T(" get #%d as %d key"), loop_i, checkkey[loop_i]);
						// 初めてDown(make)されたら、フラグを立てる。
						// checkkey_state as 0: none, 1: 1回down, 2: up: state 1のときに、upが来た場合

						if (checkkey_state[loop_i] == 0 && !(current_data.kid.Flags & KEYBOARD_INPUT_DATA::BREAK))
						{
							// 初めてキーダウンが来た
							checkkey_state[loop_i] = 1;
							//DBG_PRINT(_T(" down\n"));
						}
						else
						{
							if (checkkey_state[loop_i] == 1 && (current_data.kid.Flags & KEYBOARD_INPUT_DATA::BREAK))
							{
								// キーダウンした後で、キーアップした。
								checkkey_state[loop_i] = 2;
								//DBG_PRINT(_T(" up\n"));
							}
						}
					}
					loop_i++; // change checkkey
				}

				// すべてのキーのstateを確認して、0のみなら抜ける。
				// 1があったら抜ける
				// 最後までチェックして0と2だけしかなければSix Up

				// 000000 = 0 none
				// 000001 = 1 none
				// 000002 = 2 up
				// 000010 = 1 none
				// 000011 = 2 none
				// 000012 = 3 none
				// 000020 = 2 up
				// 000021 = 3 none
				// 000022 = 4 up
				// 000100 = 1 none
				// 000101 = 2 none

				// 222222 = 18 up

				loop_i = 0;
				checksum = 0;
				while (loop_i < Check_keymax_for6point)
				{
					if (checkkey_state[loop_i] == 1) // downのままのキーが存在する。
						goto loop_next;
					checksum += checkkey_state[loop_i];
					loop_i++;
				}

				if (checksum == 0) // どのキーも押されていない。
				{
					goto loop_next;
				}

				// 0か2の状態のキーしか存在しない。
				//DBG_PRINT(_T(" detect all key up\n" ));

				// stateの初期化
				loop_i = 0;
				while (loop_i < Check_keymax_for6point)
				{
					checkkey_state[loop_i] = 0;
					loop_i++;
				}

				// dummy key down
				//DBG_PRINT(_T(" send dummy key down\n" ));
				kid.MakeCode = 0x50; // dummy keyは E1-0x50
				kid.Flags = KEYBOARD_INPUT_DATA::E1;
				SendtoKeyboardHandler(1, kid.Flags, kid.MakeCode);

				// dummy key up
				//DBG_PRINT(_T(" send dummy key up\n" ));
				kid.MakeCode = 0x50; // dummy keyは E1-0x50
				kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
				SendtoKeyboardHandler(1, kid.Flags, kid.MakeCode);

			loop_next:
				// 続いて、スレッドへの割り込みを待つ。
				switch (WaitForSingleObject(handle, dwWaitTime))
				{
				case WAIT_TIMEOUT: // timeoutした。
					break;

				case WAIT_OBJECT_0: // m_interruptThreadFor6pointEvent
					switch (m_interruptThreadFor6pointReason)
					{
					default:
						ASSERT(false);
						break;
					case InterruptThreadReason_Terminate:
						goto break_while;
						break;

					case InterruptThreadReason_Pause:
					{
						CHECK_TRUE(SetEvent(m_threadFor6pointEvent));
						while (WaitForMultipleObjects(1, &m_interruptThreadFor6pointEvent, FALSE, INFINITE) != WAIT_OBJECT_0)
						{
							switch (m_interruptThreadFor6pointReason)
							{
							case InterruptThreadReason_Terminate:
								goto break_while;
								break;
							case InterruptThreadReason_Resume:
								break;
							}
						}
						CHECK_TRUE(SetEvent(m_threadFor6pointEvent));
					}
					break;
					} // switch (m_interruptThreadFor6pointReason)
					break;
				} // switch(WaitForSingleObject)
			}	 // if m_seeting
			else
			{
				Sleep(dwWaitTime);
			}
	} // while
	ReleaseMutex(m_for6pointQueueMutex);

break_while:
	ReleaseMutex(m_for6pointQueueMutex);
	CHECK_TRUE(SetEvent(m_threadFor6pointEvent));
}

// TapHold timer thread
unsigned int WINAPI Engine::tapHoldTimer(void *i_this)
{
	reinterpret_cast<Engine *>(i_this)->tapHoldTimer();
	_endthreadex(0);
	return 0;
}
void Engine::tapHoldTimer()
{
	while (!m_doTapHoldForceTerminate)
	{
		DWORD r = WaitForSingleObject(m_tapHoldStartEvent, 500);
		if (m_doTapHoldForceTerminate) break;
		if (r != WAIT_OBJECT_0) continue;

		int threshold = m_tapHoldThreshold;
		if (threshold <= 0) threshold = 1;
		r = WaitForSingleObject(m_tapHoldCancelEvent, (DWORD)threshold);
		if (m_doTapHoldForceTerminate) break;
		if (r == WAIT_TIMEOUT)
			SetEvent(m_tapHoldExpiredEvent);
	}
}

// TapDance timer thread — inner loop handles tap-count restarts via m_tapDanceStartEvent
unsigned int WINAPI Engine::tapDanceTimer(void *i_this)
{
	reinterpret_cast<Engine *>(i_this)->tapDanceTimer();
	_endthreadex(0);
	return 0;
}
void Engine::tapDanceTimer()
{
	while (!m_doTapDanceForceTerminate)
	{
		DWORD r = WaitForSingleObject(m_tapDanceStartEvent, 500);
		if (m_doTapDanceForceTerminate) break;
		if (r != WAIT_OBJECT_0) continue;

		HANDLE waitHandles[2] = {m_tapDanceCancelEvent, m_tapDanceStartEvent};
		for (;;)
		{
			int timeout = m_tapDanceTimeout;
			if (timeout <= 0) timeout = 1;
			r = WaitForMultipleObjects(2, waitHandles, FALSE, (DWORD)timeout);
			if (m_doTapDanceForceTerminate) goto td_break;
			if (r == WAIT_OBJECT_0)     break;          // canceled → back to outer loop
			if (r == WAIT_OBJECT_0 + 1) continue;       // restart → reset timer
			if (r == WAIT_TIMEOUT)
			{
				SetEvent(m_tapDanceExpiredEvent);
				break;
			}
		}
	}
td_break:;
}

// Combo timer thread — inner loop handles window restarts via m_comboStartEvent
unsigned int WINAPI Engine::comboTimer(void *i_this)
{
	reinterpret_cast<Engine *>(i_this)->comboTimer();
	_endthreadex(0);
	return 0;
}
void Engine::comboTimer()
{
	while (!m_doComboForceTerminate)
	{
		DWORD r = WaitForSingleObject(m_comboStartEvent, 500);
		if (m_doComboForceTerminate) break;
		if (r != WAIT_OBJECT_0) continue;

		HANDLE waitHandles[2] = {m_comboCancelEvent, m_comboStartEvent};
		for (;;)
		{
			int window = m_comboWindow;
			if (window <= 0) window = 1;
			r = WaitForMultipleObjects(2, waitHandles, FALSE, (DWORD)window);
			if (m_doComboForceTerminate) goto co_break;
			if (r == WAIT_OBJECT_0)     break;          // canceled → back to outer loop
			if (r == WAIT_OBJECT_0 + 1) continue;       // restart window
			if (r == WAIT_TIMEOUT)
			{
				SetEvent(m_comboExpiredEvent);
				break;
			}
		}
	}
co_break:;
}

Engine::Engine(tomsgstream &i_log, int i_keyboard_hook, int i_mouse_hook, int i_win8wa)
	: m_hwndAssocWindow(NULL),
	  m_setting(NULL),
	  m_keyboard_hook(i_keyboard_hook),
	  m_mouse_hook(i_mouse_hook),
	  m_win8wa(i_win8wa),
	  m_device(INVALID_HANDLE_VALUE),
	  m_driverMutex(NULL),
	  m_driverMutexHeld(false),
	  m_didNodokaStartDevice(false),
	  m_stopped(false),
	  m_threadEvent(NULL),
	  m_threadCheckModifierEvent(NULL),
	  m_threadKeyboardPastEvent(NULL),
	  m_threadFor6pointEvent(NULL),
	  m_nodokadVersion(_T("Keyboard LL Hook Mode")),
	  m_buttonPressed(false),
	  m_dragging(false),
	  m_keyboardHandler(installKeyboardHook, Engine::keyboardDetour),
	  m_mouseHandler(installMouseHook, Engine::mouseDetour),
	  m_inputQueue(NULL),
	  m_readEvent(NULL),
	  m_reInjectEvent(NULL),
	  m_queueMutex(NULL),
	  m_pastQueueMutex(NULL),
	  m_interruptThreadEvent(NULL),
	  m_interruptThreadCheckModifierEvent(NULL),
	  m_interruptThreadKeyboardPastEvent(NULL),
	  m_interruptThreadFor6pointEvent(NULL),
	  m_sts4nodoka(NULL),
	  m_cts4nodoka(NULL),
	  m_ats4nodoka(NULL),
	  m_gamepad(NULL),
	  m_doForceTerminate(false),
	  m_doCheckModifierForceTerminate(false),
	  m_doKeyboardPastForceTerminate(false),
	  m_doFor6pointForceTerminate(false),
	  m_tapHoldThreadHandle(NULL),
	  m_tapHoldThreadId(0),
	  m_tapHoldStartEvent(NULL),
	  m_tapHoldCancelEvent(NULL),
	  m_tapHoldExpiredEvent(NULL),
	  m_doTapHoldForceTerminate(false),
	  m_tapHoldState(TH_IDLE),
	  m_tapHoldCurrentRule(NULL),
	  m_tapHoldThreshold(200),
	  m_tapHoldNeedPartSecond(false),
	  m_tapHoldHasBufferedKey(false),
	  m_tapHoldPermissiveOtherKey(NULL),
	  m_tapHoldLastTapKey(NULL),
	  m_tapHoldLastTapTime(0),
	  m_tapDanceThreadHandle(NULL),
	  m_tapDanceThreadId(0),
	  m_tapDanceStartEvent(NULL),
	  m_tapDanceCancelEvent(NULL),
	  m_tapDanceExpiredEvent(NULL),
	  m_doTapDanceForceTerminate(false),
	  m_tapDanceState(TD_IDLE),
	  m_tapDanceCurrentRule(NULL),
	  m_tapDanceCount(0),
	  m_tapDanceTimeout(300),
	  m_comboThreadHandle(NULL),
	  m_comboThreadId(0),
	  m_comboStartEvent(NULL),
	  m_comboCancelEvent(NULL),
	  m_comboExpiredEvent(NULL),
	  m_doComboForceTerminate(false),
	  m_comboState(CO_IDLE),
	  m_comboWindow(50),
	  m_comboDetectorMode(Setting::CD_TIMEOUT),
	  m_comboDetectorModePending(Setting::CD_TIMEOUT),
	  m_comboWindowPending(-1),
	  m_comboOverlapRatioPending(-1),
	  m_comboNestedAlwaysMatchPending(-1),
	  m_comboIdleThresholdPending(-1),
	  m_comboWindowActive(50),
	  m_comboOverlapRatioActive(0),
	  m_comboNestedAlwaysMatchActive(false),
	  m_comboIdleThresholdActive(0),
	  m_comboAllKeysDownTime(0),
	  m_comboAllKeysDown(false),
	  m_comboZeroLatencyActive(false),
	  m_isLogMode(false),
	  m_isEnabled(true),
	  m_iconColorNumber(0),
	  m_isSynchronizing(false),
	  m_eSync(NULL),
	  m_generateKeyboardEventsRecursionGuard(0),
	  m_currentKeyPressCount(0),
	  m_currentKeyPressCountOnWin32(0),
	  m_lastGeneratedKey(NULL),
	  m_oneShotRepeatableRepeatCount(0),
	  m_isPrefix(false),
	  m_currentKeymap(NULL),
	  m_currentFocusOfThread(NULL),
	  m_hwndFocus(NULL),
	  m_afShellExecute(NULL),
	  m_variable(0),
	  m_pastQueue(NULL),
	  m_log(i_log)
{
	// set LL hook mode
	g_hookDataExe->m_keyboard_hook = m_keyboard_hook;
	g_hookDataExe->m_mouse_hook = m_mouse_hook;
	g_hookDataExe->m_win8wa = m_win8wa;

	// initialize Current contexts for timer-based state machines
	m_tapHoldContext.m_keymap  = NULL;
	m_tapDanceContext.m_keymap = NULL;
	m_comboContext.m_keymap    = NULL;
	memset(&m_tapHoldKeyData,     0, sizeof(m_tapHoldKeyData));
	memset(&m_tapHoldBufferedKey, 0, sizeof(m_tapHoldBufferedKey));

	// m_lastPressedKey clear
	for (size_t i = 0; i < NUMBER_OF(m_lastPressedKey); ++i)
		m_lastPressedKey[i] = NULL;

	// initialize physical modifier press tracking (for syncModifiersFromGetAsyncKeyState)
	memset(m_physModFirstPressedTime, 0, sizeof(m_physModFirstPressedTime));
	memset(m_physModWasPressed,       0, sizeof(m_physModWasPressed));
	m_lastKeyEventTime = 0;
	m_prevKeyEventTime = 0;

	// set default lock state
	for (int i = 0; i < Modifier::Type_end; ++i)
		m_currentLock.dontcare(static_cast<Modifier::Type>(i));
	for (int i = Modifier::Type_Lock0; i <= Modifier::Type_LockF; ++i)
		m_currentLock.release(static_cast<Modifier::Type>(i));

	for (int i = Modifier::Type_TouchpadSticky; i <= Modifier::Type_TouchpadRSticky; ++i)
		m_currentLock.release(static_cast<Modifier::Type>(i));

	for (int i = Modifier::Type_Keyboard0; i <= Modifier::Type_Keyboard7; ++i)
		m_currentLock.release(static_cast<Modifier::Type>(i));

	m_currentLock.release(Modifier::Type_DP);

	if (m_setting && m_setting->m_UseUnitID == 1)
		m_currentLock.press(Modifier::Type_Keyboard0, true); // def option UnitID有効ならばデフォルトK0にしておく。

	if (!open())
	{
		g_hookDataExe->m_device = false;
		if (m_keyboard_hook == 0)
			throw ErrorMessage() << loadString(IDS_driverNotInstalled);
	}
	else
	{
		g_hookDataExe->m_device = true;

		TCHAR versionBuf[256];
		DWORD length = 0;

		if (DeviceIoControl(m_device, IOCTL_NODOKA_GET_VERSION, NULL, 0,
							versionBuf, sizeof(versionBuf), &length, NULL) &&
			length && length < sizeof(versionBuf)) // fail safe
			m_nodokadVersion = tstring(versionBuf, length / 2);
	}
	// create event for sync
	CHECK_TRUE(m_eSync = CreateEvent(NULL, FALSE, FALSE, NULL));
	// create named pipe for &SetImeString
	m_hookPipe = CreateNamedPipe(addSessionId(HOOK_PIPE_NAME).c_str(),
								 PIPE_ACCESS_OUTBOUND,
								 PIPE_TYPE_BYTE, 1,
								 0, 0, 0, NULL);

	StrExprArg::setEngine(this);
	m_msllHookCurrent.pt.x = 0;
	m_msllHookCurrent.pt.y = 0;
	m_msllHookCurrent.mouseData = 0;
	m_msllHookCurrent.flags = 0;
	m_msllHookCurrent.time = 0;
	m_msllHookCurrent.dwExtraInfo = 0;
}

// open nodoka device
bool Engine::open()
{
	OutputDebugString(_T("nodoka Engine::open() start\n"));
	if (m_keyboard_hook != 0)
	{
		m_device = INVALID_HANDLE_VALUE;
		OutputDebugString(_T("nodoka Engine::open() failed\n"));
		return false;
	}
	// open nodoka m_device
	m_device = CreateFile(NODOKA_DEVICE_FILE_NAME, GENERIC_READ | GENERIC_WRITE,
						  0, NULL, OPEN_EXISTING,
						  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);

	if (m_device != INVALID_HANDLE_VALUE)
	{
		OutputDebugString(_T("nodoka Engine::open() success\n"));
		return true;
	}
	{
		DWORD err0 = GetLastError();
		TCHAR buf[256];
		_stprintf_s(buf, _T("nodoka Engine::open() initial CreateFile failed err=%lu\n"), err0);
		OutputDebugString(buf);

		// STATUS_INTERNAL_ERROR (err=1359): nodokad はロード済みだが別プロセスが既に開いている。
		// リトライしても無意味なので即終了。
		if (err0 == ERROR_INTERNAL_ERROR)
		{
			OutputDebugString(_T("nodoka Engine::open() failed: device already open by another process\n"));
			return false;
		}

		// ERROR_FILE_NOT_FOUND (err=2) など: PnP スタック構築完了前に起動した race condition。
		// DIF_PROPERTYCHANGE は管理者権限が必要なため使用しない。
		// PnP による DriverEntry 完了を待ちながらリトライ（最大 6 秒）。
	}

	for (int i = 0; i < 30; i++)
	{
		Sleep(200);
		m_device = CreateFile(NODOKA_DEVICE_FILE_NAME, GENERIC_READ | GENERIC_WRITE,
			0, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
		if (m_device != INVALID_HANDLE_VALUE)
		{
			OutputDebugString(_T("nodoka Engine::open() success\n"));
			return true;
		}
		if (i == 0 || i == 14 || i == 29)
		{
			DWORD erri = GetLastError();
			TCHAR buf[128];
			_stprintf_s(buf, _T("nodoka Engine::open() retry[%d] CreateFile failed err=%lu\n"), i, erri);
			OutputDebugString(buf);
		}
	}
	OutputDebugString(_T("nodoka Engine::open() failed\n"));
	return false;
}

// close nodoka device
void Engine::close()
{
	if (m_device != INVALID_HANDLE_VALUE)
	{
		CHECK_TRUE(CloseHandle(m_device));
	}
	m_device = INVALID_HANDLE_VALUE;
}

// detect kbdaddid and retrieve its version from registry Parameters\Version
void Engine::detectKbdAddId()
{
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		_T("SYSTEM\\CurrentControlSet\\Services\\kbdaddid\\Parameters"),
		0, KEY_READ, &hKey) != ERROR_SUCCESS)
		return;

	DWORD license = 0;
	DWORD cbData = sizeof(DWORD);
	RegQueryValueEx(hKey, _T("License"), NULL, NULL, (LPBYTE)&license, &cbData);

	if (license == 1)
	{
		TCHAR version[64] = {};
		cbData = sizeof(version);
		DWORD regType = 0;
		if (RegQueryValueEx(hKey, _T("Version"), NULL, &regType, (LPBYTE)version, &cbData) == ERROR_SUCCESS
			&& regType == REG_SZ && version[0] != _T('\0'))
			m_kbdAddIdVersion = version;
		else
			m_kbdAddIdVersion = _T("(unknown)");
	}
	RegCloseKey(hKey);
}

// start keyboard handler thread
void Engine::start()
{
	if ((m_device == INVALID_HANDLE_VALUE) && (m_keyboard_hook != 0))
		m_keyboardHandler.start(this);

	// LL フックモードでのみスレッドローカル WH_KEYBOARD フックをインストール
	// ログウィンドウなど同プロセス uiAccess ウィンドウにフォーカスがあるとき
	// WH_KEYBOARD_LL が呼ばれない OS の動作（uiAccess=true の副作用）を補完する。
	// installKeyboardHook 完了後（m_keyboardHandler.start() の戻り後）に呼ぶこと。
	if (m_keyboard_hook == 1)
	{
		// コメントアウトすれば、ログウィンドウにフォーカスがあるときに詳細ログ出力が止まる
		installLocalKeyboardHook(GetCurrentThreadId(), true);
	}
	if (m_mouse_hook == 1)
		m_mouseHandler.start(this);

	CHECK_TRUE(m_inputQueue = new std::deque<KEYBOARD_INPUT_DATA>);
	CHECK_TRUE(m_pastQueue = new std::deque<KEYBOARD_PAST>);
	CHECK_TRUE(m_for6pointQueue = new std::deque<KEYBOARD_PAST>);

	CHECK_TRUE(m_queueMutex = CreateMutex(NULL, FALSE, NULL));
	CHECK_TRUE(m_pastQueueMutex = CreateMutex(NULL, FALSE, NULL));
	CHECK_TRUE(m_for6pointQueueMutex = CreateMutex(NULL, FALSE, NULL));
	m_driverMutex = CreateMutex(NULL, FALSE, _T("Global\\{46269F4D-D560-40f9-B38B-DB5E280FEF47}_driver"));

	CHECK_TRUE(m_threadEvent = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_readEvent = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_reInjectEvent = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_interruptThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL));

	m_ol.Offset = 0;
	m_ol.OffsetHigh = 0;
	m_ol.hEvent = m_readEvent;

	CHECK_TRUE(m_writeEvent = CreateEvent(NULL, FALSE, FALSE, NULL));
	m_writeOl.Offset = 0;
	m_writeOl.OffsetHigh = 0;
	m_writeOl.hEvent = m_writeEvent;

	// Create timer-feature events BEFORE starting keyboardHandler (they go into handles[])
	CHECK_TRUE(m_tapHoldStartEvent   = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_tapHoldCancelEvent  = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_tapHoldExpiredEvent = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_tapDanceStartEvent   = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_tapDanceCancelEvent  = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_tapDanceExpiredEvent = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_comboStartEvent   = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_comboCancelEvent  = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_comboExpiredEvent = CreateEvent(NULL, FALSE, FALSE, NULL));

	CHECK_TRUE(m_threadHandle = (HANDLE)_beginthreadex(NULL, 0, keyboardHandler, this, 0, &m_threadId));
	CHECK(WAIT_OBJECT_0 ==, WaitForSingleObject(m_threadEvent, INFINITE));

	// Start timer threads (after keyboardHandler is ready)
	m_doTapHoldForceTerminate  = false;
	m_doTapDanceForceTerminate = false;
	m_doComboForceTerminate    = false;
	CHECK_TRUE(m_tapHoldThreadHandle  = (HANDLE)_beginthreadex(NULL, 0, tapHoldTimer,  this, 0, &m_tapHoldThreadId));
	CHECK_TRUE(m_tapDanceThreadHandle = (HANDLE)_beginthreadex(NULL, 0, tapDanceTimer, this, 0, &m_tapDanceThreadId));
	CHECK_TRUE(m_comboThreadHandle    = (HANDLE)_beginthreadex(NULL, 0, comboTimer,    this, 0, &m_comboThreadId));

	// for CheckModifier thread
	CHECK_TRUE(m_threadCheckModifierEvent = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_interruptThreadCheckModifierEvent = CreateEvent(NULL, FALSE, FALSE, NULL));

	CHECK_TRUE(m_threadCheckModifierHandle = (HANDLE)_beginthreadex(NULL, 0, CheckModifier, this, 0, &m_threadCheckModifierId));
	CHECK(WAIT_OBJECT_0 ==, WaitForSingleObject(m_threadCheckModifierEvent, INFINITE));

	// for KeyboardPast thread
	CHECK_TRUE(m_threadKeyboardPastEvent = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_interruptThreadKeyboardPastEvent = CreateEvent(NULL, FALSE, FALSE, NULL));

	CHECK_TRUE(m_threadKeyboardPastHandle = (HANDLE)_beginthreadex(NULL, 0, KeyboardPast, this, 0, &m_threadKeyboardPastId));
	CHECK(WAIT_OBJECT_0 ==, WaitForSingleObject(m_threadKeyboardPastEvent, INFINITE));

	// for For6point thread
	//CHECK_TRUE( m_threadFor6pointEvent = CreateEvent(NULL, FALSE, FALSE, NULL) );
	//CHECK_TRUE( m_interruptThreadFor6pointEvent = CreateEvent(NULL, FALSE, FALSE, NULL) );

	//CHECK_TRUE( m_threadFor6pointHandle = (HANDLE)_beginthreadex(NULL, 0, For6point, this, 0, &m_threadFor6pointId) );
	//CHECK( WAIT_OBJECT_0 ==, WaitForSingleObject(m_threadFor6pointEvent, INFINITE) );
}

// stop keyboard handler thread
void Engine::stop()
{
	// stop() is called explicitly from ~Nodoka() and again from ~Engine() when
	// m_engine is destroyed afterward. Without this guard the second call touches
	// handles the first call already closed and NULLed (CloseHandle(NULL),
	// WaitForSingleObject(NULL, ...)), which is undefined behavior and can hang
	// or assert depending on build/runtime.
	if (m_stopped)
	{
		OutputDebugString(_T("nodoka Engine::stop() already stopped, skipping\n"));
		return;
	}
	m_stopped = true;

	/*
	if (m_threadFor6pointEvent)
	{
		m_doFor6pointForceTerminate = true;
		do {
			m_interruptThreadFor6pointReason = InterruptThreadReason_Terminate;
			SetEvent(m_interruptThreadFor6pointEvent);
			// wait for message handler thread terminate
		} while (WaitForSingleObject(m_threadFor6pointEvent, 100) != WAIT_OBJECT_0);

		CHECK_TRUE( CloseHandle(m_threadFor6pointEvent) );
		m_threadFor6pointEvent = NULL;
	}
	*/

	OutputDebugString(_T("nodoka Engine::stop() start\n"));

	// ローカルキーボードフックを最初にアンインストール
	if (m_keyboard_hook == 1)
		installLocalKeyboardHook(0, false);

	if (m_threadKeyboardPastEvent)
	{
		OutputDebugString(_T("nodoka Engine::stop() waiting KeyboardPast\n"));
		m_doKeyboardPastForceTerminate = true;
		do
		{
			m_interruptThreadKeyboardPastReason = InterruptThreadReason_Terminate;
			SetEvent(m_interruptThreadKeyboardPastEvent);
			// wait for message handler thread terminate
		} while (WaitForSingleObject(m_threadKeyboardPastEvent, 100) != WAIT_OBJECT_0);
		OutputDebugString(_T("nodoka Engine::stop() KeyboardPast done\n"));

		CHECK_TRUE(CloseHandle(m_threadKeyboardPastEvent));
		m_threadKeyboardPastEvent = NULL;
	}

	if (m_threadCheckModifierEvent)
	{
		OutputDebugString(_T("nodoka Engine::stop() waiting CheckModifier\n"));
		m_doCheckModifierForceTerminate = true;
		do
		{
			m_interruptThreadCheckModifierReason = InterruptThreadReason_Terminate;
			SetEvent(m_interruptThreadCheckModifierEvent);
			// wait for message handler thread terminate
		} while (WaitForSingleObject(m_threadCheckModifierEvent, 100) != WAIT_OBJECT_0);
		OutputDebugString(_T("nodoka Engine::stop() CheckModifier done\n"));

		CHECK_TRUE(CloseHandle(m_threadCheckModifierEvent));
		m_threadCheckModifierEvent = NULL;
	}

	// フック解除を先に行い、InputHandler スレッドを終了させてから keyboardHandler を終了する
	if (m_mouse_hook == 1)
		m_mouseHandler.stop();

	if ((m_device == INVALID_HANDLE_VALUE) && (m_keyboard_hook != 0))
		m_keyboardHandler.stop();

	// Terminate timer threads first (they use events that keyboardHandler watches)
	if (m_tapHoldThreadHandle)
	{
		m_doTapHoldForceTerminate = true;
		SetEvent(m_tapHoldStartEvent);   // wake thread from 500ms wait
		WaitForSingleObject(m_tapHoldThreadHandle, 1000);
		CloseHandle(m_tapHoldThreadHandle);
		m_tapHoldThreadHandle = NULL;
	}
	if (m_tapDanceThreadHandle)
	{
		m_doTapDanceForceTerminate = true;
		SetEvent(m_tapDanceStartEvent);
		WaitForSingleObject(m_tapDanceThreadHandle, 1000);
		CloseHandle(m_tapDanceThreadHandle);
		m_tapDanceThreadHandle = NULL;
	}
	if (m_comboThreadHandle)
	{
		m_doComboForceTerminate = true;
		SetEvent(m_comboStartEvent);
		WaitForSingleObject(m_comboThreadHandle, 1000);
		CloseHandle(m_comboThreadHandle);
		m_comboThreadHandle = NULL;
	}

	if (m_threadEvent)
	{
		OutputDebugString(_T("nodoka Engine::stop() waiting keyboardHandler\n"));
		m_doForceTerminate = true;

		// デバイスドライバモードでは CancelIo(m_device) が nodokad.sys の cancel ルーチンで
		// ブロックするため、先にデバイスハンドルをクローズして保留中の ReadFile IRP を
		// 強制キャンセルする。CloseHandle 後は m_device == INVALID_HANDLE_VALUE となるので
		// keyboardHandler の CancelIo ガードが機能する。
		if (m_device != INVALID_HANDLE_VALUE)
		{
			OutputDebugString(_T("nodoka Engine::stop() closing device handle to cancel pending ReadFile\n"));
			CloseHandle(m_device);
			m_device = INVALID_HANDLE_VALUE;
		}

		do
		{
			m_interruptThreadReason = InterruptThreadReason_Terminate;
			SetEvent(m_interruptThreadEvent);
			PostThreadMessage(m_threadId, WM_APP, 0, 0); // MsgWaitForMultipleObjects を確実に起こす
			// wait for message handler thread terminate
		} while (WaitForSingleObject(m_threadEvent, 100) != WAIT_OBJECT_0);
		OutputDebugString(_T("nodoka Engine::stop() keyboardHandler done\n"));

		CHECK_TRUE(CloseHandle(m_threadEvent));
		m_threadEvent = NULL;
	}

	/*
	WaitForSingleObject(m_for6pointQueueMutex, INFINITE);
	delete m_for6pointQueue;
	m_for6pointQueue = NULL;
	ReleaseMutex(m_for6pointQueueMutex);
	*/

	WaitForSingleObject(m_pastQueueMutex, INFINITE);
	delete m_pastQueue;
	m_pastQueue = NULL;
	ReleaseMutex(m_pastQueueMutex);

	WaitForSingleObject(m_queueMutex, INFINITE);
	delete m_inputQueue;
	m_inputQueue = NULL;
	ReleaseMutex(m_queueMutex);

	WaitForSingleObject(m_threadHandle, 100);
	CHECK_TRUE(CloseHandle(m_threadHandle));
	m_threadHandle = NULL;

	// stop nodokad
	if (m_didNodokaStartDevice)
	{
		SC_HANDLE hscm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
		if (hscm)
		{
			SC_HANDLE hs = OpenService(hscm, NODOKA_DRIVER_NAME, SERVICE_STOP);
			if (hs)
			{
				SERVICE_STATUS ss;
				ControlService(hs, SERVICE_CONTROL_STOP, &ss);
				CloseServiceHandle(hs);
			}
			CloseServiceHandle(hscm);
		}
	}

	CHECK_TRUE(CloseHandle(m_readEvent));
	m_readEvent = NULL;
	if (m_reInjectEvent) { CHECK_TRUE(CloseHandle(m_reInjectEvent)); m_reInjectEvent = NULL; }

	CHECK_TRUE(CloseHandle(m_writeEvent));
	m_writeEvent = NULL;

	CHECK_TRUE(CloseHandle(m_interruptThreadEvent));
	m_interruptThreadEvent = NULL;

	CHECK_TRUE(CloseHandle(m_interruptThreadCheckModifierEvent));
	m_interruptThreadCheckModifierEvent = NULL;

	CHECK_TRUE(CloseHandle(m_interruptThreadKeyboardPastEvent));
	m_interruptThreadKeyboardPastEvent = NULL;

	//CHECK_TRUE( CloseHandle(m_interruptThreadFor6pointEvent) );
	//m_interruptThreadFor6pointEvent = NULL;

	// Close timer-feature event handles
	if (m_tapHoldStartEvent)   { CloseHandle(m_tapHoldStartEvent);   m_tapHoldStartEvent   = NULL; }
	if (m_tapHoldCancelEvent)  { CloseHandle(m_tapHoldCancelEvent);  m_tapHoldCancelEvent  = NULL; }
	if (m_tapHoldExpiredEvent) { CloseHandle(m_tapHoldExpiredEvent); m_tapHoldExpiredEvent = NULL; }
	if (m_tapDanceStartEvent)   { CloseHandle(m_tapDanceStartEvent);   m_tapDanceStartEvent   = NULL; }
	if (m_tapDanceCancelEvent)  { CloseHandle(m_tapDanceCancelEvent);  m_tapDanceCancelEvent  = NULL; }
	if (m_tapDanceExpiredEvent) { CloseHandle(m_tapDanceExpiredEvent); m_tapDanceExpiredEvent = NULL; }
	if (m_comboStartEvent)   { CloseHandle(m_comboStartEvent);   m_comboStartEvent   = NULL; }
	if (m_comboCancelEvent)  { CloseHandle(m_comboCancelEvent);  m_comboCancelEvent  = NULL; }
	if (m_comboExpiredEvent) { CloseHandle(m_comboExpiredEvent); m_comboExpiredEvent = NULL; }
}

bool Engine::pause()
{
	SESSTRACE(m_log, _T("enter m_device=") << (m_device != INVALID_HANDLE_VALUE)
		<< _T(" keyboard_hook=") << m_keyboard_hook);
	/*
	do {
		m_interruptThreadFor6pointReason = InterruptThreadReason_Pause;
		SetEvent(m_interruptThreadFor6pointEvent);
	} while (WaitForSingleObject(m_threadFor6pointEvent, 2000) != WAIT_OBJECT_0);
	*/

	// Fix E-2: Close the driver device first to minimize the window during which
	// isOpen=1 on the login screen. CheckModifier and KeyboardPast threads do not
	// use m_device directly (they use SendtoKeyboardHandler via m_inputQueue), so
	// it is safe to close the device before pausing those threads.
	if ((m_device != INVALID_HANDLE_VALUE) && m_keyboard_hook == 0)
	{ // nodokad がopenされている。かつ Keyboard LL hook/RawInput hookが使われていないとき
		// pause()/resume() run on the UI message-loop thread (WM_WTSSESSION_CHANGE handler),
		// the same thread &Sync (WM_COPYDATA) and the tasktray icon clicks depend on.
		// A bounded wait here keeps a slow/stuck peer session from freezing this session's
		// message loop forever; m_driverMutexHeld tracks whether resume() must release it.
		SESSTRACE(m_log, _T("waiting for m_driverMutex"));
		m_driverMutexHeld = (WaitForSingleObject(m_driverMutex, 3000) == WAIT_OBJECT_0);
		SESSTRACE(m_log, _T("m_driverMutex wait done held=") << m_driverMutexHeld);
		if (!m_driverMutexHeld)
		{
			Acquire a(&m_log, 0);
			m_log << _T("pause(): m_driverMutex acquire timed out, proceeding without it") << std::endl;
		}
		SESSTRACE(m_log, _T("signalling InterruptThreadReason_Pause, waiting for ack"));
		do
		{
			m_interruptThreadReason = InterruptThreadReason_Pause;
			SetEvent(m_interruptThreadEvent);
		} while (WaitForSingleObject(m_threadEvent, 100) != WAIT_OBJECT_0);
		SESSTRACE(m_log, _T("InterruptThreadReason_Pause ack received, calling close()"));
		close();
		SESSTRACE(m_log, _T("close() returned"));
	}

	SESSTRACE(m_log, _T("signalling KeyboardPast Pause, waiting for ack"));
	do
	{
		m_interruptThreadKeyboardPastReason = InterruptThreadReason_Pause;
		SetEvent(m_interruptThreadKeyboardPastEvent);
	} while (WaitForSingleObject(m_threadKeyboardPastEvent, 2000) != WAIT_OBJECT_0);
	SESSTRACE(m_log, _T("KeyboardPast Pause ack received"));

	SESSTRACE(m_log, _T("signalling CheckModifier Pause, waiting for ack"));
	do
	{
		m_interruptThreadCheckModifierReason = InterruptThreadReason_Pause;
		SetEvent(m_interruptThreadCheckModifierEvent);
	} while (WaitForSingleObject(m_threadCheckModifierEvent, 2000) != WAIT_OBJECT_0);
	SESSTRACE(m_log, _T("CheckModifier Pause ack received"));

	SESSTRACE(m_log, _T("exit"));
	return true;
}

bool Engine::resume()
{
	SESSTRACE(m_log, _T("enter m_device=") << (m_device != INVALID_HANDLE_VALUE)
		<< _T(" keyboard_hook=") << m_keyboard_hook);
	if ((m_device == INVALID_HANDLE_VALUE) && (m_keyboard_hook == 0))
	{ // nodokad close済 かつ Keyboard LL Hook/RawInput hookが使われていないとき
		SESSTRACE(m_log, _T("calling open()"));
		if (!open())
		{
			SESSTRACE(m_log, _T("open() failed"));
			// Only release if pause() actually acquired it (see pause()'s timed wait).
			if (m_driverMutexHeld) { ReleaseMutex(m_driverMutex); m_driverMutexHeld = false; }
			return false; // FIXME
		}
		SESSTRACE(m_log, _T("open() succeeded, signalling InterruptThreadReason_Resume, waiting for ack"));
		do
		{
			m_interruptThreadReason = InterruptThreadReason_Resume;
			SetEvent(m_interruptThreadEvent);
		} while (WaitForSingleObject(m_threadEvent, 100) != WAIT_OBJECT_0);
		SESSTRACE(m_log, _T("InterruptThreadReason_Resume ack received, releasing m_driverMutex held=") << m_driverMutexHeld);
		if (m_driverMutexHeld) { ReleaseMutex(m_driverMutex); m_driverMutexHeld = false; }
	}

	/*
	do {
		m_interruptThreadFor6pointReason = InterruptThreadReason_Resume;
		SetEvent(m_interruptThreadFor6pointEvent);
	} while (WaitForSingleObject(m_threadFor6pointEvent, 2000) != WAIT_OBJECT_0);
	*/

	SESSTRACE(m_log, _T("signalling CheckModifier Resume, waiting for ack"));
	do
	{
		m_interruptThreadCheckModifierReason = InterruptThreadReason_Resume;
		SetEvent(m_interruptThreadCheckModifierEvent);
	} while (WaitForSingleObject(m_threadCheckModifierEvent, 2000) != WAIT_OBJECT_0);
	SESSTRACE(m_log, _T("CheckModifier Resume ack received"));

	SESSTRACE(m_log, _T("signalling KeyboardPast Resume, waiting for ack"));
	do
	{
		m_interruptThreadKeyboardPastReason = InterruptThreadReason_Resume;
		SetEvent(m_interruptThreadKeyboardPastEvent);
	} while (WaitForSingleObject(m_threadKeyboardPastEvent, 2000) != WAIT_OBJECT_0);
	SESSTRACE(m_log, _T("KeyboardPast Resume ack received"));

	SESSTRACE(m_log, _T("exit"));
	return true;
}

bool Engine::prepairQuit()
{
	// terminate and unload DLL for ThumbSense support if loaded
#ifndef _WIN64
	manageTs4nodoka(_T("sts4nodoka.dll"), _T("SynCOM.dll"), false, &m_sts4nodoka);
	manageTs4nodoka(_T("cts4nodoka.dll"), _T("TouchPad.dll"), false, &m_cts4nodoka);
	manageTs4nodoka(_T("ats4nodoka.dll"), _T("Vxdif.dll"), false, &m_ats4nodoka);
	manageTs4nodoka(_T("gamepad.dll"), _T("gamepad.dll"), false, &m_gamepad);
#else
	manageTs4nodoka(_T("sts4nodoka64.dll"), _T("SynCOM.dll"), false, &m_sts4nodoka);
	manageTs4nodoka(_T("ats4nodoka64.dll"), _T("Vxdif.dll"), false, &m_ats4nodoka);
	manageTs4nodoka(_T("gamepad64.dll"), _T("gamepad64.dll"), false, &m_gamepad);
#endif
	return true;
}

Engine::~Engine()
{
	stop();
	CHECK_TRUE(CloseHandle(m_eSync));
	if (m_driverMutex)
		CloseHandle(m_driverMutex);

	// close m_device
	close();

	// destroy named pipe for &SetImeString
	if (m_hookPipe && m_hookPipe != INVALID_HANDLE_VALUE)
	{
		DisconnectNamedPipe(m_hookPipe);
		CHECK_TRUE(CloseHandle(m_hookPipe));
	}
}

void Engine::manageTs4nodoka(TCHAR *i_ts4nodokaDllName, TCHAR *i_dependDllName, bool i_load, HMODULE *i_pTs4nodoka)
{
	Acquire a(&m_log, 0);
	typedef bool(__stdcall * FUNC_Term)();
	typedef bool(__stdcall * FUNC_Init)(UINT);
	if (i_load == false) // unload要求
	{
		if (*i_pTs4nodoka) // load済か?
		{
			FUNC_Term pTs4nodokaTerm = (FUNC_Term)GetProcAddress(*i_pTs4nodoka, "ts4nodokaTerm");
			if (pTs4nodokaTerm != NULL)
			{
				if (pTs4nodokaTerm() == true)   // DLLの終了処理
					FreeLibrary(*i_pTs4nodoka); // DLLのunload
				*i_pTs4nodoka = NULL;
				m_log << i_ts4nodokaDllName << _T(" unloaded") << std::endl;
			}
		}
	}
	else // load要求
	{
		if (*i_pTs4nodoka) // load済か?
		{
			m_log << i_ts4nodokaDllName << _T(" already loaded") << std::endl;
			FUNC_Term pTs4nodokaTerm = (FUNC_Term)GetProcAddress(*i_pTs4nodoka, "ts4nodokaTerm");
			if (pTs4nodokaTerm != NULL)
				pTs4nodokaTerm(); // DLLの終了処理

			FUNC_Init pTs4nodokaInit = (FUNC_Init)GetProcAddress(*i_pTs4nodoka, "ts4nodokaInit");
			if (pTs4nodokaInit != NULL)
			{
				if (pTs4nodokaInit(m_threadId) == true) // 初期化成功
					m_log << i_ts4nodokaDllName << _T(" Initialize") << std::endl;
				else // 初期化失敗
					m_log << i_ts4nodokaDllName
						  << _T(" load failed: can't initialize") << std::endl;
			}
			else
			{
				m_log << i_ts4nodokaDllName << _T(" init fail") << std::endl;
			}
		}
		else // loadされていない
		{
			if (SearchPath(NULL, i_dependDllName, NULL, 0, NULL, NULL) == 0) // DLLファイルが無い
			{
				m_log << _T("load ") << i_ts4nodokaDllName
					  << _T(" failed: can't find ") << i_dependDllName
					  << std::endl;
			}
			else // DLLファイルは存在したので loadにトライ
			{
				// WinVerifyTrust check: reject unsigned/tampered DLLs
				TCHAR fullPath[MAX_PATH] = {};
				bool sigOK = (SearchPath(NULL, i_ts4nodokaDllName, NULL, MAX_PATH, fullPath, NULL) > 0)
				             && verifyDllSignature(fullPath);
				if (!sigOK)
				{
					m_log << _T("[警告] ") << i_ts4nodokaDllName
					      << _T(" 署名検証失敗 - ロードをスキップ") << std::endl;
				}
				else
				{
				*i_pTs4nodoka = LoadLibrary(i_ts4nodokaDllName);
				if (*i_pTs4nodoka == NULL) // loadに失敗
				{
					m_log << _T("load ") << i_ts4nodokaDllName
						  << _T(" failed: can't find it") << std::endl;
				}
				else // loadに成功したので、初期化を実施
				{
					FUNC_Init pTs4nodokaInit = (FUNC_Init)GetProcAddress(*i_pTs4nodoka, "ts4nodokaInit");
					if (pTs4nodokaInit != NULL)
					{
						if (pTs4nodokaInit(m_threadId) == true) //初期化成功
							m_log << i_ts4nodokaDllName << _T(" load & Initialize") << std::endl;
						else //初期化失敗
							m_log << i_ts4nodokaDllName
								  << _T(" load failed: can't initialize") << std::endl;
					}
					else // 初期化エントリが無く、失敗
					{
						m_log << i_ts4nodokaDllName << _T(" init fail") << std::endl;
					}
				}
				} // end sigOK
			}
		}
	}
}

// set m_setting
bool Engine::setSetting(Setting *i_setting)
{
	Acquire a(&m_cs);
	if (m_isSynchronizing)
		return false;

	if (m_setting)
	{
		for (Keyboard::KeyIterator i = m_setting->m_keyboard.getKeyIterator();
			 *i; ++i)
		{
			Key *key = i_setting->m_keyboard.searchKey(*(*i));
			if (key)
			{
				key->m_isPressed = (*i)->m_isPressed;
				key->m_isPressedOnWin32 = (*i)->m_isPressedOnWin32;
				key->m_isPressedByAssign = (*i)->m_isPressedByAssign;
			}
		}
		if (m_lastGeneratedKey)
			m_lastGeneratedKey =
				i_setting->m_keyboard.searchKey(*m_lastGeneratedKey);
		for (size_t i = 0; i < NUMBER_OF(m_lastPressedKey); ++i)
			if (m_lastPressedKey[i])
				m_lastPressedKey[i] =
					i_setting->m_keyboard.searchKey(*m_lastPressedKey[i]);
	}

	m_setting = i_setting;

	// Reset combo pending fields from setting on reload
	m_comboDetectorModePending       = i_setting->m_ComboDetectorMode;
	m_comboWindowPending             = -1;
	m_comboOverlapRatioPending       = -1;
	m_comboNestedAlwaysMatchPending  = -1;
	m_comboIdleThresholdPending      = -1;
	if (m_comboState == CO_IDLE)
	{
		m_comboDetectorMode             = m_comboDetectorModePending;
		m_comboWindowActive             = i_setting->m_ComboWindow;
		m_comboOverlapRatioActive       = i_setting->m_ComboOverlapRatio;
		m_comboNestedAlwaysMatchActive  = i_setting->m_ComboNestedAlwaysMatch;
		m_comboIdleThresholdActive      = i_setting->m_ComboIdleThreshold;
	}

#ifndef _WIN64
	manageTs4nodoka(_T("sts4nodoka.dll"), _T("SynCOM.dll"), m_setting->m_sts4nodoka, &m_sts4nodoka);
	manageTs4nodoka(_T("cts4nodoka.dll"), _T("TouchPad.dll"), m_setting->m_cts4nodoka, &m_cts4nodoka);
	manageTs4nodoka(_T("ats4nodoka.dll"), _T("Vxdif.dll"), m_setting->m_ats4nodoka, &m_ats4nodoka);
	manageTs4nodoka(_T("gamepad.dll"), _T("gamepad.dll"), m_setting->m_gamepad, &m_gamepad);
#else
	manageTs4nodoka(_T("sts4nodoka64.dll"), _T("SynCOM.dll"), m_setting->m_sts4nodoka, &m_sts4nodoka);
	manageTs4nodoka(_T("ats4nodoka64.dll"), _T("Vxdif.dll"), m_setting->m_ats4nodoka, &m_ats4nodoka);
	manageTs4nodoka(_T("gamepad64.dll"), _T("gamepad64.dll"), m_setting->m_gamepad, &m_gamepad);
#endif

	g_hookDataExe->m_correctKanaLockHandling = m_setting->m_correctKanaLockHandling;
	g_hookDataExe->m_CaretBlinkTime = m_setting->m_CaretBlinkTime;
	g_hookDataExe->m_BlinkTimeOff = m_setting->m_BlinkTimeOff;
	g_hookDataExe->m_BlinkTimeOn = m_setting->m_BlinkTimeOn;
	g_hookDataExe->m_UseTSF = m_setting->m_UseTSF;

	if (m_currentFocusOfThread)
	{
		for (FocusOfThreads::iterator i = m_focusOfThreads.begin();
			 i != m_focusOfThreads.end(); i++)
		{
			FocusOfThread *fot = &(*i).second;
			m_setting->m_keymaps.searchWindow(&fot->m_keymaps,
											  fot->m_className, fot->m_titleName);
		}
	}
	m_setting->m_keymaps.searchWindow(&m_globalFocus.m_keymaps, _T(""), _T(""));
	if (m_globalFocus.m_keymaps.empty())
	{
		Acquire a(&m_log, 0);
		m_log << _T("internal error: m_globalFocus.m_keymap is empty")
			  << std::endl;
	}
	m_currentFocusOfThread = &m_globalFocus;
	setCurrentKeymap(m_globalFocus.m_keymaps.front());
	m_hwndFocus = NULL;
	return true;
}

void Engine::checkShow(HWND i_hwnd)
{
	// update show style of window
	// this update should be done in hook DLL, but to
	// avoid update-loss for some applications(such as
	// cmd.exe), we update here.
	bool isMaximized = false;
	bool isMinimized = false;
	bool isMDIMaximized = false;
	bool isMDIMinimized = false;
	while (i_hwnd)
	{
		LONG_PTR exStyle = GetWindowLongPtr(i_hwnd, GWL_EXSTYLE);
		if (exStyle & WS_EX_MDICHILD)
		{
			WINDOWPLACEMENT placement;
			placement.length = sizeof(WINDOWPLACEMENT);
			if (GetWindowPlacement(i_hwnd, &placement))
			{
				switch (placement.showCmd)
				{
				case SW_SHOWMAXIMIZED:
					isMDIMaximized = true;
					break;
				case SW_SHOWMINIMIZED:
					isMDIMinimized = true;
					break;
				case SW_SHOWNORMAL:
				default:
					break;
				}
			}
		}

		LONG_PTR style = GetWindowLongPtr(i_hwnd, GWL_STYLE);
		if ((style & WS_CHILD) == 0)
		{
			WINDOWPLACEMENT placement;
			placement.length = sizeof(WINDOWPLACEMENT);
			if (GetWindowPlacement(i_hwnd, &placement))
			{
				switch (placement.showCmd)
				{
				case SW_SHOWMAXIMIZED:
					isMaximized = true;
					break;
				case SW_SHOWMINIMIZED:
					isMinimized = true;
					break;
				case SW_SHOWNORMAL:
				default:
					break;
				}
			}
		}
		i_hwnd = GetParent(i_hwnd);
	}
	setShow(isMDIMaximized, isMDIMinimized, true);
	setShow(isMaximized, isMinimized, false);
}

// focus
bool Engine::setFocus(HWND i_hwndFocus, DWORD i_threadId,
					  const tstringi &i_className, const tstringi &i_titleName,
					  bool i_isConsole)
{
	Acquire a(&m_cs);
	if (m_isSynchronizing)
		return false;
	if (i_hwndFocus == NULL)
		return true;

	// remove newly created thread's id from m_detachedThreadIds
	if (!m_detachedThreadIds.empty())
	{
		DetachedThreadIds::iterator i;
		bool retry;
		do
		{
			retry = false;
			for (i = m_detachedThreadIds.begin();
				 i != m_detachedThreadIds.end(); ++i)
				if (*i == i_threadId)
				{
					m_detachedThreadIds.erase(i);
					retry = true;
					break;
				}
		} while (retry);
	}

	FocusOfThread *fot;
	FocusOfThreads::iterator i = m_focusOfThreads.find(i_threadId);
	if (i != m_focusOfThreads.end())
	{
		fot = &(*i).second;
		if (fot->m_hwndFocus == i_hwndFocus &&
			fot->m_isConsole == i_isConsole &&
			fot->m_className == i_className &&
			fot->m_titleName == i_titleName)
			return true;
	}
	else
	{
		i = m_focusOfThreads.insert(
								FocusOfThreads::value_type(i_threadId, FocusOfThread()))
				.first;
		fot = &(*i).second;
		fot->m_threadId = i_threadId;
	}
	fot->m_hwndFocus = i_hwndFocus;
	fot->m_isConsole = i_isConsole;
	fot->m_className = i_className;
	fot->m_titleName = i_titleName;

	if (m_setting)
	{
		m_setting->m_keymaps.searchWindow(&fot->m_keymaps,
										  i_className, i_titleName);
		ASSERT(0 < fot->m_keymaps.size());
	}
	else
		fot->m_keymaps.clear();
	checkShow(i_hwndFocus);
	return true;
}

// lock state
bool Engine::setLockState(bool i_isNumLockToggled,
						  bool i_isCapsLockToggled,
						  bool i_isScrollLockToggled,
						  bool i_isKanaLockToggled,
						  bool i_isImeLockToggled,
						  bool i_isImeCompToggled,
						  bool i_isCandidateWindow)
{
	Acquire a(&m_cs);
	if (m_isSynchronizing)
		return false;

	m_currentLock.on(Modifier::Type_NumLock, i_isNumLockToggled);
	m_currentLock.on(Modifier::Type_CapsLock, i_isCapsLockToggled);
	m_currentLock.on(Modifier::Type_ScrollLock, i_isScrollLockToggled);
	m_currentLock.on(Modifier::Type_KanaLock, i_isKanaLockToggled);
	m_currentLock.on(Modifier::Type_ImeLock, i_isImeLockToggled);
	m_currentLock.on(Modifier::Type_ImeComp, i_isImeCompToggled);
	m_currentLock.on(Modifier::Type_ImeCandi, i_isCandidateWindow);

	if (m_setting)
		if (m_setting->m_UseTSF)
			if (!i_isImeLockToggled) // UseTSF W.A. IME OFFなら各Modifierを落とす
			{
				//m_currentLock.off(Modifier::Type_KanaLock);		// KL- KANAはそのまま
				m_currentLock.off(Modifier::Type_Harf); // IH-
				//m_currentLock.off(Modifier::Type_Katakana);		// IK- KANAはそのまま
				m_currentLock.off(Modifier::Type_Native); // IJ-
			}
	/*
	Acquire b(&m_log, 1);

	m_log << _T(" NL: ") << (i_isNumLockToggled ? _T("1") : _T("0"));
	m_log << _T(" CL: ") << (i_isCapsLockToggled ? _T("1") : _T("0"));
	m_log << _T(" SL: ") << (i_isScrollLockToggled ? _T("1") : _T("0"));
	m_log << _T(" KL: ") << (i_isKanaLockToggled ? _T("1") : _T("0"));
	m_log << _T(" IL: ") << (i_isImeLockToggled ? _T("1") : _T("0"));
	m_log << _T(" IC: ") << (i_isImeCompToggled ? _T("1") : _T("0"));
	m_log << _T(" IW: ") << (i_isCandidateWindow ? _T("1") : _T("0"));
	m_log << _T(" IH: ") << (i_isHarf ? _T("1") : _T("0"));
	m_log << _T(" IK: ") << (i_isKatakana ? _T("1") : _T("0"));
	m_log << _T(" IJ: ") << (i_isNative ? _T("1") : _T("0"));
	m_log << std::endl;
	*/

	return true;
}

bool Engine::setLockState2()
{
	Acquire a(&m_cs);
	if (m_isSynchronizing)
		return false;

	if (m_setting)
		if (m_setting->m_UseTSF)
			if (g_hookDataExe->pCv != NULL)
			{
				BOOL i_isImeLockToggled = FALSE;
				BOOL i_isKanaLockToggled = FALSE;
				BOOL i_isHarf = FALSE;
				BOOL i_isKatakana = FALSE;
				BOOL i_isNative = FALSE;

				if (g_hookDataExe->pCv->m_supportTsfOpenClose)
				{
					i_isImeLockToggled = (g_hookDataExe->pCv->m_bImeStatus);
					m_currentLock.press(Modifier::Type_ImeLock, !!i_isImeLockToggled); // IL-
				}
				if (g_hookDataExe->pCv->m_supportTsfConversion)
				{
					if (i_isImeLockToggled)
					{
						i_isKanaLockToggled = ((g_hookDataExe->pCv->m_conversion) & IME_CMODE_ROMAN) ? FALSE : TRUE; // KL- ,!ROMAN
						i_isHarf = ((g_hookDataExe->pCv->m_conversion) & IME_CMODE_FULLSHAPE) ? FALSE : TRUE;		 // IH- ,!FULL
						i_isKatakana = ((g_hookDataExe->pCv->m_conversion) & IME_CMODE_KATAKANA) ? TRUE : FALSE;	 // IK-
						i_isNative = ((g_hookDataExe->pCv->m_conversion) & IME_CMODE_NATIVE) ? TRUE : FALSE;		 // IJ-
					}
					m_currentLock.press(Modifier::Type_KanaLock, !!i_isKanaLockToggled);
					m_currentLock.press(Modifier::Type_Harf, !!i_isHarf);
					m_currentLock.press(Modifier::Type_Katakana, !!i_isKatakana);
					m_currentLock.press(Modifier::Type_Native, !!i_isNative);
				}
			}

	return true;
}

bool Engine::setLockState2A(DWORD dwComversion) // not use.
{
	Acquire a(&m_cs);
	if (m_isSynchronizing)
		return false;

	if (m_setting)
		if (m_setting->m_UseTSF)
		{
			BOOL i_isKanaLockToggled = FALSE;
			BOOL i_isHarf = FALSE;
			BOOL i_isKatakana = FALSE;
			BOOL i_isNative = FALSE;

			if (g_hookDataExe->pCv->m_supportTsfConversion)
			{
				i_isKanaLockToggled = (dwComversion & IME_CMODE_ROMAN) ? FALSE : TRUE; // KL- ,!ROMAN
				i_isHarf = (dwComversion & IME_CMODE_FULLSHAPE) ? FALSE : TRUE;		   // IH- ,!FULL
				i_isKatakana = (dwComversion & IME_CMODE_KATAKANA) ? TRUE : FALSE;	 // IK-
				i_isNative = (dwComversion & IME_CMODE_NATIVE) ? TRUE : FALSE;		   // IJ-

				m_currentLock.press(Modifier::Type_KanaLock, !!i_isKanaLockToggled);
				m_currentLock.press(Modifier::Type_Harf, !!i_isHarf);
				m_currentLock.press(Modifier::Type_Katakana, !!i_isKatakana);
				m_currentLock.press(Modifier::Type_Native, !!i_isNative);
			}
		}

	return true;
}

bool Engine::setLockState2B(bool bComposition)
{
	Acquire a(&m_cs);
	if (m_isSynchronizing)
		return false;

	if (m_setting)
		if (m_setting->m_UseTSF)
		{
			m_currentLock.press(Modifier::Type_ImeComp, bComposition); // IC-
		}
	return true;
}
bool Engine::setLockState3(UINT UnitID)
{
	// WM_INPUT(nodoka.cpp)で、どのキーボードから来たかでKxモディファイヤーを変更する。
	Acquire a(&m_cs);
	if (m_isSynchronizing)
		return false;

	// とりあえず初期化
	for (int i = Modifier::Type_Keyboard0; i <= Modifier::Type_Keyboard7; ++i)
		m_currentLock.press(static_cast<Modifier::Type>(i), false);

	// Modifier K0-からK7-処理
	int i = Modifier::Type_Keyboard0 + UnitID;
	m_currentLock.press(static_cast<Modifier::Type>(i), true);

	return true;
}

bool Engine::setLockState4(bool bDoublePress)
{
	Acquire a(&m_cs);
	if (m_isSynchronizing)
		return false;

	Acquire b(&m_log, 1);
	m_currentLock.press(Modifier::Type_DP, bDoublePress); // DP-
	return true;
}

// show
bool Engine::setShow(bool i_isMaximized, bool i_isMinimized,
					 bool i_isMDI)
{
	SESSTRACE(m_log, _T("waiting for m_cs"));
	Acquire a(&m_cs);
	SESSTRACE(m_log, _T("acquired m_cs"));
	if (m_isSynchronizing)
	{
		SESSTRACE(m_log, _T("m_isSynchronizing, returning false"));
		return false;
	}
	Acquire b(&m_log, 1);
	Modifier::Type max, min;
	if (i_isMDI == true)
	{
		max = Modifier::Type_MdiMaximized;
		min = Modifier::Type_MdiMinimized;
	}
	else
	{
		max = Modifier::Type_Maximized;
		min = Modifier::Type_Minimized;
	}
	m_currentLock.on(max, i_isMaximized);
	m_currentLock.on(min, i_isMinimized);
	m_log << _T("Set show to ") << (i_isMaximized ? _T("Maximized") : i_isMinimized ? _T("Minimized") : _T("Normal"));
	if (i_isMDI == true)
	{
		m_log << _T(" (MDI)");
	}
	m_log << std::endl;
	return true;
}

// sync
bool Engine::syncNotify()
{
	Acquire a(&m_cs);
	if (!m_isSynchronizing)
	{
		// diagnostic: notification arrived while no &Sync wait was pending
		// (e.g. a delayed/duplicate detection from the hook side).
		Acquire b(&m_log, 0);
		m_log << _T("syncNotify(): stale (m_isSynchronizing == false)") << std::endl;
		return false;
	}
	CHECK_TRUE(SetEvent(m_eSync));
	return true;
}

// thread detach notify
bool Engine::threadDetachNotify(DWORD i_threadId)
{
	Acquire a(&m_cs);
	m_detachedThreadIds.push_back(i_threadId);

	//Acquire b(&m_log, 1);
	//m_log << _T("m_detachedThreadIds.push_back ") << i_threadId << std::endl;

	return true;
}

// get help message
void Engine::getHelpMessages(tstring *o_helpMessage, tstring *o_helpTitle)
{
	Acquire a(&m_cs);
	*o_helpMessage = m_helpMessage;
	*o_helpTitle = m_helpTitle;
}

unsigned int WINAPI Engine::InputHandler::run(void *i_this)
{
	reinterpret_cast<InputHandler *>(i_this)->run();
	_endthreadex(0);
	return 0;
}

Engine::InputHandler::InputHandler(INSTALL_HOOK i_installHook, INPUT_DETOUR i_inputDetour)
	: m_installHook(i_installHook), m_inputDetour(i_inputDetour)
{
	CHECK_TRUE(m_hEventLL = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_hThread = (HANDLE)_beginthreadex(NULL, 0, run, this, CREATE_SUSPENDED, &m_threadId));
}

Engine::InputHandler::~InputHandler()
{
	CloseHandle(m_hEventLL);
}

void Engine::InputHandler::run()
{
	MSG msg;

	CHECK_FALSE(m_installHook(m_inputDetour, m_engine, true));
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
	SetEvent(m_hEventLL);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		// nothing to do...
	}

	CHECK_FALSE(m_installHook(m_inputDetour, m_engine, false));

	return;
}

int Engine::InputHandler::start(Engine *i_engine)
{
	m_engine = i_engine;
	ResumeThread(m_hThread);
	WaitForSingleObject(m_hEventLL, INFINITE);
	return 0;
}

int Engine::InputHandler::stop()
{
	PostThreadMessage(m_threadId, WM_QUIT, 0, 0);
	WaitForSingleObject(m_hThread, INFINITE);
	return 0;
}

// command notify
void Engine::commandNotify(HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam)
{
	Acquire b(&m_log, 0);
	HWND hf = m_hwndFocus;
	if (!hf)
		return;

	if (GetWindowThreadProcessId(hf, NULL) ==
		GetWindowThreadProcessId(m_hwndAssocWindow, NULL))
		return; // inhibit the investigation of NODOKA

	const _TCHAR *target = NULL;
	int number_target = 0;

	if (i_hwnd == hf)
		target = _T("ToItself");
	else if (i_hwnd == GetParent(hf))
		target = _T("ToParentWindow");
	else
	{
		// Function::toMainWindow
		HWND h = hf;
		while (true)
		{
			HWND p = GetParent(h);
			if (!p)
				break;
			h = p;
		}
		if (i_hwnd == h)
			target = _T("ToMainWindow");
		else
		{
			// Function::toOverlappedWindow
			HWND h = hf;
			while (h)
			{
				LONG_PTR style = GetWindowLongPtr(h, GWL_STYLE);
				if ((style & WS_CHILD) == 0)
					break;
				h = GetParent(h);
			}
			if (i_hwnd == h)
				target = _T("ToOverlappedWindow");
			else
			{
				// number
				HWND h = hf;
				for (number_target = 0; h; number_target++, h = GetParent(h))
					if (i_hwnd == h)
						break;
				return;
			}
		}
	}

	m_log << _T("&PostMessage(");
	if (target)
		m_log << target;
	else
		m_log << number_target;
	m_log << _T(", ") << i_message
		  << _T(", 0x") << std::hex << i_wParam
		  << _T(", 0x") << i_lParam << _T(") # hwnd = ")
		  << reinterpret_cast<DWORD_PTR>(i_hwnd) << _T(", ")
		  << _T("message = ") << std::dec;
	if (i_message == WM_COMMAND)
		m_log << _T("WM_COMMAND, ");
	else if (i_message == WM_SYSCOMMAND)
		m_log << _T("WM_SYSCOMMAND, ");
	else
		m_log << i_message << _T(", ");
	m_log << _T("wNotifyCode = ") << HIWORD(i_wParam) << _T(", ")
		  << _T("wID = ") << LOWORD(i_wParam) << _T(", ")
		  << _T("hwndCtrl = 0x") << std::hex << i_lParam << std::dec << std::endl;
}
