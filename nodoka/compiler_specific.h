//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// compiler_specific.h
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#ifndef _COMPILER_SPECIFIC_H
#define _COMPILER_SPECIFIC_H

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Microsoft Visual C++ 6.0

#if defined(_MSC_VER)

// C4061 enum 'identifier' is not handled by case label
// C4100 argument 'identifier' is not used
// C4132 const 'object' must be initialized
// C4552 'operator' : operator has no effect
// C4701 local variable 'name' may be uninitialized
// C4706 condition is a result of a assign
// C4786 identifier is truncated into 255 chars (in debug information)
#pragma warning(disable : 4061 4100 4132 4552 4701 4706 4786)

#define setmode _setmode
//#define for if (false); else for

#define stati64_t _stati64

#else
#error "I don't know the details of this compiler... Plz hack."

#endif

#endif // _COMPILER_SPECIFIC_H
