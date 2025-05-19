//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// engine.cpp

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

#include <iomanip>
#include <imm.h>
#include <process.h>

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
						m_log << _T("\tHWND:\t") << std::hex << (DWORD)fot->m_hwndFocus
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
						  << std::hex << (DWORD)m_currentFocusOfThread->m_hwndFocus
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
				m_log << _T("HWND:\t") << std::hex << reinterpret_cast<DWORD>(hwndFore)
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
			WriteFile(m_device, i_kid, sizeof(*i_kid), &m_len, &m_ol);
			CHECK_TRUE(GetOverlappedResult(m_device, &m_ol, &m_len, TRUE));
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

			if (m_keyboard_hook == 2 && !(g_hookDataExe->m_RDP))
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

// Post to KeyboardHandler()
void Engine::SendtoKeyboardHandler(int mode, int flag, USHORT MakeCode)
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
		kid.ExtraInformation = 0;

		m_inputQueue->push_back(kid);
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

		SendtoKeyboardHandler(1, kid.Flags, kid.MakeCode);

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
			HWND target = reinterpret_cast<HWND>(g_hookDataExe->m_hwndMouseHookTarget);

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
	HANDLE handles[] = {m_readEvent, m_interruptThreadEvent};

	// initialize ok
	CHECK_TRUE(SetEvent(m_threadEvent));
	PeekMessage(&message, NULL, 0, 0, PM_NOREMOVE); // メッセージキューの生成

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

	// loop
	while (!m_doForceTerminate)
	{
	start_loop:
		// for LL Hook, raw input Hook,  Mouse Hook
		if (m_setting)
			if ((m_keyboard_hook == 1) || (m_keyboard_hook == 2) || (m_mouse_hook == 1) || (m_isEnabled && m_setting->m_CheckModifier) || (m_isEnabled && m_setting->m_For6point))
			{
				// m_queueMutexを確認し、m_inputQueueからデータを取り出す。
				switch (WaitForSingleObject(m_queueMutex, 5))
				{
				case WAIT_OBJECT_0:
					if ((m_inputQueue != NULL) && !(m_inputQueue->empty()))
					{
						kid = m_inputQueue->front();
						m_inputQueue->pop_front();
						ReleaseMutex(m_queueMutex);
						goto quit_loop;
					}
					break;
				default:
					break;
				}
				ReleaseMutex(m_queueMutex);
			}

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

		switch (MsgWaitForMultipleObjects(NUMBER_OF(handles), &handles[0], FALSE, dwWaitTime, QS_POSTMESSAGE))
		{
		case WAIT_TIMEOUT: // timeoutしたのでcontinueして戻る。
			bbi = false;   // ReadFile()は完了していないので、次回 ReadFile()を実行させない。
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
			{
				//bbi = true;		// ここで trueにしてはいけない。
				continue; //goto rewait;		// Repeat不要だったので、Eventを待つ。
			}
			break;
		case WAIT_OBJECT_0: // m_readEvent
			bbi = true;		// ReadFile()を実行させる。
			if (!GetOverlappedResult(m_device, &m_ol, &len, FALSE))
				continue; // retry ReadFile()
			break;		  // switch/case break;

		case WAIT_OBJECT_0 + 1: // m_interruptThreadEvent
			CancelIo(m_device);
			switch (m_interruptThreadReason)
			{
			default:
			{
				ASSERT(false);
				break;
			}

			case InterruptThreadReason_Terminate:
				goto break_while;
			case InterruptThreadReason_Pause:
			{
				CHECK_TRUE(SetEvent(m_threadEvent));
				while (WaitForMultipleObjects(1, &m_interruptThreadEvent, FALSE, INFINITE) != WAIT_OBJECT_0)
					;
				switch (m_interruptThreadReason)
				{
				case InterruptThreadReason_Terminate:
					goto break_while;

				case InterruptThreadReason_Resume:
					break;

				default:
					ASSERT(false);
					break;
				}
				CHECK_TRUE(SetEvent(m_threadEvent));
				break;
			}
			}
			break;
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

		Acquire a(&m_cs);

		if (!m_currentFocusOfThread ||
			!m_currentKeymap)
		{
			injectInputOut(&kid);

			Acquire a(&m_log, 1);
			if (!m_currentFocusOfThread)
				m_log << _T("internal error: m_currentFocusOfThread == NULL")
					  << std::endl;
			if (!m_currentKeymap)
				m_log << _T("internal error: m_currentKeymap == NULL")
					  << std::endl;
			updateLastPressedKey(NULL);
			continue;
		}

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
				continue;
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
			outputToLog(&key, c.m_mkey, 1);
			if (isPhysicallyPressed) // もし他のキーが押されたなら oneShotは無効にする。
			{
				m_oneShotKey.m_key = NULL;
				{
					Acquire a(&m_log, 1);
					m_log << _T("one shot modifier is NULL") << std::endl;
				}
			}
			//			beginGeneratingKeyboardEvents(c, isModifier, m_oneShot2);
			beginGeneratingKeyboardEvents(c, isModifier, false);
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

	CHECK_TRUE(SetEvent(m_threadEvent));
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

			case InterruptThreadReason_Pause:
			{
				CHECK_TRUE(SetEvent(m_threadCheckModifierEvent));

				while (WaitForSingleObject(handle, INFINITE) != WAIT_OBJECT_0)
					switch (m_interruptThreadCheckModifierReason)
					{
					case InterruptThreadReason_Terminate:
						goto break_while;

					case InterruptThreadReason_Resume:
						break;
					}
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
			case InterruptThreadReason_Pause:
			{
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

Engine::Engine(tomsgstream &i_log, int i_keyboard_hook, int i_mouse_hook, int i_win8wa)
	: m_hwndAssocWindow(NULL),
	  m_setting(NULL),
	  m_keyboard_hook(i_keyboard_hook),
	  m_mouse_hook(i_mouse_hook),
	  m_win8wa(i_win8wa),
	  m_device(INVALID_HANDLE_VALUE),
	  m_didNodokaStartDevice(false),
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

	// m_lastPressedKey clear
	for (size_t i = 0; i < NUMBER_OF(m_lastPressedKey); ++i)
		m_lastPressedKey[i] = NULL;

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
	if (m_keyboard_hook != 0)
	{
		m_device = INVALID_HANDLE_VALUE;
		return false;
	}
	// open nodoka m_device
	m_device = CreateFile(NODOKA_DEVICE_FILE_NAME, GENERIC_READ | GENERIC_WRITE,
						  0, NULL, OPEN_EXISTING,
						  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);

	if (m_device != INVALID_HANDLE_VALUE)
	{
		return true;
	}

	// start nodokad
	SC_HANDLE hscm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (hscm)
	{
		SC_HANDLE hs = OpenService(hscm, NODOKA_DRIVER_NAME, SERVICE_START);
		if (hs)
		{
			StartService(hs, 0, NULL);
			CloseServiceHandle(hs);
			m_didNodokaStartDevice = true;
		}
		CloseServiceHandle(hscm);
	}

	/*
	// open nodoka m_device
	m_device = CreateFile(NODOKA_DEVICE_FILE_NAME, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
	*/
	return (m_device != INVALID_HANDLE_VALUE);
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

// start keyboard handler thread
void Engine::start()
{
	if ((m_device == INVALID_HANDLE_VALUE) && (m_keyboard_hook != 0))
		m_keyboardHandler.start(this);

	if (m_mouse_hook == 1)
		m_mouseHandler.start(this);

	CHECK_TRUE(m_inputQueue = new std::deque<KEYBOARD_INPUT_DATA>);
	CHECK_TRUE(m_pastQueue = new std::deque<KEYBOARD_PAST>);
	CHECK_TRUE(m_for6pointQueue = new std::deque<KEYBOARD_PAST>);

	CHECK_TRUE(m_queueMutex = CreateMutex(NULL, FALSE, NULL));
	CHECK_TRUE(m_pastQueueMutex = CreateMutex(NULL, FALSE, NULL));
	CHECK_TRUE(m_for6pointQueueMutex = CreateMutex(NULL, FALSE, NULL));

	CHECK_TRUE(m_threadEvent = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_readEvent = CreateEvent(NULL, FALSE, FALSE, NULL));
	CHECK_TRUE(m_interruptThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL));

	m_ol.Offset = 0;
	m_ol.OffsetHigh = 0;
	m_ol.hEvent = m_readEvent;

	CHECK_TRUE(m_threadHandle = (HANDLE)_beginthreadex(NULL, 0, keyboardHandler, this, 0, &m_threadId));
	CHECK(WAIT_OBJECT_0 ==, WaitForSingleObject(m_threadEvent, INFINITE));

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

	if (m_threadKeyboardPastEvent)
	{
		m_doKeyboardPastForceTerminate = true;
		do
		{
			m_interruptThreadKeyboardPastReason = InterruptThreadReason_Terminate;
			SetEvent(m_interruptThreadKeyboardPastEvent);
			// wait for message handler thread terminate
		} while (WaitForSingleObject(m_threadKeyboardPastEvent, 100) != WAIT_OBJECT_0);

		CHECK_TRUE(CloseHandle(m_threadKeyboardPastEvent));
		m_threadKeyboardPastEvent = NULL;
	}

	if (m_threadCheckModifierEvent)
	{
		m_doCheckModifierForceTerminate = true;
		do
		{
			m_interruptThreadCheckModifierReason = InterruptThreadReason_Terminate;
			SetEvent(m_interruptThreadCheckModifierEvent);
			// wait for message handler thread terminate
		} while (WaitForSingleObject(m_threadCheckModifierEvent, 100) != WAIT_OBJECT_0);

		CHECK_TRUE(CloseHandle(m_threadCheckModifierEvent));
		m_threadCheckModifierEvent = NULL;
	}

	if (m_threadEvent)
	{
		m_doForceTerminate = true;
		do
		{
			m_interruptThreadReason = InterruptThreadReason_Terminate;
			SetEvent(m_interruptThreadEvent);
			// wait for message handler thread terminate
		} while (WaitForSingleObject(m_threadEvent, 100) != WAIT_OBJECT_0);

		CHECK_TRUE(CloseHandle(m_threadEvent));
		m_threadEvent = NULL;
	}

	if (m_mouse_hook == 1)
		m_mouseHandler.stop();

	if ((m_device == INVALID_HANDLE_VALUE) && (m_keyboard_hook != 0))
		m_keyboardHandler.stop();

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

	CHECK_TRUE(CloseHandle(m_interruptThreadEvent));
	m_interruptThreadEvent = NULL;

	CHECK_TRUE(CloseHandle(m_interruptThreadCheckModifierEvent));
	m_interruptThreadCheckModifierEvent = NULL;

	CHECK_TRUE(CloseHandle(m_interruptThreadKeyboardPastEvent));
	m_interruptThreadKeyboardPastEvent = NULL;

	//CHECK_TRUE( CloseHandle(m_interruptThreadFor6pointEvent) );
	//m_interruptThreadFor6pointEvent = NULL;
}

bool Engine::pause()
{
	/*
	do {
		m_interruptThreadFor6pointReason = InterruptThreadReason_Pause;
		SetEvent(m_interruptThreadFor6pointEvent);
	} while (WaitForSingleObject(m_threadFor6pointEvent, 2000) != WAIT_OBJECT_0);
	*/

	do
	{
		m_interruptThreadKeyboardPastReason = InterruptThreadReason_Pause;
		SetEvent(m_interruptThreadKeyboardPastEvent);
	} while (WaitForSingleObject(m_threadKeyboardPastEvent, 2000) != WAIT_OBJECT_0);

	do
	{
		m_interruptThreadCheckModifierReason = InterruptThreadReason_Pause;
		SetEvent(m_interruptThreadCheckModifierEvent);
	} while (WaitForSingleObject(m_threadCheckModifierEvent, 2000) != WAIT_OBJECT_0);

	if ((m_device != INVALID_HANDLE_VALUE) && m_keyboard_hook == 0)
	{ // nodokad がopenされている。かつ Keyboard LL hook/RawInput hookが使われていないとき
		do
		{
			m_interruptThreadReason = InterruptThreadReason_Pause;
			SetEvent(m_interruptThreadEvent);
		} while (WaitForSingleObject(m_threadEvent, 100) != WAIT_OBJECT_0);
		close();
	}

	return true;
}

bool Engine::resume()
{
	if ((m_device == INVALID_HANDLE_VALUE) && (m_keyboard_hook == 0))
	{ // nodokad close済 かつ Keyboard LL Hook/RawInput hookが使われていないとき
		if (!open())
		{
			return false; // FIXME
		}
		do
		{
			m_interruptThreadReason = InterruptThreadReason_Resume;
			SetEvent(m_interruptThreadEvent);
		} while (WaitForSingleObject(m_threadEvent, 100) != WAIT_OBJECT_0);
	}

	/*
	do {
		m_interruptThreadFor6pointReason = InterruptThreadReason_Resume;
		SetEvent(m_interruptThreadFor6pointEvent);
	} while (WaitForSingleObject(m_threadFor6pointEvent, 2000) != WAIT_OBJECT_0);
	*/

	do
	{
		m_interruptThreadCheckModifierReason = InterruptThreadReason_Resume;
		SetEvent(m_interruptThreadCheckModifierEvent);
	} while (WaitForSingleObject(m_threadCheckModifierEvent, 2000) != WAIT_OBJECT_0);

	do
	{
		m_interruptThreadKeyboardPastReason = InterruptThreadReason_Resume;
		SetEvent(m_interruptThreadKeyboardPastEvent);
	} while (WaitForSingleObject(m_threadKeyboardPastEvent, 2000) != WAIT_OBJECT_0);

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
	Acquire a(&m_cs);
	if (m_isSynchronizing)
		return false;
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
		return false;
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
		  << reinterpret_cast<DWORD>(i_hwnd) << _T(", ")
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
