//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// nodoka.h
// Copyright 2008-2026 applet <applet@bp.iij4u.or.jp>
// License: EPL-2.0 - https://www.eclipse.org/legal/epl-2.0/

#ifndef _NODOKA_H
#define _NODOKA_H

///
#define NODOKA_REGISTRY_ROOT HKEY_CURRENT_USER, _T("Software\\appletkan\\nodoka")
#define NODOKA_REGISTRY_ROOT2 HKEY_CURRENT_USER, _T("Software\\AppDataLow\\Software\\appletkan\\nodoka")
#define NODOKA_REGISTRY_ROOT3 HKEY_CURRENT_USER, _T("Software\\AppDataLow\\Software\\appletkan")
#define KBDCLASS_REGISTRY_ENUM HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\services\\kbdclass\\Enum")
///
#define MUTEX_NODOKA_EXCLUSIVE_RUNNING _T("{46269F4D-D560-40f9-B38B-DB5E280FEF47}")
#define MUTEX_NODOKA_HELPER_EXCLUSIVE_RUNNING _T("{ 46269F4D-D560-40f9-B38B-DB5E280FEF47}")

///
#define MAX_NODOKA_REGISTRY_ENTRIES 256

#endif // _NODOKA_H
