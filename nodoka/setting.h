//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting.h
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#ifndef _SETTING_H
#define _SETTING_H

#include "keymap.h"
#include "parser.h"
#include "multithread.h"
#include "stringtool.h"
#include <set>

/// this class contains all of loaded settings
class Setting
{
public:
	typedef std::set<tstringi> Symbols;	///
	typedef std::list<Modifier> Modifiers; ///
	typedef struct _key_state
	{
		int state;	// 0(init), -1(break), 1(make), 2(repeat), 4(delay), 6(delay+repeat)
		DWORD st1;	// 1st timestamp
		DWORD st2;	// 2nd timestamp
		DWORD st3;	// real key down timestamp	物理的にキーダウンがあった時刻。breakもmake来なくなったときにリピート停止に使う。
		DWORD delayA; // keyboard delay time	システムのデフォルトは250msから3sだが、任意設定できるようにする。
		DWORD delayB; // keyboard speed		システムでは1秒間に2.5回から30回のため、33msが最大。1msから設定できるが実際に動くかどうかは不明。
		DWORD delayI; // delay indivisual		この値が指定されているとキー入力しても delayする。0から65535ms
	} key_state;
	typedef struct _keyboard_table
	{
		HANDLE hDevice; // Keyboardの固有ハンドル
		UINT UnitID;	// Nodoka固有のKeyboard Unit ID 0～8
		DWORD dwVendorId;
		DWORD dwProductId;
		DWORD dwRevisionId;
		DWORD dwExtraInfo; // kbdaddidモード用 ExtraInformation値 (0=未設定)
	} keyboard_table;

public:
	Keyboard m_keyboard;				   ///
	Keymaps m_keymaps;					   ///
	KeySeqs m_keySeqs;					   ///
	Symbols m_symbols;					   ///
	bool m_correctKanaLockHandling;		   ///
	bool m_sts4nodoka;					   ///
	bool m_cts4nodoka;					   ///
	bool m_ats4nodoka;					   ///
	bool m_mouseEvent;					   ///
	LONG m_dragThreshold;				   ///
	unsigned int m_oneShotRepeatableDelay; ///
	int m_CenterVal;
	int m_SendTextDelay;
	bool m_gamepad;			  ///
	int m_maxVALUE;			  //= 10000;	// 最大値
	int m_thVALUE;			  //= 5000;		// 閾値
	int m_deadzoneVALUE;	  //= 2500;		// デッドゾーンの範囲
	int m_REPEAT_TIMES_1;	 //= 20;		// キー入力後リピートするまでの間隔
	int m_REPEAT_TIMES_2;	 //= 3;			// 2個目以降のリピート間隔
	int m_WAIT;				  //= 10;		// ms
	int m_REPEAT_FLAG_PAD;	//= 0xffff;		//
	int m_REPEAT_FLAG_HAT;	//= 0xffff;		//
	int m_REPEAT_FLAG_BUTTON; //= 0xfff;	//
	bool m_CaretBlinkTime;
	DWORD m_BlinkTimeOff;
	DWORD m_BlinkTimeOn;
	bool m_Repeat;
	DWORD m_DelayA;
	DWORD m_DelayB;
	DWORD m_DelayMax;
	bool m_DelayMaxFlag;
	bool m_FocusChange;
	key_state m_keyState[8 * 4 * 256]; // 8個のキーボード x (none, E0, E1, E0E1) x 256キー
	bool m_CheckModifier;
	int m_CheckModifierTime;
	bool m_UseTSF;
	int m_UseUnitID;					// rawinputによるUnitID切り替え機能有効無効
	int m_UseKbdAddId;					// kbdaddidによるUnitID切り替え機能有効無効
	int m_KEY_first;					// 初回設定
	keyboard_table m_keyboard_table[8]; // UnitID読み替えテーブル
	bool m_UseDoublePress;				// DP option
	int m_DoublePressPeriod;			// DP value [ms]
	int m_DoublePressDelay;				// DP delay [ms]
	int m_number;
	bool m_UseFakeUp;  // FakeUp
	int m_FakeUpDelay; // FakeUp 遅延時間
	int m_FakeUpKey;   // FakeUp MakeKey
	bool m_For6point;
	int m_key1of6;
	int m_key2of6;
	int m_key3of6;
	int m_key4of6;
	int m_key5of6;
	int m_key6of6;
	int m_SyncModifierGracePeriod; // grace period [ms] for syncModifiersFromGetAsyncKeyState
	int m_ModifierAutoClear;       // auto-clear stuck modifiers after N seconds of inactivity (0=disabled)

	/// combo / taphold / tapdance global defaults
	int  m_ComboWindow;            ///< combo simultaneous window [ms]
	int  m_TapHoldThreshold;       ///< taphold long-press threshold [ms]
	bool m_TapHoldInterruptIsTap;  ///< true=interrupted by other key → tap, false=hold
	bool m_TapHoldPermissiveHold;  ///< true=other key down+up confirms hold (permissive hold)
	bool m_TapHoldOnOtherKeyPress; ///< true=other key down confirms hold immediately
	int  m_TapHoldQuickTapTerm;    ///< 0=disabled; >0=ms: same key within window fires tap instantly
	int  m_TapDanceTimeout;        ///< tapdance inter-tap timeout [ms]

	/// combo detector mode
	enum ComboDetectorMode
	{
		CD_TIMEOUT,       ///< timeout-based (default)
		CD_IMMEDIATE,     ///< fire only on exact match, no timer
		CD_ROLLOVER,      ///< allow interleaved non-combo keys, no timer
		CD_STRICT_ORDER,  ///< keys must be pressed in definition order (timeout-based)
		CD_ZERO_LATENCY,  ///< immediate output + IME backspace correction
	};
	ComboDetectorMode m_ComboDetectorMode; ///< active detection mode
	int  m_ComboIdleThreshold;             ///< idle-distance [ms]; 0 = disabled
	int  m_ComboOverlapRatio;              ///< [%] overlap ratio threshold; 0 = disabled
	bool m_ComboNestedAlwaysMatch;         ///< keep CO_PENDING while key1 held after timeout

public:
	Setting()
		: m_correctKanaLockHandling(false),
		  m_sts4nodoka(false),
		  m_cts4nodoka(false),
		  m_ats4nodoka(false),
		  m_CenterVal(3200),
		  m_SendTextDelay(20),
		  m_gamepad(false),
		  m_maxVALUE(10000),
		  m_thVALUE(5000),
		  m_deadzoneVALUE(2500),
		  m_REPEAT_TIMES_1(20),
		  m_REPEAT_TIMES_2(10),
		  m_WAIT(10),
		  m_REPEAT_FLAG_PAD(0xffff),
		  m_REPEAT_FLAG_HAT(0xffff),
		  m_REPEAT_FLAG_BUTTON(0xffff),
		  m_CaretBlinkTime(false),
		  m_BlinkTimeOff(500),
		  m_BlinkTimeOn(50),
		  m_Repeat(false),
		  m_DelayA(250),
		  m_DelayB(33),
		  m_DelayMax(10000),
		  m_DelayMaxFlag(false),
		  m_FocusChange(false),
		  m_mouseEvent(false),
		  m_dragThreshold(0),
		  m_CheckModifier(false),
		  m_CheckModifierTime(0),
		  m_UseTSF(true),
		  m_UseUnitID(0),
		  m_UseKbdAddId(0),
		  m_KEY_first(1),
		  m_UseDoublePress(false),
		  m_DoublePressPeriod(0),
		  m_DoublePressDelay(0),
		  m_number(0),
		  m_UseFakeUp(false),
		  m_FakeUpDelay(0),
		  m_FakeUpKey(255),
		  m_For6point(false),
		  m_key1of6(0),
		  m_key2of6(0),
		  m_key3of6(0),
		  m_key4of6(0),
		  m_key5of6(0),
		  m_key6of6(0),
		  m_SyncModifierGracePeriod(0),
		  m_ModifierAutoClear(0),
		  m_oneShotRepeatableDelay(0),
		  m_ComboWindow(50),
		  m_TapHoldThreshold(200),
		  m_TapHoldInterruptIsTap(true),
		  m_TapHoldPermissiveHold(false),
		  m_TapHoldOnOtherKeyPress(false),
		  m_TapHoldQuickTapTerm(0),
		  m_TapDanceTimeout(300),
		  m_ComboDetectorMode(CD_TIMEOUT),
		  m_ComboIdleThreshold(0),
		  m_ComboOverlapRatio(0),
		  m_ComboNestedAlwaysMatch(false) {}
};

///
namespace Event
{
///
extern Key prefixed;
///
extern Key before_key_down;
///
extern Key after_key_up;
///
extern Key *events[];
} // namespace Event

///
class SettingLoader
{
#define FUNCTION_FRIEND
#include "functions.h"
#undef FUNCTION_FRIEND

public:
	///
	class FunctionCreator
	{
	public:
		const _TCHAR *m_name;	///
		FunctionData *m_creator; ///
	};

private:
	typedef std::vector<Token> Tokens;		///
	typedef std::vector<tstringi> Prefixes; ///
	typedef std::vector<bool> CanReadStack; ///

private:
	Setting *m_setting;		/// loaded setting
	bool m_isThereAnyError; /// is there any error ?

	SyncObject *m_soLog; /// guard log output stream
	tostream *m_log;	 /// log output stream

	tstringi m_currentFilename; /// current filename

	Tokens m_tokens;	   /// tokens for current line
	Tokens::iterator m_ti; /// current processing token

	static Prefixes *m_prefixes;	   /// prefix terminal symbol
	static size_t m_prefixesRefCcount; /// reference count of prefix

	Keymap *m_currentKeymap; /// current keymap

	CanReadStack m_canReadStack; /// for &lt;COND_SYMBOL&gt;

	Modifier m_defaultAssignModifier; /** default
																				&lt;ASSIGN_MODIFIER&gt; */
	Modifier m_defaultKeySeqModifier; /** default
																				&lt;KEYSEQ_MODIFIER&gt; */

private:
	bool isEOL();												/// is there no more tokens ?
	Token *getToken();											/// get next token
	Token *lookToken();											/// look next token
	bool getOpenParen(bool i_doesThrow, const _TCHAR *i_name);  /// argument "("
	bool getCloseParen(bool i_doesThrow, const _TCHAR *i_name); /// argument ")"
	bool getComma(bool i_doesThrow, const _TCHAR *i_name);		/// argument ","

	void load_LINE();   /// &lt;LINE&gt;
	void load_DEFINE(); /// &lt;DEFINE&gt;
	void load_IF();		/// &lt;IF&gt;
	void load_ELSE(bool i_isElseIf, const tstringi &i_token);
	/// &lt;ELSE&gt; &lt;ELSEIF&gt;
	bool load_ENDIF(const tstringi &i_token); /// &lt;ENDIF&gt;
	void load_INCLUDE();					  /// &lt;INCLUDE&gt;
	void load_SCAN_CODES(Key *o_key);		  /// &lt;SCAN_CODES&gt;
	void load_DEFINE_KEY();					  /// &lt;DEFINE_KEY&gt;
	void load_DEFINE_MODIFIER();			  /// &lt;DEFINE_MODIFIER&gt;
	void load_DEFINE_SYNC_KEY();			  /// &lt;DEFINE_SYNC_KEY&gt;
	void load_DEFINE_ALIAS();				  /// &lt;DEFINE_ALIAS&gt;
	void load_DEFINE_SUBSTITUTE();			  /// &lt;DEFINE_SUBSTITUTE&gt;
	void load_DEFINE_OPTION();				  /// &lt;DEFINE_OPTION&gt;
	void load_KEYBOARD_DEFINITION();		  /// &lt;KEYBOARD_DEFINITION&gt;
	Modifier load_MODIFIER(Modifier::Type i_mode, Modifier i_modifier,
						   Modifier::Type *o_mode = NULL);
	/// &lt;..._MODIFIER&gt;
	Key *load_KEY_NAME(); /// &lt;KEY_NAME&gt;
	void load_KEYMAP_DEFINITION(const Token *i_which);
	/// &lt;KEYMAP_DEFINITION&gt;
	void load_ARGUMENT(bool *o_arg);				/// &lt;ARGUMENT&gt;
	void load_ARGUMENT(int *o_arg);					/// &lt;ARGUMENT&gt;
	void load_ARGUMENT(unsigned int *o_arg);		/// &lt;ARGUMENT&gt;
	void load_ARGUMENT(unsigned __int64 *o_arg);	/// &lt;ARGUMENT&gt;
	void load_ARGUMENT(__int64 *o_arg);				/// &lt;ARGUMENT&gt;
	void load_ARGUMENT(long *o_arg);				/// &lt;ARGUMENT&gt;
	void load_ARGUMENT(DWORD *o_arg);				/// &lt;ARGUMENT&gt;
	void load_ARGUMENT(tstringq *o_arg);			/// &lt;ARGUMENT&gt;
	void load_ARGUMENT(tstring *o_arg);				/// &lt;ARGUMENT&gt;
	void load_ARGUMENT(std::list<tstringq> *o_arg); /// &lt;ARGUMENT&gt;
	void load_ARGUMENT(tregex *o_arg);				/// &lt;ARGUMENT&gt;
	void load_ARGUMENT(VKey *o_arg);				/// &lt;ARGUMENT_VK&gt;
	void load_ARGUMENT(ToWindowType *o_arg);		/// &lt;ARGUMENT_WINDOW&gt;
	void load_ARGUMENT(GravityType *o_arg);			/// &lt;ARGUMENT&gt;
	void load_ARGUMENT(MouseHookType *o_arg);		/// &lt;ARGUMENT&gt;
	void load_ARGUMENT(NodokaDialogType *o_arg);	/// &lt;ARGUMENT&gt;
	void load_ARGUMENT(ModifierLockType *o_arg);	/// &lt;ARGUMENT_LOCK&gt;
	void load_ARGUMENT(ToggleType *o_arg);			/// &lt;ARGUMENT&gt;
	void load_ARGUMENT(ShowCommandType *o_arg);		///&lt;ARGUMENT_SHOW_WINDOW&gt;
	void load_ARGUMENT(TargetWindowType *o_arg);
	/// &lt;ARGUMENT_TARGET_WINDOW_TYPE&gt;
	void load_ARGUMENT(BooleanType *o_arg);			  /// &lt;bool&gt;
	void load_ARGUMENT(LogicalOperatorType *o_arg);   /// &lt;ARGUMENT&gt;
	void load_ARGUMENT(Modifier *o_arg);			  /// &lt;ARGUMENT&gt;
	void load_ARGUMENT(const Keymap **o_arg);		  /// &lt;ARGUMENT&gt;
	void load_ARGUMENT(const KeySeq **o_arg);		  /// &lt;ARGUMENT&gt;
	void load_ARGUMENT(StrExprArg *o_arg);			  /// &lt;ARGUMENT&gt;
	void load_ARGUMENT(WindowMonitorFromType *o_arg); /// &lt;ARGUMENT&gt;
	void load_ARGUMENT(Setting::ComboDetectorMode *o_arg); /// &lt;ARGUMENT&gt;
	KeySeq *load_KEY_SEQUENCE(
		const tstringi &i_name = _T(""), bool i_isInParen = false,
		Modifier::Type i_mode = Modifier::Type_KEYSEQ); /// &lt;KEY_SEQUENCE&gt;
	void load_KEY_ASSIGN();								/// &lt;KEY_ASSIGN&gt;
	void load_EVENT_ASSIGN();							/// &lt;EVENT_ASSIGN&gt;
	void load_MODIFIER_ASSIGNMENT();					/// &lt;MODIFIER_ASSIGN&gt;
	void load_LOCK_ASSIGNMENT();						/// &lt;LOCK_ASSIGN&gt;
	void load_KEYSEQ_DEFINITION();						/// &lt;KEYSEQ_DEFINITION&gt;
	void loadComboDefinition();							/// combo rule
	void loadTapHoldDefinition();						/// taphold rule
	void loadTapDanceDefinition();						/// tapdance rule
	KeySeq *loadSingleKeySequence();					/// single key-sequence helper
	Modifier load_MODIFIER_for_rule();					/// parse optional modifier prefix for combo/taphold/tapdance

	/// load
	void load(const tstringi &i_filename);

	/// is the filename readable ?
	bool isReadable(const tstringi &i_filename, int i_debugLevel = 1) const;

	/// get filename
	bool getFilename(const tstringi &i_name,
					 tstringi *o_path, int i_debugLevel = 1) const;

public:
	///
	SettingLoader(SyncObject *i_soLog, tostream *i_log);

	/// load setting
	bool load(Setting *o_setting, const tstringi &i_filename = _T(""));
};

/// get home directory path
typedef std::list<tstringi> HomeDirectories;
extern void getHomeDirectories(HomeDirectories *o_path);

#endif // !_SETTING_H
