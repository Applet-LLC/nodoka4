//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// dlgsetting.cpp

#include "misc.h"

#include "nodoka.h"
#include "nodokarc.h"
#include "registry.h"
#include "stringtool.h"
#include "windowstool.h"
#include "setting.h"
#include "dlgeditsetting.h"
#include "layoutmanager.h"

#include <commctrl.h>
#include <windowsx.h>
#include <shlwapi.h>

///
class DlgSetting : public LayoutManager
{
	HWND m_hwndNodokaPaths; ///

	///
	Registry m_reg;

	typedef DlgEditSettingData Data; ///

	///
	void insertItem(int i_index, const Data &i_data)
	{
		LVITEM item;
		item.mask = LVIF_TEXT;
		item.iItem = i_index;

		item.iSubItem = 0;
		item.pszText = const_cast<_TCHAR *>(i_data.m_name.c_str());
		CHECK_TRUE(ListView_InsertItem(m_hwndNodokaPaths, &item) != -1);

		ListView_SetItemText(m_hwndNodokaPaths, i_index, 1,
							 const_cast<_TCHAR *>(i_data.m_filename.c_str()));
		ListView_SetItemText(m_hwndNodokaPaths, i_index, 2,
							 const_cast<_TCHAR *>(i_data.m_symbols.c_str()));
	}

	///
	void setItem(int i_index, const Data &i_data)
	{
		ListView_SetItemText(m_hwndNodokaPaths, i_index, 0,
							 const_cast<_TCHAR *>(i_data.m_name.c_str()));
		ListView_SetItemText(m_hwndNodokaPaths, i_index, 1,
							 const_cast<_TCHAR *>(i_data.m_filename.c_str()));
		ListView_SetItemText(m_hwndNodokaPaths, i_index, 2,
							 const_cast<_TCHAR *>(i_data.m_symbols.c_str()));
	}

	///
	void getItem(int i_index, Data *o_data)
	{
		_TCHAR buf[GANA_MAX_PATH];
		LVITEM item;
		item.mask = LVIF_TEXT;
		item.iItem = i_index;
		item.pszText = buf;
		item.cchTextMax = NUMBER_OF(buf);

		item.iSubItem = 0;
		CHECK_TRUE(ListView_GetItem(m_hwndNodokaPaths, &item));
		o_data->m_name = item.pszText;

		item.iSubItem = 1;
		CHECK_TRUE(ListView_GetItem(m_hwndNodokaPaths, &item));
		o_data->m_filename = item.pszText;

		item.iSubItem = 2;
		CHECK_TRUE(ListView_GetItem(m_hwndNodokaPaths, &item));
		o_data->m_symbols = item.pszText;
	}

	///
	void setSelectedItem(int i_index)
	{
		ListView_SetItemState(m_hwndNodokaPaths, i_index,
							  LVIS_SELECTED, LVIS_SELECTED);
	}

	void setSelectedItem_EnsureVisible(int i_index)
	{
		ListView_EnsureVisible(m_hwndNodokaPaths, i_index, FALSE);
	}
	///
	int getSelectedItem()
	{
		if (ListView_GetSelectedCount(m_hwndNodokaPaths) == 0)
			return -1;
		for (int i = 0;; ++i)
		{
			if (ListView_GetItemState(m_hwndNodokaPaths, i, LVIS_SELECTED))
				return i;
		}
	}

	// determine processor architecture
	void getSysInfo(SYSTEM_INFO *sysInfo)
	{
		static bool first = true;
		static void(WINAPI * pGetNativeSystemInfo)(LPSYSTEM_INFO);
		if (first)
		{
			first = false;
			*(FARPROC *)&pGetNativeSystemInfo =
				GetProcAddress(LoadLibrary(_T("kernel32")), "GetNativeSystemInfo");
		}
		if (pGetNativeSystemInfo)
		{
			pGetNativeSystemInfo(sysInfo);
			return;
		}
		GetSystemInfo(sysInfo);
	}

	///
	BOOL isW2K()
	{
		// W2Kかどうか確認する。
		OSVERSIONINFO ver;
		ZeroMemory(&ver, sizeof(OSVERSIONINFO));
		ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx(&ver);

		if (ver.dwMajorVersion == 5)
			if (ver.dwMinorVersion == 0)
				return TRUE;
		return FALSE;
	}

	BOOL checkDotNet()
	{
		// get ngen.exe path
		_TCHAR winPath[MAX_PATH];
		_TCHAR frameworkPath[MAX_PATH];
		_TCHAR exeLine[MAX_PATH];

		GetWindowsDirectory(winPath, NUMBER_OF(winPath));

#ifdef _WIN64
		_sntprintf_s(frameworkPath, NUMBER_OF(frameworkPath), _TRUNCATE, _T("%s\\%s"),
					 winPath, _T("\\Microsoft.NET\\Framework64\\v4.0.30319"));
#else
		_sntprintf_s(frameworkPath, NUMBER_OF(frameworkPath), _TRUNCATE, _T("%s\\%s"),
					 winPath, _T("\\Microsoft.NET\\Framework\\v4.0.30319"));
#endif
		_sntprintf_s(exeLine, NUMBER_OF(exeLine), _TRUNCATE, _T("%s\\%s"),
					 frameworkPath, _T("ngen.exe"));

		// check ngen.exe
		WIN32_FIND_DATA wfd;
		HANDLE hFile = FindFirstFile(exeLine, &wfd);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			FindClose(hFile);
			return FALSE;
		}
		else
		{
			FindClose(hFile);
			return TRUE;
		}
	}

	///
	BOOL CheckVirtualStore(tstringi fullpath)
	{
		// Vistaかどうか確認する。XP, 2000だったら return TRUE;
		OSVERSIONINFO ver;
		ZeroMemory(&ver, sizeof(OSVERSIONINFO));
		ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx(&ver);

		SYSTEM_INFO sysInfo;
		getSysInfo(&sysInfo);

		if (ver.dwMajorVersion < 6)
			return TRUE;
#if 0
		// filename が c:\Program Files\nodoka 以下のものか確認する。
		tstring programfiles_0 = GetEnv(_T("ProgramFiles"));
		tstring programfiles_1 = programfiles_0 + _T("\\nodoka");
		tregex programfiles(programfiles_1);
		tsmatch programfilespath;

		MessageBox(NULL, fullpath.c_str(), L"fullpath", MB_OK);
		MessageBox(NULL, programfiles_1.c_str(), L"programfiles", MB_OK);

		if (!boost::regex_search(fullpath, programfilespath, programfiles))
			return TRUE;

		MessageBox(NULL, L"Is Program Files file", L"programfiles", MB_OK);
#endif
		// VirtualStoreのパスを取得する。
		TCHAR VirtualStore[GANA_MAX_PATH] = L"\0";
		if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
			wsprintfW(VirtualStore, L"%s\\VirtualStore\\Program Files (x86)\\nodoka", GetEnv(_T("LOCALAPPDATA")));
		else
			wsprintfW(VirtualStore, L"%s\\VirtualStore\\Program Files\\nodoka", GetEnv(_T("LOCALAPPDATA")));

#if 0
		// filenameからPathを取り除いて、VirtualStoreを付ける。
		TCHAR VirtualStoreFile[GANA_MAX_PATH] = L"\0";
		tregex reg(_T("^(.*)\\\\[^\\\\]*$"));
		tsmatch what;
		//		tstringi path(buf);     TCHAR->tsringi
		if (boost::regex_search(fullpath, what, reg))
			wsprintfW(VirtualStoreFile, L"%s\\%s", VirtualStore, what.str(1));
		else
			wsprintfW(VirtualStoreFile, L"%s\\%s", VirtualStore, fullpath.c_str());

		MessageBox(NULL, VirtualStoreFile, L"VirtualStoreFile", MB_OK);
		// fileの存在を確認する。
		WIN32_FIND_DATA  wfd;
		HANDLE hFile = FindFirstFile(VirtualStoreFile, &wfd );
#endif

		WIN32_FIND_DATA wfd;
		HANDLE hFile = FindFirstFile(VirtualStore, &wfd);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			FindClose(hFile);
			return TRUE;
		}
		else
		{
			FindClose(hFile);
			// VirtualStore に nodokaのフォルダがあったので、開くかどうか聞いて YESなら開く。
			// VirtualStoreがある限り、FALSEで返すのでファイルオープンは実行できない。
			tstring text = loadString(IDS_virtualNodokaFile);
			tstring title = loadString(IDS_nodoka);
			if (IDYES == MessageBox((HWND)NULL, text.c_str(), title.c_str(), MB_YESNO))
			{
				//DeleteFileW(VirtualStoreFile);
				ShellExecute(NULL, NULL, VirtualStore, NULL, NULL, SW_SHOWNORMAL);
			}
			return FALSE;
		}
	}

public:
	///
	DlgSetting(HWND i_hwnd)
		: LayoutManager(i_hwnd),
		  m_hwndNodokaPaths(NULL),
		  m_reg(NODOKA_REGISTRY_ROOT)
	{
	}

	/// WM_INITDIALOG
	BOOL wmInitDialog(HWND /* i_focus */, LPARAM /* i_lParam */)
	{
		// save m_hwnd;
		m_hwndSetting = m_hwnd;
		setSmallIcon(m_hwnd, IDI_ICON_nodoka);
		setBigIcon(m_hwnd, IDI_ICON_nodoka);

		CHECK_TRUE(m_hwndNodokaPaths = GetDlgItem(m_hwnd, IDC_LIST_nodokaPaths));

		// create list view colmn
		RECT rc;
		GetClientRect(m_hwndNodokaPaths, &rc);

		LVCOLUMN lvc;
		lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
		lvc.fmt = LVCFMT_LEFT;
		lvc.cx = (rc.right - rc.left) / 3;

		tstringi str = loadString(IDS_nodokaPathName);
		lvc.pszText = const_cast<_TCHAR *>(str.c_str());
		CHECK(0 ==, ListView_InsertColumn(m_hwndNodokaPaths, 0, &lvc));
		str = loadString(IDS_nodokaPath);
		lvc.pszText = const_cast<_TCHAR *>(str.c_str());
		CHECK(1 ==, ListView_InsertColumn(m_hwndNodokaPaths, 1, &lvc));
		str = loadString(IDS_nodokaSymbols);
		lvc.pszText = const_cast<_TCHAR *>(str.c_str());
		CHECK(2 ==, ListView_InsertColumn(m_hwndNodokaPaths, 2, &lvc));

		Data data;
		insertItem(0, data); // TODO: why ?

		// set list view
		tregex split(_T("^([^;]*);([^;]*);(.*)$"));
		tstringi dot_nodoka;
		int i;
		for (i = 0; i < MAX_NODOKA_REGISTRY_ENTRIES; ++i)
		{
			_TCHAR buf[100];
			_sntprintf_s(buf, NUMBER_OF(buf), _TRUNCATE, _T(".nodoka%d"), i);
			if (!m_reg.read(buf, &dot_nodoka))
				break;

			tsmatch what;
			if (boost::regex_match(dot_nodoka, what, split))
			{
				data.m_name = what.str(1);
				data.m_filename = what.str(2);
				data.m_symbols = what.str(3);
				insertItem(i, data);
			}
		}

		CHECK_TRUE(ListView_DeleteItem(m_hwndNodokaPaths, i)); // TODO: why ?

		// arrange list view size
		ListView_SetColumnWidth(m_hwndNodokaPaths, 0, LVSCW_AUTOSIZE);
		ListView_SetColumnWidth(m_hwndNodokaPaths, 1, LVSCW_AUTOSIZE);
		ListView_SetColumnWidth(m_hwndNodokaPaths, 2, LVSCW_AUTOSIZE);

		ListView_SetExtendedListViewStyle(m_hwndNodokaPaths, LVS_EX_FULLROWSELECT);

		// set selection
		int index;
		m_reg.read(_T(".nodokaIndex"), &index, 0);
		setSelectedItem(index);
		setSelectedItem_EnsureVisible(index);

		if (isW2K() || !(checkDotNet()))
			EnableWindow(GetDlgItem(m_hwnd, IDC_BUTTON_editfile2), FALSE);

		// set layout manager
		typedef LayoutManager LM;
		addItem(GetDlgItem(m_hwnd, IDC_STATIC_nodokaPaths),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_TOP_EDGE,
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_LIST_nodokaPaths),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_TOP_EDGE,
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_up),
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_CENTER,
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_CENTER);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_down),
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_CENTER,
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_CENTER);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_add),
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_edit),
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_delete),
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_editfile),
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDCANCEL),
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_setting_reload),
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_editfile2),
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_LOG),
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_LOG2),
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_PAUSE),
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDOK),
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_CENTER, LM::ORIGIN_BOTTOM_EDGE);
		restrictSmallestSize();
		return TRUE;
	}
	/// WM_CLOSE
	BOOL wmClose()
	{
		EndDialog(m_hwnd, 0);
		return TRUE;
	}

	/// WM_NOTIFY
	BOOL wmNotify(int i_id, NMHDR *i_nmh)
	{
		switch (i_id)
		{
		case IDC_LIST_nodokaPaths:
			if (i_nmh->code == NM_DBLCLK)
				FORWARD_WM_COMMAND(m_hwnd, IDC_BUTTON_edit, NULL, 0, SendMessage);
			return TRUE;
		}
		return TRUE;
	}

	/// WM_COMMAND
	BOOL wmCommand(int /* i_notifyCode */, int i_id, HWND /* i_hwndControl */)
	{
		_TCHAR buf[GANA_MAX_PATH];
		switch (i_id)
		{
		case IDC_BUTTON_up:
		case IDC_BUTTON_down:
		{
			int count = ListView_GetItemCount(m_hwndNodokaPaths);
			if (count < 2)
				return TRUE;
			int index = getSelectedItem();
			if (index < 0 ||
				(i_id == IDC_BUTTON_up && index == 0) ||
				(i_id == IDC_BUTTON_down && index == count - 1))
				return TRUE;

			int target = (i_id == IDC_BUTTON_up) ? index - 1 : index + 1;

			Data dataIndex, dataTarget;
			getItem(index, &dataIndex);
			getItem(target, &dataTarget);
			setItem(index, dataTarget);
			setItem(target, dataIndex);

			setSelectedItem(target);
			return TRUE;
		}

		case IDC_BUTTON_add:
		{
			Data data;
			int index = getSelectedItem();
			if (0 <= index)
				getItem(index, &data);
			if (DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_editSetting),
							   m_hwnd, dlgEditSetting_dlgProc, (LPARAM)&data))
				if (!data.m_name.empty())
				{
					insertItem(0, data);
					setSelectedItem(0);
				}
			return TRUE;
		}

		case IDC_BUTTON_delete:
		{
			int index = getSelectedItem();
			if (0 <= index)
			{
				CHECK_TRUE(ListView_DeleteItem(m_hwndNodokaPaths, index));
				int count = ListView_GetItemCount(m_hwndNodokaPaths);
				if (count == 0)
					;
				else if (count == index)
					setSelectedItem(index - 1);
				else
					setSelectedItem(index);
			}
			return TRUE;
		}

		case IDC_BUTTON_edit:
		{
			Data data;
			int index = getSelectedItem();
			if (index < 0)
				return TRUE;
			getItem(index, &data);
			if (DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_editSetting),
							   m_hwnd, dlgEditSetting_dlgProc, (LPARAM)&data))
			{
				setItem(index, data);
				setSelectedItem(index);
			}
			return TRUE;
		}

		case IDC_BUTTON_editfile2:
		case IDC_BUTTON_editfile:
		{
			Data data;
			BOOL bFlag = FALSE;
			BOOL bNODOKA = FALSE;
			BOOL bNODOKA_FILE = FALSE;
			BOOL bHOME = FALSE;
			BOOL bHOME_FILE = FALSE;
			BOOL bHOMEPATH = FALSE;
			BOOL bHOMEPATH_FILE = FALSE;
			BOOL bUSERPROFILE = FALSE;
			BOOL bUSERPROFILE_FILE = FALSE;
			BOOL bSYSTEM_FILE = FALSE;

			HANDLE hFile;
			WIN32_FIND_DATA wfd;

			const _TCHAR *nodoka;
			const _TCHAR *home;
			const _TCHAR *homedrive;
			const _TCHAR *homepath;
			const _TCHAR *userprofile;
			TCHAR buff_nodoka[GANA_MAX_PATH] = L"\0";
			TCHAR buff_home[GANA_MAX_PATH] = L"\0";
			TCHAR buff_homepath[GANA_MAX_PATH] = L"\0";
			TCHAR buff_userprofile[GANA_MAX_PATH] = L"\0";
			TCHAR buff_userhome[GANA_MAX_PATH] = L"\0";
			TCHAR buff_system[GANA_MAX_PATH] = L"\0";
			TCHAR name[GANA_MAX_PATH] = L"dot.nodoka";
			TCHAR buff_guiname[GANA_MAX_PATH] = L"\0";
			TCHAR guiname[GANA_MAX_PATH] = L"GuiEdit.exe";
			//TCHAR szErr[1000];

			tstring text;
			tstring title;
			bool bGUI = false;

			// update setting reg
			int count = ListView_GetItemCount(m_hwndNodokaPaths);
			int index;
			for (index = 0; index < count; ++index)
			{
				_sntprintf_s(buf, NUMBER_OF(buf), _TRUNCATE, _T(".nodoka%d"), index);
				Data data;
				getItem(index, &data);
				m_reg.write(buf, data.m_name + _T(";") +
									 data.m_filename + _T(";") + data.m_symbols);
			}
			for (;; ++index)
			{
				_sntprintf_s(buf, NUMBER_OF(buf), _TRUNCATE, _T(".nodoka%d"), index);
				if (!m_reg.remove(buf))
					break;
			}
			index = getSelectedItem();
			if (index < 0)
				index = 0;
			m_reg.write(_T(".nodokaIndex"), index);

			if (i_id == IDC_BUTTON_editfile2)
				bGUI = true;

			getItem(index, &data);

			_TCHAR szPath[GANA_MAX_PATH];
			_TCHAR szDrive[_MAX_DRIVE];
			_TCHAR szDir[_MAX_DIR];

			// exeと同じ場所にある dot.nodoka のパスを作る。
			// 同時に、GuiEdit.exe へのパスを作る。

			if (GetModuleFileName(GetModuleHandle(NULL), szPath, NUMBER_OF(szPath)))
			{
				bSYSTEM_FILE = TRUE;
				_wsplitpath_s((const wchar_t *)szPath,
							  (wchar_t *)szDrive, sizeof(szDrive) / sizeof(szDrive[0]),
							  (wchar_t *)szDir, sizeof(szDir) / sizeof(szDir[0]),
							  NULL, 0, NULL, 0);

				wsprintfW(buff_system, L"%s\\%s\\%s", szDrive, szDir, name);
				wsprintfW(buff_guiname, L"%s\\%s\\%s", szDrive, szDir, guiname);
			}

			if ((data.m_filename.empty())) // ファイル名の指定が無い場合、dot.nodokaを探す。
			{
				hFile = FindFirstFile(buff_system, &wfd); // exeと同じ場所にあるか?
				if (hFile == INVALID_HANDLE_VALUE)
				{
					bSYSTEM_FILE = FALSE; // 無かった。
				}
				else
				{
					bFlag = TRUE; // あった。
					data.m_filename = buff_system;
				}

				//wsprintfW(szErr, L"szPath:%s, buff_system:%s, bSYSTEM_FILE:%d, bFlag:%d, data.m_filename:%s", szPath, buff_system, bSYSTEM_FILE, bFlag, data.m_filename.c_str());
				//MessageBox(NULL, szErr, NULL, MB_OK);

				userprofile = GetEnv(_T("USERPROFILE")); // USERPROFILEは?
				if (userprofile)
				{
					bUSERPROFILE = TRUE;
					bUSERPROFILE_FILE = TRUE;
					wsprintfW(buff_userprofile, L"%s\\%s", userprofile, name);
					hFile = FindFirstFile(buff_userprofile, &wfd);
					if (hFile == INVALID_HANDLE_VALUE)
						bUSERPROFILE_FILE = FALSE;
					else
					{
						bFlag = TRUE;
						data.m_filename = buff_userprofile;
					}
				}

				homedrive = GetEnv(_T("HOMEDRIVE")); // HOMEPATHは?
				homepath = GetEnv(_T("HOMEPATH"));
				if (homedrive && homepath)
				{
					bHOMEPATH = TRUE;
					bHOMEPATH_FILE = TRUE;
					wsprintfW(buff_homepath, L"%s%s\\%s", homedrive, homepath, name);
					hFile = FindFirstFile(buff_homepath, &wfd);
					if (hFile == INVALID_HANDLE_VALUE)
						bHOMEPATH_FILE = FALSE;
					else
					{
						bFlag = TRUE;
						data.m_filename = buff_homepath;
					}
				}

				home = GetEnv(_T("HOME")); // HOMEは?
				if (home)
				{
					bHOME = TRUE;
					bHOME_FILE = TRUE;
					wsprintfW(buff_home, L"%s\\%s", home, name);
					hFile = FindFirstFile(buff_home, &wfd);
					if (hFile == INVALID_HANDLE_VALUE)
						bHOME_FILE = FALSE;
					else
					{
						bFlag = TRUE;
						data.m_filename = buff_home;
					}
				}

				nodoka = GetEnv(_T("NODOKA")); // NODOKAは?
				if (nodoka)
				{
					bNODOKA = TRUE;
					bNODOKA_FILE = TRUE;
					wsprintfW(buff_nodoka, L"%s\\%s", nodoka, name);
					hFile = FindFirstFile(buff_nodoka, &wfd);
					if (hFile == INVALID_HANDLE_VALUE)
						bNODOKA_FILE = FALSE;
					else
					{
						bFlag = TRUE;
						data.m_filename = buff_nodoka;
					}
				}

				FindClose(hFile);

				// NODOKA,HOME,HOMEPATH,USERPROFILEのいずれかがあり、どこかにファイルがある場合 コピー不要
				// NODOKA,HOME,HOMEPATH,USERPROFILEのいずれかがあり、どこにもファイルが無い場合  HOME, USERPROFILE, HOMEPATH順にトライ
				// 以下は未実施
				// 環境変数が全く未定義の場合、コピー先が無いので、コピーは実施しない。OSがVistaの場合 警告を出す。
				// 規定のdot.nodokaが無い場合、コピーは実施しない。エラーを出す。

				//wsprintfW(szErr, L"bHOME %d, bHOMEPATH %d, bUSERPROFILE %d, bHOME_FILE %d, bHOMEPATH_FILE %d, bUSERPROFILE_FILE %d, bSYSTEM_FILE %d",
				//	bHOME, bHOMEPATH, bUSERPROFILE, bHOME_FILE, bHOMEPATH_FILE, bUSERPROFILE_FILE, bSYSTEM_FILE);
				//MessageBox(NULL, szErr, NULL, MB_OK);
				if ((bNODOKA || bHOME || bHOMEPATH || bUSERPROFILE) && bSYSTEM_FILE)
				{
					if (bNODOKA_FILE || bHOME_FILE || bHOMEPATH_FILE || bUSERPROFILE_FILE)
					{
						bFlag = TRUE;
					}
					else // 実体がユーザ側にないので、ファイルコピー実施。
					{
						tstring text = loadString(IDS_copyNodokaFile);
						tstring title = loadString(IDS_nodoka);
						if (IDYES == MessageBox((HWND)NULL, text.c_str(), title.c_str(), MB_YESNO))
						{
							if (bNODOKA == TRUE)
							{
								CopyFile(buff_system, buff_nodoka, TRUE);
								SetFileAttributes(buff_nodoka, FILE_ATTRIBUTE_NORMAL);
								data.m_filename = buff_nodoka;
							}
							else
							{
								if (bHOME == TRUE)
								{
									CopyFile(buff_system, buff_home, TRUE);
									SetFileAttributes(buff_home, FILE_ATTRIBUTE_NORMAL);
									data.m_filename = buff_home;
								}
								else
								{
									if (bUSERPROFILE == TRUE)
									{
										CopyFile(buff_system, buff_userprofile, TRUE);
										SetFileAttributes(buff_userprofile, FILE_ATTRIBUTE_NORMAL);
										data.m_filename = buff_userprofile;
									}
									else
									{
										if (bHOMEPATH == TRUE)
										{
											CopyFile(buff_system, buff_homepath, TRUE);
											SetFileAttributes(buff_homepath, FILE_ATTRIBUTE_NORMAL);
											data.m_filename = buff_homepath;
										}
									}
								}
							}
						}
					}
				}
			}
			else // ファイル名の指定があった。
			{
				bFlag = TRUE;
			}

			if (bFlag == TRUE)
			{
				// VirtualStoreに Nodokaフォルダがあったら、それを開くか確認し、VirtualStoreがある場合は、ファイルオープンはキャンセルにする。
				if (CheckVirtualStore(data.m_filename))
				{
					// VirtualStore cheakはパスしたので、ファイルオープンしてみる。
					// 現在指定されているパスで開いてみて、だめならNODOKA,HOME,USERPROFILE,HOMEPATHの順で開いてみる。
					int errnum;
					if (bGUI == false)
					{
						errnum = (int)ShellExecute(NULL, NULL, data.m_filename.c_str(), NULL, NULL, SW_SHOWNORMAL);

						if (errnum <= 32)
						{
							// 失敗したので、新規ファイルとなるよう USERPROFILE,HOME,HOMDEDRIVE/HOMEPATHを調べて、先頭につけて開いてみる。
							// まず .nodoka に関連付けられているエディタの実行ファイル名を取得する。
							DWORD dwOut = GANA_MAX_PATH;
							TCHAR pszNodokaEditExeFile[GANA_MAX_PATH];
							HRESULT hr = ::AssocQueryString(ASSOCF_NOTRUNCATE, ASSOCSTR_EXECUTABLE, _T(".nodoka"), _T("open"), pszNodokaEditExeFile, &dwOut);

							nodoka = GetEnv(_T("NODOKA"));
							if (nodoka)
							{
								wsprintfW(buff_userhome, L"%s\\%s", nodoka, data.m_filename.c_str());
								ShellExecute(NULL, _T("open"), pszNodokaEditExeFile, buff_userhome, nodoka, SW_SHOWNORMAL);
							}
							else
							{
								home = GetEnv(_T("HOME"));
								if (home)
								{
									wsprintfW(buff_userhome, L"%s\\%s", home, data.m_filename.c_str());
									ShellExecute(NULL, _T("open"), pszNodokaEditExeFile, buff_userhome, home, SW_SHOWNORMAL);
								}
								else
								{
									userprofile = GetEnv(_T("USERPROFILE"));
									if (userprofile)
									{
										wsprintfW(buff_userhome, L"%s\\%s", userprofile, data.m_filename.c_str());
										ShellExecute(NULL, _T("open"), pszNodokaEditExeFile, buff_userhome, userprofile, SW_SHOWNORMAL);
									}
									else
									{
										homedrive = GetEnv(_T("HOMEDRIVE"));
										homepath = GetEnv(_T("HOMEPATH"));
										if (homedrive && homepath)
										{
											wsprintfW(buff_userhome, L"%s%s\\%s", homedrive, homepath, data.m_filename.c_str());
											wsprintfW(buff_homepath, L"%s%s", homedrive, homepath);
											ShellExecute(NULL, _T("open"), pszNodokaEditExeFile, buff_userhome, buff_homepath, SW_SHOWNORMAL);
										}
										else
										{
											// NODOKA,USERPROFILE,HOME,HOMDEDRIVE/HOMEPATHのいずれも無かったので、エラー表示する。
											text = loadString(IDS_errorNodokaFile);
											title = loadString(IDS_nodoka);
											MessageBox((HWND)NULL, text.c_str(), title.c_str(), MB_OK | MB_ICONSTOP);
										}
									}
								}
							}
						}
					}
					else
					{
						// gui_edit
						wsprintfW(buff_userhome, L"%s", data.m_filename.c_str());
						hFile = FindFirstFile(buff_userhome, &wfd);
						if (hFile == INVALID_HANDLE_VALUE)
						{
							nodoka = GetEnv(_T("NODOKA"));
							wsprintfW(buff_userhome, L"%s\\%s", nodoka, data.m_filename.c_str());
							if (nodoka == NULL)
							{
								home = GetEnv(_T("HOME"));
								wsprintfW(buff_userhome, L"%s\\%s", home, data.m_filename.c_str());
								if (home == NULL)
								{
									userprofile = GetEnv(_T("USERPROFILE"));
									wsprintfW(buff_userhome, L"%s\\%s", userprofile, data.m_filename.c_str());
									if (userprofile == NULL)
									{
										homedrive = GetEnv(_T("HOMEDRIVE"));
										homepath = GetEnv(_T("HOMEPATH"));
										wsprintfW(buff_userhome, L"%s%s\\%s", homedrive, homepath, data.m_filename.c_str());
										if (homedrive == NULL || homepath == NULL)
										{
											text = loadString(IDS_errorNodokaFile);
											title = loadString(IDS_nodoka);
											MessageBox((HWND)NULL, text.c_str(), title.c_str(), MB_OK | MB_ICONSTOP);
										}
									}
								}
							}
						}
						//MessageBox(NULL, buff_userhome, L"NODOKA", MB_OK);
						wsprintfW(name, L"\"%s\"", buff_userhome);
						errnum = (int)ShellExecute(NULL, NULL, buff_guiname, name, NULL, SW_SHOWNORMAL);
					}
				}
			}
			return TRUE;
		}

		case IDOK:
		{
			int count = ListView_GetItemCount(m_hwndNodokaPaths);
			int index;
			for (index = 0; index < count; ++index)
			{
				_sntprintf_s(buf, NUMBER_OF(buf), _TRUNCATE, _T(".nodoka%d"), index);
				Data data;
				getItem(index, &data);
				m_reg.write(buf, data.m_name + _T(";") +
									 data.m_filename + _T(";") + data.m_symbols);
			}
			for (;; ++index)
			{
				_sntprintf_s(buf, NUMBER_OF(buf), _TRUNCATE, _T(".nodoka%d"), index);
				if (!m_reg.remove(buf))
					break;
			}
			index = getSelectedItem();
			if (index < 0)
				index = 0;
			m_reg.write(_T(".nodokaIndex"), index);
			EndDialog(m_hwnd, 1);
			return TRUE;
		}

		case IDC_BUTTON_setting_reload:
		{
			int count = ListView_GetItemCount(m_hwndNodokaPaths);
			int index;
			for (index = 0; index < count; ++index)
			{
				_sntprintf_s(buf, NUMBER_OF(buf), _TRUNCATE, _T(".nodoka%d"), index);
				Data data;
				getItem(index, &data);
				m_reg.write(buf, data.m_name + _T(";") +
									 data.m_filename + _T(";") + data.m_symbols);
			}
			for (;; ++index)
			{
				_sntprintf_s(buf, NUMBER_OF(buf), _TRUNCATE, _T(".nodoka%d"), index);
				if (!m_reg.remove(buf))
					break;
			}
			index = getSelectedItem();
			if (index < 0)
				index = 0;
			m_reg.write(_T(".nodokaIndex"), index);

			HWND i_hwnd = FindWindow(_T("nodokaTasktray"), NULL);
			SendMessage(i_hwnd, WM_COMMAND, MAKELONG(ID_MENUITEM_reload, 0), 0);

			return TRUE;
		}
		case IDC_BUTTON_LOG:
		{
			HWND i_hwnd = FindWindow(_T("nodokaTasktray"), NULL);
			SendMessage(i_hwnd, WM_COMMAND, MAKELONG(ID_MENUITEM_log, 0), 0);

			return TRUE;
		}
		case IDC_BUTTON_LOG2:
		{
			HWND i_hwnd = FindWindow(_T("nodokaTasktray"), NULL);
			SendMessage(i_hwnd, WM_COMMAND, MAKELONG(ID_MENUITEM_investigate, 0), 0);

			return TRUE;
		}
		case IDC_BUTTON_PAUSE:
		{
			HWND i_hwnd = FindWindow(_T("nodokaTasktray"), NULL);
			SendMessage(i_hwnd, WM_COMMAND, MAKELONG(ID_MENUITEM_disable, 0), 0);

			return TRUE;
		}

		case IDCANCEL:
		{
			CHECK_TRUE(EndDialog(m_hwnd, 0));
			return TRUE;
		}
		}
		return FALSE;
	}
};

//
INT_PTR CALLBACK dlgSetting_dlgProc(HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam)
{
	DlgSetting *wc;
	getUserData(i_hwnd, &wc);
	if (!wc)
		switch (i_message)
		{
		case WM_INITDIALOG:
			wc = setUserData(i_hwnd, new DlgSetting(i_hwnd));
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
		case WM_NOTIFY:
			return wc->wmNotify(static_cast<int>(i_wParam),
								reinterpret_cast<NMHDR *>(i_lParam));
		default:
			return wc->defaultWMHandler(i_message, i_wParam, i_lParam);
		}
	return FALSE;
}
