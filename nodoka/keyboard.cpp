//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting.cpp

#include "keyboard.h"

#include <algorithm>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Key

// add a name or an alias of key
void Key::addName(const tstringi &i_name)
{
	m_names.push_back(i_name);
}

// add a scan code
void Key::addScanCode(const ScanCode &i_sc)
{
	ASSERT(m_scanCodesSize < MAX_SCAN_CODES_SIZE);
	m_scanCodes[m_scanCodesSize++] = i_sc;
}

// initializer
Key &Key::initialize()
{
	m_names.clear();
	m_isPressed = false;
	m_isPressedOnWin32 = false;
	m_isPressedByAssign = false;
	m_scanCodesSize = 0;
	return *this;
}

// equation by name
bool Key::operator==(const tstringi &i_name) const
{
	return std::find(m_names.begin(), m_names.end(), i_name) != m_names.end();
}

// is the scan code of this key ?
bool Key::isSameScanCode(const Key &i_key) const
{
	if (m_scanCodesSize != i_key.m_scanCodesSize)
		return false;
	return isPrefixScanCode(i_key);
}

// is the key's scan code the prefix of this key's scan code ?
bool Key::isPrefixScanCode(const Key &i_key) const
{
	for (size_t i = 0; i < i_key.m_scanCodesSize; ++i)
		if (m_scanCodes[i] != i_key.m_scanCodes[i])
			return false;
	return true;
}

// stream output
tostream &operator<<(tostream &i_ost, const Key &i_mk)
{
	return i_ost << i_mk.getName();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Modifier

Modifier::Modifier()
	: m_modifiers(0),
	  m_dontcares(0)
{
	ASSERT(Type_end <= (sizeof(MODIFIERS) * 8));
	static const Type defaultDontCare[] =
		{
			Type_Up,
			Type_Down,
			Type_Repeat,
			Type_ImeLock,
			Type_ImeComp,
			Type_NumLock,
			Type_CapsLock,
			Type_ScrollLock,
			Type_KanaLock,
			Type_Maximized,
			Type_Minimized,
			Type_MdiMaximized,
			Type_MdiMinimized,
			Type_Touchpad,
			Type_TouchpadSticky,
			Type_TouchpadL,
			Type_TouchpadLSticky,
			Type_TouchpadR,
			Type_TouchpadRSticky,
			Type_Lock0,
			Type_Lock1,
			Type_Lock2,
			Type_Lock3,
			Type_Lock4,
			Type_Lock5,
			Type_Lock6,
			Type_Lock7,
			Type_Lock8,
			Type_Lock9,
			Type_LockA,
			Type_LockB,
			Type_LockC,
			Type_LockD,
			Type_LockE,
			Type_LockF,
			Type_Keyboard0,
			Type_Keyboard1,
			Type_Keyboard2,
			Type_Keyboard3,
			Type_Keyboard4,
			Type_Keyboard5,
			Type_Keyboard6,
			Type_Keyboard7,
			Type_ImeCandi,
			Type_Harf,
			Type_Katakana,
			Type_Native,
			Type_DP,
		};
	for (size_t i = 0; i < NUMBER_OF(defaultDontCare); ++i)
		dontcare(defaultDontCare[i]);
}

// add m's modifiers where this dontcare
void Modifier::add(const Modifier &i_m)
{
	for (int i = 0; i < Type_end; ++i)
	{
		if (isDontcare(static_cast<Modifier::Type>(i)))
			if (!i_m.isDontcare(static_cast<Modifier::Type>(i)))
				if (i_m.isPressed(static_cast<Modifier::Type>(i)))
					press(static_cast<Modifier::Type>(i));
				else
					release(static_cast<Modifier::Type>(i));
	}
}

// stream output
tostream &operator<<(tostream &i_ost, const Modifier &i_m)
{
	struct Mods
	{
		Modifier::Type m_mt;
		const _TCHAR *m_symbol;
	};

	const static Mods mods[] =
		{
			{Modifier::Type_Up, _T("U-")},
			{Modifier::Type_Down, _T("D-")},
			{Modifier::Type_Shift, _T("S-")},
			{Modifier::Type_Alt, _T("A-")},
			{Modifier::Type_Control, _T("C-")},
			{Modifier::Type_Windows, _T("W-")},
			{Modifier::Type_Repeat, _T("R-")},
			{Modifier::Type_ImeLock, _T("IL-")},
			{Modifier::Type_ImeComp, _T("IC-")},
			{Modifier::Type_ImeComp, _T("I-")},
			{Modifier::Type_NumLock, _T("NL-")},
			{Modifier::Type_CapsLock, _T("CL-")},
			{Modifier::Type_ScrollLock, _T("SL-")},
			{Modifier::Type_KanaLock, _T("KL-")},
			{Modifier::Type_Maximized, _T("MAX-")},
			{Modifier::Type_Minimized, _T("MIN-")},
			{Modifier::Type_MdiMaximized, _T("MMAX-")},
			{Modifier::Type_MdiMinimized, _T("MMIN-")},
			{Modifier::Type_Touchpad, _T("T-")},
			{Modifier::Type_TouchpadSticky, _T("TS-")},
			{Modifier::Type_TouchpadL, _T("TL-")},
			{Modifier::Type_TouchpadLSticky, _T("TLS-")},
			{Modifier::Type_TouchpadR, _T("TR-")},
			{Modifier::Type_TouchpadRSticky, _T("TRS-")},
			{Modifier::Type_Mod0, _T("M0-")},
			{Modifier::Type_Mod1, _T("M1-")},
			{Modifier::Type_Mod2, _T("M2-")},
			{Modifier::Type_Mod3, _T("M3-")},
			{Modifier::Type_Mod4, _T("M4-")},
			{Modifier::Type_Mod5, _T("M5-")},
			{Modifier::Type_Mod6, _T("M6-")},
			{Modifier::Type_Mod7, _T("M7-")},
			{Modifier::Type_Mod8, _T("M8-")},
			{Modifier::Type_Mod9, _T("M9-")},
			{Modifier::Type_Lock0, _T("L0-")},
			{Modifier::Type_Lock1, _T("L1-")},
			{Modifier::Type_Lock2, _T("L2-")},
			{Modifier::Type_Lock3, _T("L3-")},
			{Modifier::Type_Lock4, _T("L4-")},
			{Modifier::Type_Lock5, _T("L5-")},
			{Modifier::Type_Lock6, _T("L6-")},
			{Modifier::Type_Lock7, _T("L7-")},
			{Modifier::Type_Lock8, _T("L8-")},
			{Modifier::Type_Lock9, _T("L9-")},
			{Modifier::Type_LockA, _T("LA-")},
			{Modifier::Type_LockB, _T("LB-")},
			{Modifier::Type_LockC, _T("LC-")},
			{Modifier::Type_LockD, _T("LD-")},
			{Modifier::Type_LockE, _T("LE-")},
			{Modifier::Type_LockF, _T("LF-")},
			{Modifier::Type_Keyboard0, _T("K0-")},
			{Modifier::Type_Keyboard1, _T("K1-")},
			{Modifier::Type_Keyboard2, _T("K2-")},
			{Modifier::Type_Keyboard3, _T("K3-")},
			{Modifier::Type_Keyboard4, _T("K4-")},
			{Modifier::Type_Keyboard5, _T("K5-")},
			{Modifier::Type_Keyboard6, _T("K6-")},
			{Modifier::Type_Keyboard7, _T("K7-")},
			{Modifier::Type_ImeCandi, _T("IW-")},
			{Modifier::Type_Harf, _T("IH-")},
			{Modifier::Type_Katakana, _T("IK-")},
			{Modifier::Type_Native, _T("IJ-")},
			{Modifier::Type_DP, _T("DP-")},

		};

	for (size_t i = 0; i < NUMBER_OF(mods); ++i)
		if (!i_m.isDontcare(mods[i].m_mt) && i_m.isPressed(mods[i].m_mt))
			i_ost << mods[i].m_symbol;
#if 0
		else if (!i_m.isDontcare(mods[i].m_mt) && i_m.isPressed(mods[i].m_mt))
			i_ost << _T("~") << mods[i].m_symbol;
		else
			i_ost << _T("*") << mods[i].m_symbol;
#endif

	return i_ost;
}

/// stream output
tostream &operator<<(tostream &i_ost, Modifier::Type i_type)
{
	const _TCHAR *modNames[] =
		{
			_T("Shift"),
			_T("Control"),
			_T("Alt"),
			_T("Windows"),
			_T("Up"),
			_T("Down"),
			_T("Repeat"),
			_T("ImeLock"),
			_T("ImeComp"),
			_T("NumLock"),
			_T("CapsLock"),
			_T("ScrollLock"),
			_T("KanaLock"),
			_T("Maximized"),
			_T("Minimized"),
			_T("MdiMaximized"),
			_T("MdiMinimized"),
			_T("Touchpad"),
			_T("TouchpadSticky"),
			_T("TouchpadL"),
			_T("TouchpadLSticky"),
			_T("TouchpadR"),
			_T("TouchpadRSticky"),
			_T("Mod0"),
			_T("Mod1"),
			_T("Mod2"),
			_T("Mod3"),
			_T("Mod4"),
			_T("Mod5"),
			_T("Mod6"),
			_T("Mod7"),
			_T("Mod8"),
			_T("Mod9"),
			_T("Lock0"),
			_T("Lock1"),
			_T("Lock2"),
			_T("Lock3"),
			_T("Lock4"),
			_T("Lock5"),
			_T("Lock6"),
			_T("Lock7"),
			_T("Lock8"),
			_T("Lock9"),
			_T("LockA"),
			_T("LockB"),
			_T("LockC"),
			_T("LockD"),
			_T("LockE"),
			_T("LockF"),
		};

	int i = static_cast<int>(i_type);
	if (0 <= i && i < NUMBER_OF(modNames))
		i_ost << modNames[i];

	return i_ost;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ModifiedKey

// stream output
tostream &operator<<(tostream &i_ost, const ModifiedKey &i_mk)
{
	if (i_mk.m_key)
		i_ost << i_mk.m_modifier << *i_mk.m_key;
	return i_ost;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Keyboard::KeyIterator

Keyboard::KeyIterator::KeyIterator(Keys *i_hashedKeys, size_t i_hashedKeysSize)
	: m_hashedKeys(i_hashedKeys),
	  m_hashedKeysSize(i_hashedKeysSize),
	  m_i((*m_hashedKeys).begin())
{
	if ((*m_hashedKeys).empty())
	{
		do
		{
			--m_hashedKeysSize;
			++m_hashedKeys;
		} while (0 < m_hashedKeysSize && (*m_hashedKeys).empty());
		if (0 < m_hashedKeysSize)
			m_i = (*m_hashedKeys).begin();
	}
}

// Next Scancode
void Keyboard::KeyIterator::next()
{
	if (m_hashedKeysSize == 0)
		return;
	++m_i;
	if (m_i == (*m_hashedKeys).end())
	{
		do
		{
			--m_hashedKeysSize;
			++m_hashedKeys;
		} while (0 < m_hashedKeysSize && (*m_hashedKeys).empty());
		if (0 < m_hashedKeysSize)
			m_i = (*m_hashedKeys).begin();
	}
}

// Current Scancode
Key *Keyboard::KeyIterator::operator*()
{
	if (m_hashedKeysSize == 0)
		return NULL;
	return &*m_i;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Keyboard

// get press key's Scancode list
Keyboard::Keys &Keyboard::getKeys(const Key &i_key)
{
	ASSERT(1 <= i_key.getScanCodesSize());
	return m_hashedKeys[i_key.getScanCodes()->m_scan % HASHED_KEYS_SIZE];
}

// add a key
void Keyboard::addKey(const Key &i_key)
{
	getKeys(i_key).push_front(i_key);
}

// add a key name alias
void Keyboard::addAlias(const tstringi &i_aliasName, Key *i_key)
{
	m_aliases.insert(Aliases::value_type(i_aliasName, i_key));
}

// add substitute
void Keyboard::addSubstitute(const ModifiedKey &i_mkeyFrom,
							 const ModifiedKey &i_mkeyTo)
{
	m_substitutes.push_front(Substitute(i_mkeyFrom, i_mkeyTo));
}

// add a modifier key
void Keyboard::addModifier(Modifier::Type i_mt, Key *i_key)
{
	ASSERT((int)i_mt < (int)Modifier::Type_BASIC);
	if (std::find(m_mods[i_mt].begin(), m_mods[i_mt].end(), i_key) != m_mods[i_mt].end())
		return; // already added
	m_mods[i_mt].push_back(i_key);
}

// search a key
Key *Keyboard::searchKey(const Key &i_key)
{
	Keys &keys = getKeys(i_key);
	for (Keys::iterator i = keys.begin(); i != keys.end(); ++i)
		if ((*i).isSameScanCode(i_key))
			return &*i;
	return NULL;
}

// search a key (of which the key's scan code is the prefix)
Key *Keyboard::searchPrefixKey(const Key &i_key)
{
	Keys &keys = getKeys(i_key);
	for (Keys::iterator i = keys.begin(); i != keys.end(); ++i)
		if ((*i).isPrefixScanCode(i_key))
			return &*i;
	return NULL;
}

// search a key by name
Key *Keyboard::searchKey(const tstringi &i_name)
{
	Aliases::iterator i = m_aliases.find(i_name);
	if (i != m_aliases.end())
		return (*i).second;
	return searchKeyByNonAliasName(i_name);
}

// search a key by non-alias name
Key *Keyboard::searchKeyByNonAliasName(const tstringi &i_name)
{
	for (int j = 0; j < HASHED_KEYS_SIZE; ++j)
	{
		Keys &keys = m_hashedKeys[j];
		Keys::iterator i = std::find(keys.begin(), keys.end(), i_name);
		if (i != keys.end())
			return &*i;
	}
	return NULL;
}

/// search a substitute
ModifiedKey Keyboard::searchSubstitute(const ModifiedKey &i_mkey)
{
	for (Substitutes::const_iterator
			 i = m_substitutes.begin();
		 i != m_substitutes.end(); ++i)
		if (i->m_mkeyFrom.m_key == i_mkey.m_key &&
			i->m_mkeyFrom.m_modifier.doesMatch(i_mkey.m_modifier))
			return i->m_mkeyTo;
	return ModifiedKey(); // not found (.m_mkey is NULL)
}
