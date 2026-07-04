//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// engine.h
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#ifndef _ENGINE_H
#define _ENGINE_H

#include "multithread.h"
#include "setting.h"
#include "msgstream.h"
#include "hook.h"
#include <map>
#include <set>
#include <queue>
//#  include "rawinput.h"

enum
{
	///
	WM_APP_engineNotify = WM_APP + 110,
};

///
enum EngineNotify
{
	EngineNotify_shellExecute,		  ///
	EngineNotify_loadSetting,		  ///
	EngineNotify_showDlg,			  ///
	EngineNotify_helpMessage,		  ///
	EngineNotify_setForegroundWindow, ///
	EngineNotify_clearLog,			  ///
	EngineNotify_changeicon,		  ///
};

///
class Engine
{
private:
	enum
	{
		MAX_GENERATE_KEYBOARD_EVENTS_RECURSION_COUNT = 64, ///
		MAX_KEYMAP_PREFIX_HISTORY = 64,					   ///
	};

	typedef Keymaps::KeymapPtrList KeymapPtrList; ///

	/// focus of a thread
	class FocusOfThread
	{
	public:
		DWORD m_threadId;		 /// thread id
		HWND m_hwndFocus;		 /** window that has focus on
																the thread */
		tstringi m_className;	/// class name of hwndFocus
		tstringi m_titleName;	/// title name of hwndFocus
		bool m_isConsole;		 /// is hwndFocus console ?
		KeymapPtrList m_keymaps; /// keymaps

	public:
		///
		FocusOfThread() : m_threadId(0), m_hwndFocus(NULL), m_isConsole(false) {}
	};
	typedef std::map<DWORD /*ThreadId*/, FocusOfThread> FocusOfThreads; ///

	typedef std::list<DWORD /*ThreadId*/> DetachedThreadIds; ///

	/// keyboard output delay
	class KEYBOARD_PAST
	{
	public:
		KEYBOARD_INPUT_DATA kid;
		DWORD volatile time_stamp;
		DWORD number;
	};

	/// current status in generateKeyboardEvents
	class Current
	{
	public:
		const Keymap *m_keymap; /// current keymap
		ModifiedKey m_mkey;		/// current processing key that user inputed
		/// index in currentFocusOfThread-&gt;keymaps
		Keymaps::KeymapPtrList::iterator m_i;

	public:
		///
		bool isPressed() const
		{
			return m_mkey.m_modifier.isOn(Modifier::Type_Down);
		}
	};

	friend class FunctionParam;

	/// part of keySeq
	enum Part
	{
		Part_all,  ///
		Part_up,   ///
		Part_down, ///
	};

	///
	class EmacsEditKillLine
	{
		tstring m_buf; /// previous kill-line contents

	public:
		bool m_doForceReset; ///

	private:
		///
		HGLOBAL makeNewKillLineBuf(const _TCHAR *i_data, int *i_retval);

	public:
		///
		void reset() { m_buf.resize(0); }
		/** EmacsEditKillLineFunc.
				clear the contents of the clopboard
				at that time, confirm if it is the result of the previous kill-line
				*/
		void func();
		/// EmacsEditKillLinePred
		int pred();
	};

	/// window positon for &amp;WindowHMaximize, &amp;WindowVMaximize
	class WindowPosition
	{
	public:
		///
		enum Mode
		{
			Mode_normal, ///
			Mode_H,		 ///
			Mode_V,		 ///
			Mode_HV,	 ///
		};

	public:
		HWND m_hwnd; ///
		RECT m_rc;   ///
		Mode m_mode; ///

	public:
		///
		WindowPosition(HWND i_hwnd, const RECT &i_rc, Mode i_mode)
			: m_hwnd(i_hwnd), m_rc(i_rc), m_mode(i_mode) {}
	};
	typedef std::list<WindowPosition> WindowPositions;

	typedef std::list<HWND> WindowsWithAlpha; /// windows for &amp;WindowSetAlpha

	enum InterruptThreadReason
	{
		InterruptThreadReason_Terminate,
		InterruptThreadReason_Pause,
		InterruptThreadReason_Resume,
	};

	enum TapHoldState  { TH_IDLE, TH_PENDING, TH_HOLDING };
	enum TapDanceState { TD_IDLE, TD_COUNTING };
	enum ComboState    { CO_IDLE, CO_PENDING, CO_PENDING_EVAL };

	///
	class InputHandler
	{
	public:
		typedef int (*INSTALL_HOOK)(INPUT_DETOUR i_keyboardDetour, Engine *i_engine, bool i_install);

		static unsigned int WINAPI run(void *i_this);

		InputHandler(INSTALL_HOOK i_installHook, INPUT_DETOUR i_inputDetour);

		~InputHandler();

		void run();

		int start(Engine *i_engine);

		int stop();

	private:
		unsigned m_threadId;
		HANDLE m_hThread;
		HANDLE m_hEventLL;
		INSTALL_HOOK m_installHook;
		INPUT_DETOUR m_inputDetour;
		Engine *m_engine;
		int m_mode;
	};

private:
	CriticalSection m_cs; /// criticalSection

	// setting
	HWND m_hwndAssocWindow;		 /** associated window (we post message to it) */
	Setting *volatile m_setting; /// setting

	//key_state m_keyState[4 * 256];	// none, E0, E1, E0E1

	// engine thread state
	HANDLE m_device;				   /// nodoka device
	HANDLE m_driverMutex;			   /// Global named mutex serializing driver access across sessions
	bool m_driverMutexHeld;			   /// true while this process actually holds m_driverMutex (pause() acquired it)
	bool m_didNodokaStartDevice;	   /** Did the nodoka start the nodoka-device ? */
	bool m_stopped;					   /** true once stop() has run; guards against double-teardown
											   (stop() is called explicitly from ~Nodoka() and again
											   from ~Engine(), which would otherwise touch already-NULLed
											   handles). */
	HANDLE m_threadEvent;			   /** 1. thread has been started	2. thread is about to end*/
	HANDLE m_threadCheckModifierEvent; /** 1. thread has been started	2. thread is about to end*/
	HANDLE m_threadKeyboardPastEvent;  /** 1. thread has been started	2. thread is about to end*/
	HANDLE m_threadFor6pointEvent;	 /** 1. thread has been started	2. thread is about to end*/

	HANDLE m_threadHandle;
	HANDLE m_threadCheckModifierHandle;
	HANDLE m_threadKeyboardPastHandle;
	HANDLE m_threadFor6pointHandle;

	std::deque<KEYBOARD_PAST> *m_pastQueue;
	std::deque<KEYBOARD_INPUT_DATA> *m_inputQueue;
	std::deque<KEYBOARD_PAST> *m_for6pointQueue;
	HANDLE m_queueMutex;
	HANDLE m_pastQueueMutex;
	HANDLE m_for6pointQueueMutex;

	MSLLHOOKSTRUCT m_msllHookCurrent;
	bool m_buttonPressed;
	bool m_dragging;
	InputHandler m_keyboardHandler;
	InputHandler m_mouseHandler;

	tstring m_nodokadVersion;					/// version of nodokad.sys
	tstring m_kbdAddIdVersion;					/// version of kbdaddid.sys (empty if not installed/active)
	HANDLE m_readEvent;							/** reading from nodoka device has been completed */
	HANDLE m_reInjectEvent;						/** re-injected keys are waiting in m_inputQueue */
	HANDLE m_interruptThreadEvent;				/// interrupt thread event
	HANDLE m_interruptThreadCheckModifierEvent; /// interrupt thread CheckModifier event
	HANDLE m_interruptThreadKeyboardPastEvent;  /// interrupt thread KeyboardPast event
	HANDLE m_interruptThreadFor6pointEvent;		/// interrupt thread For6point event

	volatile InterruptThreadReason m_interruptThreadReason;				 /// interrupt thread reason
	volatile InterruptThreadReason m_interruptThreadCheckModifierReason; /// interrupt thread reason
	volatile InterruptThreadReason m_interruptThreadKeyboardPastReason;  /// interrupt thread reason
	volatile InterruptThreadReason m_interruptThreadFor6pointReason;	 /// interrupt thread reason

	OVERLAPPED m_ol;	  /** for async read from nodoka device */
	OVERLAPPED m_writeOl; /** for async write to nodoka device (separate from m_ol to avoid IRP conflicts) */
	HANDLE m_writeEvent;  /** event for m_writeOl */
	HANDLE m_hookPipe;	/// named pipe for &SetImeString
	HMODULE m_sts4nodoka; /// DLL module for ThumbSense
	HMODULE m_cts4nodoka; /// DLL module for ThumbSense
	HMODULE m_ats4nodoka; /// DLL module for ThumbSense
	HMODULE m_gamepad;	/// DLL module for Gamepad

	bool volatile m_doForceTerminate;			   /// terminate engine thread
	bool volatile m_doCheckModifierForceTerminate; // terminate CheckModifier thread
	bool volatile m_doKeyboardPastForceTerminate;  // terminate KeyboardPast thread
	bool volatile m_doFor6pointForceTerminate;	 // terminate For6Point thread

	// TapHold timer thread
	HANDLE   m_tapHoldThreadHandle;
	unsigned m_tapHoldThreadId;
	HANDLE   m_tapHoldStartEvent;    ///< auto-reset: main→timer: key pressed, begin timing
	HANDLE   m_tapHoldCancelEvent;   ///< auto-reset: main→timer: key released before threshold
	HANDLE   m_tapHoldExpiredEvent;  ///< auto-reset: timer→main: threshold reached
	bool volatile m_doTapHoldForceTerminate;
	TapHoldState m_tapHoldState;
	const TapHoldRule *m_tapHoldCurrentRule;
	KEYBOARD_INPUT_DATA m_tapHoldKeyData;        ///< intercepted key-down event
	volatile int m_tapHoldThreshold;             ///< effective threshold for current rule [ms]
	Current m_tapHoldContext;                    ///< keymap context saved on PENDING entry
	Modifier m_tapHoldModifier;                  ///< modifier state captured at TH_PENDING entry
	bool                m_tapHoldNeedPartSecond; ///< true when hold was split: UP still needed
	KEYBOARD_INPUT_DATA m_tapHoldBufferedKey;    ///< one buffered key-down for permissive hold
	bool                m_tapHoldHasBufferedKey; ///< true when m_tapHoldBufferedKey is valid
	Key*                m_tapHoldPermissiveOtherKey; ///< key pointer for the buffered key
	Key*                m_tapHoldLastTapKey;     ///< key that last fired a tap action
	DWORD               m_tapHoldLastTapTime;    ///< tick of last tap (for quick tap term)

	// TapDance timer thread
	HANDLE   m_tapDanceThreadHandle;
	unsigned m_tapDanceThreadId;
	HANDLE   m_tapDanceStartEvent;   ///< auto-reset: main→timer: begin/restart timing
	HANDLE   m_tapDanceCancelEvent;  ///< auto-reset: main→timer: abort sequence
	HANDLE   m_tapDanceExpiredEvent; ///< auto-reset: timer→main: timeout reached
	bool volatile m_doTapDanceForceTerminate;
	TapDanceState m_tapDanceState;
	const TapDanceRule *m_tapDanceCurrentRule;
	int m_tapDanceCount;                               ///< current tap count (1-3)
	volatile int m_tapDanceTimeout;                    ///< effective timeout [ms]
	std::vector<KEYBOARD_INPUT_DATA> m_tapDanceBuffered; ///< buffered events for re-injection
	Current m_tapDanceContext;                           ///< keymap context saved on COUNTING entry
	Modifier m_tapDanceModifier;                         ///< modifier state captured at TD_COUNTING entry

	// Combo timer thread
	HANDLE   m_comboThreadHandle;
	unsigned m_comboThreadId;
	HANDLE   m_comboStartEvent;    ///< auto-reset: main→timer: begin/restart timing
	HANDLE   m_comboCancelEvent;   ///< auto-reset: main→timer: combo fired or aborted
	HANDLE   m_comboExpiredEvent;  ///< auto-reset: timer→main: window exceeded
	bool volatile m_doComboForceTerminate;
	ComboState m_comboState;
	std::vector<Key *> m_comboPressedKeys;            ///< sorted pressed keys in current attempt
	std::vector<KEYBOARD_INPUT_DATA> m_comboBuffered; ///< buffered events for re-injection
	volatile int m_comboWindow;                        ///< effective window [ms]
	Current m_comboContext;                            ///< keymap context saved on PENDING entry
	Modifier m_comboModifier;                          ///< modifier state captured at CO_PENDING entry
	Setting::ComboDetectorMode m_comboDetectorMode;         ///< active mode (fixed during CO_PENDING)
	Setting::ComboDetectorMode m_comboDetectorModePending;  ///< mode set by &SetComboDetector; applied at next CO_IDLE→CO_PENDING
	// Optional parameter overrides set by &SetComboDetector(mode, window=N, …); -1 = use def option
	int  m_comboWindowPending;             ///< -1 = use def option ComboWindow
	int  m_comboOverlapRatioPending;       ///< -1 = use def option ComboOverlapRatio
	int  m_comboNestedAlwaysMatchPending;  ///< -1 = use def option  (0=off, 1=on)
	int  m_comboIdleThresholdPending;      ///< -1 = use def option ComboIdleThreshold
	// Active values for the current CO_PENDING cycle (computed at CO_IDLE→CO_PENDING)
	int  m_comboWindowActive;
	int  m_comboOverlapRatioActive;
	bool m_comboNestedAlwaysMatchActive;
	int  m_comboIdleThresholdActive;
	DWORD m_comboAllKeysDownTime;    ///< timestamp when all combo keys were down (CO_PENDING_EVAL)
	bool  m_comboAllKeysDown;        ///< true when state is CO_PENDING_EVAL
	std::map<Key*, DWORD> m_comboKeyDownTimes; ///< per-key press timestamps for overlap ratio calc
	bool m_comboZeroLatencyActive;   ///< true when key1 was output immediately in zero-latency mode

	bool volatile m_isLogMode;					   /// is logging mode ?
	bool volatile m_isEnabled;					   /// is enabled  ?
	bool volatile m_isSynchronizing;			   /// is synchronizing ?
	int volatile m_iconColorNumber;				   // for IconColor function
	HANDLE m_eSync;								   /// event for synchronization
	int m_generateKeyboardEventsRecursionGuard;	/** guard against too many
																								recursion */

	// current key state
	Modifier m_currentLock;						 /// current lock key's state
	int volatile m_currentKeyPressCount;		 /** how many keys are pressed
																		phisically ? */
	int volatile m_currentKeyPressCountOnWin32;  /** how many keys are pressed
																					on win32 ? */
	Key *m_lastGeneratedKey;					 /// last generated key
	Key *m_lastPressedKey[2];					 /// last pressed key
	DWORD m_physModFirstPressedTime[8];			 /// for syncModifiersFromGetAsyncKeyState grace period
	bool  m_physModWasPressed[8];				 /// previous m_isPressed state per physical modifier
	volatile DWORD m_lastKeyEventTime;			 /// timestamp of last key event (for ModifierAutoClear)
	volatile DWORD m_prevKeyEventTime;			 /// timestamp of key event before current (for ComboIdleThreshold)
	ModifiedKey m_oneShotKey;					 /// one shot key
	unsigned int m_oneShotRepeatableRepeatCount; /// repeat count of one shot key
	bool m_isPrefix;							 /// is prefix ?
	bool m_doesIgnoreModifierForPrefix;			 /** does ignore modifier key
																					when prefixed ? */
	bool m_doesEditNextModifier;				 /** does edit next user input
																			key's modifier ? */
	Modifier m_modifierForNextKey;				 /** modifier for next key if
																			above is true */
	bool m_oneShot2;

	/** current keymaps.
		<dl>
		<dt>when &amp;OtherWindowClass
		<dd>currentKeymap becoms currentKeymaps[++ Current::i]
		<dt>when &amp;KeymapParent
		<dd>currentKeymap becoms currentKeyamp-&gt;parentKeymap
		<dt>other
		<dd>currentKeyamp becoms *Current::i
		</dl>
		*/
	const Keymap *volatile m_currentKeymap;			/// current keymap
	FocusOfThreads /*volatile*/ m_focusOfThreads;   ///
	FocusOfThread *volatile m_currentFocusOfThread; ///
	FocusOfThread m_globalFocus;					///
	HWND m_hwndFocus;								/// current focus window
	DetachedThreadIds m_detachedThreadIds;			///

	// for functions
	KeymapPtrList m_keymapPrefixHistory;	/// for &amp;KeymapPrevPrefix
	EmacsEditKillLine m_emacsEditKillLine;  /// for &amp;EmacsEditKillLine
	const ActionFunction *m_afShellExecute; /// for &amp;ShellExecute

	WindowPositions m_windowPositions;   ///
	WindowsWithAlpha m_windowsWithAlpha; ///

	tstring m_helpMessage; /// for &amp;HelpMessage
	tstring m_helpTitle;   /// for &amp;HelpMessage
	int m_variable;		   /// for &amp;Variable, &amp;Repeat

public:
	tomsgstream &m_log; /** log stream (output to log
															dialog's edit) */
	unsigned m_threadId;
	unsigned m_threadCheckModifierId;
	unsigned m_threadKeyboardPastId;
	unsigned m_threadFor6pointId;
	///
	void SendtoKeyboardHandler(int mode, int flag, USHORT MakeCode, ULONG_PTR extraInfo = 0);
	int m_keyboard_hook; // keyboard LL Hook mode or RawInput Hook
	int m_mouse_hook;	// mouse LL Hook mode
	int m_win8wa;

public:
	/// keyboard handler thread
	static unsigned int WINAPI keyboardDetour(Engine *i_this, WPARAM i_wParam, LPARAM i_lParam);
	/// mouse handler thread
	static unsigned int WINAPI mouseDetour(Engine *i_this, WPARAM i_wParam, LPARAM i_lParam);

private:
	unsigned int keyboardDetour(KBDLLHOOKSTRUCT *i_kid);
	///
	unsigned int mouseDetour(WPARAM i_message, MSLLHOOKSTRUCT *i_mid);
	///
	void injectInputIn(KEYBOARD_INPUT_DATA *i_kid);
	void injectInputOut(KEYBOARD_INPUT_DATA *i_kid);

	/// keyboard handler thread
	static unsigned int WINAPI keyboardHandler(void *i_this);
	///
	void keyboardHandler();

	/// CheckModifier thread
	static unsigned int WINAPI CheckModifier(void *i_this);
	///
	void CheckModifier();

	/// KeyboardPast thread
	static unsigned int WINAPI KeyboardPast(void *i_this);
	///
	void KeyboardPast();

	/// For6point thread
	static unsigned int WINAPI For6point(void *i_this);
	///
	void For6point();

	/// TapHold timer thread
	static unsigned int WINAPI tapHoldTimer(void *i_this);
	void tapHoldTimer();

	/// TapDance timer thread
	static unsigned int WINAPI tapDanceTimer(void *i_this);
	void tapDanceTimer();

	/// Combo timer thread
	static unsigned int WINAPI comboTimer(void *i_this);
	void comboTimer();

	/// re-inject a list of buffered key events to the front of m_inputQueue
	void reInjectKeys(const std::vector<KEYBOARD_INPUT_DATA> &i_events);

	/// check if i_key appears in any combo rule of the current keymap
	bool isComboCandidate(const Key *i_key);

	/// check focus window
	bool checkFocusWindow();

	// rawinput for 複数キーボード
	//		int getUnitID();
	//		UINT rawcbSize = 1024;
	//		PRAWINPUT pRawInput = (PRAWINPUT)malloc(rawcbSize);

	/// KeyState
	int Engine::getKeyState(u_int8 e0e1_flag, USHORT key, USHORT UnitId);
	void Engine::setKeyState(u_int8 e0e1_flag, USHORT key, USHORT UnitId, int state);
	DWORD Engine::getKeyStateTime(u_int8 e0e1_flag, USHORT key, USHORT UnitId, int num);
	void Engine::setKeyStateTime(u_int8 e0e1_flag, USHORT key, USHORT UnitId, int num, DWORD st);
	DWORD Engine::getKeyStateDelay(u_int8 e0e1_flag, USHORT key, USHORT UnitId, int delay);

	/// is modifier pressed ?
	bool isPressed(Modifier::Type i_mt);
	/// fix modifier key
	bool fixModifierKey(ModifiedKey *io_mkey, Keymap::AssignMode *o_am);

	/// output to log
	void outputToLog(const Key *i_key, const ModifiedKey &i_mkey,
					 int i_debugLevel);

	/// genete modifier events
	void generateModifierEvents(const Modifier &i_mod);

	/// genete event
	void generateEvents(Current i_c, const Keymap *i_keymap, Key *i_event);

	/// generate keyboard event
	void generateKeyEvent(Key *i_key, bool i_doPress, bool i_isByAssign);
	///
	void generateActionEvents(const Current &i_c, const Action *i_a,
							  bool i_doPress);
	///
	void generateKeySeqEvents(const Current &i_c, const KeySeq *i_keySeq,
							  Part i_part);
	///
	void generateKeyboardEvents(const Current &i_c);
	///
	void beginGeneratingKeyboardEvents(const Current &i_c, bool i_isModifier, bool m_oneShot2);

	/// pop all pressed key on win32
	void keyboardResetOnWin32();

	/// sync modifier state with physical keys (fixes stuck modifiers)
	void syncModifiersFromGetAsyncKeyState();

	/// clear stuck modifiers after inactivity timeout (ModifierAutoClear)
	void modifierAutoClear();

	/// get current modifiers
	Modifier getCurrentModifiers(Key *i_key, bool i_isPressed);

	/// describe bindings
	void describeBindings();

	/// update m_lastPressedKey
	void updateLastPressedKey(Key *i_key);

	/// set current keymap
	void setCurrentKeymap(const Keymap *i_keymap, bool i_doesAddToHistory = false);
	/** open nodoka device
		@return true if nodoka device successfully is opened
		*/
	bool open();

	/// close nodoka device
	void close();

	/// load/unload [sca]ts4nodoka.dll
	void manageTs4nodoka(TCHAR *i_ts4nodokaDllName, TCHAR *i_dependDllName,
						 bool i_load, HMODULE *i_pTs4nodoka);

private:
	// BEGINING OF FUNCTION DEFINITION
	/// Set Nodoka Icon
	void funcIconColor(FunctionParam *i_param, int i_icon_color);
	/// set/unset UseTSF
	void funcUseTSF(FunctionParam *i_param, BooleanType i_UseTSF);
	/// send a default key to Windows
	void funcDefault(FunctionParam *i_param);
	/// use a corresponding key of a parent keymap
	void funcKeymapParent(FunctionParam *i_param);
	/// use a corresponding key of a current window
	void funcKeymapWindow(FunctionParam *i_param);
	/// use a corresponding key of the previous prefixed keymap
	void funcKeymapPrevPrefix(FunctionParam *i_param, int i_previous);
	/// use a corresponding key of an other window class, or use a default key
	void funcOtherWindowClass(FunctionParam *i_param);
	/// prefix key
	void funcPrefix(FunctionParam *i_param, const Keymap *i_keymap,
					BooleanType i_doesIgnoreModifiers = BooleanType_true);
	/// other keymap's key
	void funcKeymap(FunctionParam *i_param, const Keymap *i_keymap);
	/// sync
	void funcSync(FunctionParam *i_param);
	/// toggle lock
	void funcToggle(FunctionParam *i_param, ModifierLockType i_lock,
					ToggleType i_toggle = ToggleType_toggle);
	/// edit next user input key's modifier
	void funcEditNextModifier(FunctionParam *i_param,
							  const Modifier &i_modifier);
	/// variable
	void funcVariable(FunctionParam *i_param, int i_mag, int i_inc);
	/// repeat N times
	void funcRepeat(FunctionParam *i_param, const KeySeq *i_keySeq,
					int i_max = 10);
	/// undefined (bell)
	void funcUndefined(FunctionParam *i_param);
	/// ignore
	void funcIgnore(FunctionParam *i_param);
	/// post message
	void funcPostMessage(FunctionParam *i_param, ToWindowType i_window,
						 UINT i_message, WPARAM i_wParam, LPARAM i_lParam);
	/// send/post message
	void funcSendPostMessage(FunctionParam *i_param, const StrExprArg &i_sendpost, const StrExprArg &i_class, const StrExprArg &i_title,
							 UINT i_message, WPARAM i_wParam, LPARAM i_lParam);
	/// SendText
	void funcSendText(FunctionParam *i_param, const StrExprArg &i_text);
	/// SendMsg
	void funcSendMsg(FunctionParam *i_param, int i_magic_number, int i_mod_key, int i_vkey);
	/// ShellExecute
	void funcShellExecute(FunctionParam *i_param, const StrExprArg &i_operation,
						  const StrExprArg &i_file, const StrExprArg &i_parameters,
						  const StrExprArg &i_directory,
						  ShowCommandType i_showCommand);
	/// SetForegroundWindow
	void funcSetForegroundWindow(FunctionParam *i_param,
								 const tregex &i_windowClassName,
								 LogicalOperatorType i_logicalOp = LogicalOperatorType_and,
								 const tregex &i_windowTitleName = tregex(_T(".*")));
	/// load setting
	void funcLoadSetting(FunctionParam *i_param,
						 const StrExprArg &i_name = StrExprArg());
	/// virtual key
	void funcVK(FunctionParam *i_param, VKey i_vkey);
	/// wait
	void funcWait(FunctionParam *i_param, int i_milliSecond);
	/// waitkey
	void funcWaitKey(FunctionParam *i_param, int i_milliSecond);
	/// investigate WM_COMMAND, WM_SYSCOMMAND
	void funcInvestigateCommand(FunctionParam *i_param);
	/// show nodoka dialog box
	void funcNodokaDialog(FunctionParam *i_param, NodokaDialogType i_dialog,
						  ShowCommandType i_showCommand);
	void funcMayuDialog(FunctionParam *i_param, NodokaDialogType i_dialog,
						ShowCommandType i_showCommand);
	/// describe bindings
	void funcDescribeBindings(FunctionParam *i_param);
	/// show help message
	void funcHelpMessage(FunctionParam *i_param,
						 const StrExprArg &i_title = StrExprArg(),
						 const StrExprArg &i_message = StrExprArg());
	/// show variable
	void funcHelpVariable(FunctionParam *i_param, const StrExprArg &i_title);
	/// raise window
	void funcWindowRaise(FunctionParam *i_param,
						 TargetWindowType i_twt = TargetWindowType_overlapped);
	/// lower window
	void funcWindowLower(FunctionParam *i_param,
						 TargetWindowType i_twt = TargetWindowType_overlapped);
	/// minimize window
	void funcWindowMinimize(FunctionParam *i_param, TargetWindowType i_twt = TargetWindowType_overlapped);
	/// maximize window
	void funcWindowMaximize(FunctionParam *i_param, TargetWindowType i_twt = TargetWindowType_overlapped);
	/// maximize window horizontally
	void funcWindowHMaximize(FunctionParam *i_param, TargetWindowType i_twt = TargetWindowType_overlapped);
	/// maximize window virtically
	void funcWindowVMaximize(FunctionParam *i_param, TargetWindowType i_twt = TargetWindowType_overlapped);
	/// maximize window virtically or horizontally
	void funcWindowHVMaximize(FunctionParam *i_param, BooleanType i_isHorizontal,
							  TargetWindowType i_twt = TargetWindowType_overlapped);
	/// move window
	void funcWindowMove(FunctionParam *i_param, int i_dx, int i_dy,
						TargetWindowType i_twt = TargetWindowType_overlapped);
	/// move window to ...
	void funcWindowMoveTo(FunctionParam *i_param, GravityType i_gravityType,
						  int i_dx, int i_dy, TargetWindowType i_twt = TargetWindowType_overlapped);
	/// move window visibly
	void funcWindowMoveVisibly(FunctionParam *i_param,
							   TargetWindowType i_twt = TargetWindowType_overlapped);
	/// move window to other monitor
	void funcWindowMonitorTo(FunctionParam *i_param,
							 WindowMonitorFromType i_fromType, int i_monitor,
							 BooleanType i_adjustPos = BooleanType_true,
							 BooleanType i_adjustSize = BooleanType_false);
	/// move window to other monitor
	void funcWindowMonitor(FunctionParam *i_param, int i_monitor,
						   BooleanType i_adjustPos = BooleanType_true,
						   BooleanType i_adjustSize = BooleanType_false);
	///
	void funcWindowClingToLeft(FunctionParam *i_param,
							   TargetWindowType i_twt = TargetWindowType_overlapped);
	///
	void funcWindowClingToRight(FunctionParam *i_param,
								TargetWindowType i_twt = TargetWindowType_overlapped);
	///
	void funcWindowClingToTop(FunctionParam *i_param,
							  TargetWindowType i_twt = TargetWindowType_overlapped);
	///
	void funcWindowClingToBottom(FunctionParam *i_param,
								 TargetWindowType i_twt = TargetWindowType_overlapped);
	/// close window
	void funcWindowClose(FunctionParam *i_param,
						 TargetWindowType i_twt = TargetWindowType_overlapped);
	/// toggle top-most flag of the window
	void funcWindowToggleTopMost(FunctionParam *i_param);
	/// identify the window
	void funcWindowIdentify(FunctionParam *i_param);
	/// set alpha blending parameter to the window
	void funcWindowSetAlpha(FunctionParam *i_param, int i_alpha);
	/// redraw the window
	void funcWindowRedraw(FunctionParam *i_param);
	/// resize window to
	void funcWindowResizeTo(FunctionParam *i_param, int i_width, int i_height,
							TargetWindowType i_twt = TargetWindowType_overlapped);
	/// resize window to Per
	void funcWindowResizeToPer(FunctionParam *i_param, int i_width, int i_height,
							   TargetWindowType i_twt = TargetWindowType_overlapped);
	/// resize & move window to
	void funcWindowResizeMoveTo(FunctionParam *i_param, int i_width, int i_height,
								GravityType i_gravityType,
								int i_dx, int i_dy, TargetWindowType i_twt = TargetWindowType_overlapped);
	/// resize & move window to Per
	void funcWindowResizeMoveToPer(FunctionParam *i_param, int i_width, int i_height,
								   GravityType i_gravityType,
								   int i_dx, int i_dy, TargetWindowType i_twt = TargetWindowType_overlapped);
	/// move the mouse cursor
	void funcMouseMove(FunctionParam *i_param, int i_dx, int i_dy);
	/// send a mouse-wheel-message to Windows
	void funcMouseWheel(FunctionParam *i_param, int i_delta);
	/// convert the contents of the Clipboard to upper case or lower case

	/// move Mouse to ...
	void funcMouseMoveTo(FunctionParam *i_param, GravityType i_gravityType,
						 int i_dx, int i_dy, BooleanType i_window = BooleanType_false);
	/// move Mouse to other monitor
	void funcMouseMonitorTo(FunctionParam *i_param,
							WindowMonitorFromType i_fromType, int i_monitor);
	/// move Mouse to other monitor
	void funcMouseMonitor(FunctionParam *i_param, int i_monitor);

	void funcClipboardChangeCase(FunctionParam *i_param,
								 BooleanType i_doesConvertToUpperCase);
	/// convert the contents of the Clipboard to upper case
	void funcClipboardUpcaseWord(FunctionParam *i_param);
	/// convert the contents of the Clipboard to lower case
	void funcClipboardDowncaseWord(FunctionParam *i_param);
	/// set the contents of the Clipboard to the string
	void funcClipboardCopy(FunctionParam *i_param, const StrExprArg &i_text);
	///
	void funcEmacsEditKillLinePred(FunctionParam *i_param,
								   const KeySeq *i_keySeq1,
								   const KeySeq *i_keySeq2);
	///
	void funcEmacsEditKillLineFunc(FunctionParam *i_param);
	/// clear log
	void funcLogClear(FunctionParam *i_param);
	/// recenter
	void funcRecenter(FunctionParam *i_param);
	/// Direct SSTP
	void funcDirectSSTP(FunctionParam *i_param,
						const tregex &i_name,
						const StrExprArg &i_protocol,
						const std::list<tstringq> &i_headers);
	/// PlugIn
	void funcPlugIn(FunctionParam *i_param,
					const StrExprArg &i_dllName,
					const StrExprArg &i_funcName = StrExprArg(),
					const StrExprArg &i_funcParam = StrExprArg(),
					BooleanType i_doesCreateThread = BooleanType_false);
	/// set IME Conversion status
	void funcSetImeConvStatus(FunctionParam *i_param, int i_status);
	/// set IME open status
	void funcSetImeStatus(FunctionParam *i_param, ToggleType i_toggle = ToggleType_toggle);
	/// set string to IME
	void funcSetImeString(FunctionParam *i_param, const StrExprArg &i_data);
	// from yamy 0.03 function.cpp
	/// enter to mouse event hook mode
	void funcMouseHook(FunctionParam *i_param, MouseHookType i_hookType, int i_hookParam);
	/// cancel prefix
	void funcCancelPrefix(FunctionParam *i_param);
	/// set ComboDetector mode (and optional params) at runtime
	void funcSetComboDetector(FunctionParam *i_param, Setting::ComboDetectorMode i_mode,
	                          int i_window, int i_overlap, int i_nested, int i_idle);

	// END OF FUNCTION DEFINITION
#define FUNCTION_FRIEND
#include "functions.h"
#undef FUNCTION_FRIEND

public:
	///
	Engine(tomsgstream &i_log, int i_keyboard_hook, int i_mouse_hook, int m_win8wa);
	///
	~Engine();

	/// start/stop keyboard handler thread
	void start();
	///
	void stop();

	/// pause keyboard handler thread and close device
	bool pause();

	/// resume keyboard handler thread and re-open device
	bool resume();

	/// do some procedure before quit which must be done synchronously
	/// (i.e. not on WM_QUIT)
	bool prepairQuit();

	/// logging mode
	void enableLogMode(bool i_isLogMode = true) { m_isLogMode = i_isLogMode; }
	///
	void disableLogMode() { m_isLogMode = false; }

	/// enable/disable engine
	void enable(bool i_isEnabled = true) { m_isEnabled = i_isEnabled; }
	///
	void disable() { m_isEnabled = false; }
	///
	bool getIsEnabled() const { return m_isEnabled; }

	/// get current icon number
	int getIconColorNumber() { return m_iconColorNumber; }

	/// set current icon number
	void setIconColorNumber(int number) { m_iconColorNumber = number; }

	/// associated window
	void setAssociatedWndow(HWND i_hwnd) { m_hwndAssocWindow = i_hwnd; }

	/// associated window
	HWND getAssociatedWndow() const { return m_hwndAssocWindow; }

	/// setting
	bool setSetting(Setting *i_setting);

	/// focus
	bool setFocus(HWND i_hwndFocus, DWORD i_threadId,
				  const tstringi &i_className,
				  const tstringi &i_titleName, bool i_isConsole);

	/// lock state
	bool setLockState(bool i_isNumLockToggled, bool i_isCapsLockToggled,
					  bool i_isScrollLockToggled, bool i_isKanaLockToggled,
					  bool i_isImeLockToggled, bool i_isImeCompToggled,
					  bool i_isCandidateWindow);
	bool setLockState2();
	bool setLockState2A(DWORD dwComversion);
	bool setLockState2B(bool bComposition);
	bool setLockState3(UINT UnitID);
	bool setLockState4(bool bDoublePress);

	/// show
	void checkShow(HWND i_hwnd);
	bool setShow(bool i_isMaximized, bool i_isMinimized, bool i_isMDI);

	/// sync
	bool syncNotify();

	/// thread detach notify
	bool threadDetachNotify(DWORD i_threadId);

	/// shell execute
	void shellExecute();

	/// get help message
	void getHelpMessages(tstring *o_helpMessage, tstring *o_helpTitle);

	/// command notify
	void commandNotify(HWND i_hwnd, UINT i_message, WPARAM i_wParam,
					   LPARAM i_lParam);

	/// get current window class name
	const tstringi &getCurrentWindowClassName() const { return m_currentFocusOfThread->m_className; }

	/// get current window title name
	const tstringi &getCurrentWindowTitleName() const { return m_currentFocusOfThread->m_titleName; }

	/// get nodokad version
	const tstring &getNodokadVersion() const { return m_nodokadVersion; }

	/// detect kbdaddid driver and retrieve its file version
	void detectKbdAddId();

	/// get kbdaddid version (empty if not installed/active)
	const tstring &getKbdAddIdVersion() const { return m_kbdAddIdVersion; }

	/// get current keymap
	const void get_keymaps();

	const tstring &getCurrentWindow_keymaps() const {
		//get_keymaps();
		//return _T("ok");
	};
};

///
class FunctionParam
{
public:
	bool m_isPressed;			/// is key pressed ?
	HWND m_hwnd;				///
	Engine::Current m_c;		/// new context
	bool m_doesNeedEndl;		/// need endl ?
	const ActionFunction *m_af; ///
};

#endif // !_ENGINE_H
