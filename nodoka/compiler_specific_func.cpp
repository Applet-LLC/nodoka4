//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// compiler_specific_func.cpp
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#include "compiler_specific_func.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Microsoft Visual C++ 6.0

#if defined(_MSC_VER)

// get compiler version string
tstring getCompilerVersionString()
{
	TCHAR buf[200];
	_sntprintf_s(buf, NUMBER_OF(buf), _TRUNCATE,
				 _T("Visual Studio 2022 v17.%d  (MSVC %d.%d.%d)"),
				 _MSC_VER % 100 - 30,
				 _MSC_VER / 100,
				 _MSC_VER % 100,
				 _MSC_FULL_VER % 100000);
	return tstring(buf);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Borland C++ 5.5.1

#elif defined(__BORLANDC__)

// get compiler version string
tstring getCompilerVersionString()
{
	TCHAR buf[100];
	_sntprintf(buf, NUMBER_OF(buf), _T("Borland C++ %d.%d.%d"),
			   __BORLANDC__ / 0x100,
			   __BORLANDC__ / 0x10 % 0x10,
			   __BORLANDC__ % 0x10);
	return tstring(buf);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// unknown

#else
#error "I don't know the details of this compiler... Plz hack."

#endif
