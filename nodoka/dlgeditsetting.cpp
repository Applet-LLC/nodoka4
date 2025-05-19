//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// dlgeditsetting.cpp

#include "misc.h"
#include "nodokarc.h"
#include "windowstool.h"
#include "dlgeditsetting.h"
#include "layoutmanager.h"
#include <windowsx.h>

///
class DlgEditSetting : public LayoutManager
{
	HWND m_hwndNodokaPathName; ///
	HWND m_hwndNodokaPath;	 ///
	HWND m_hwndSymbols;		   ///

	DlgEditSettingData *m_data; ///

public:
	///
	DlgEditSetting(HWND i_hwnd)
		: LayoutManager(i_hwnd),
		  m_hwndNodokaPathName(NULL),
		  m_hwndNodokaPath(NULL),
		  m_hwndSymbols(NULL),
		  m_data(NULL)
	{
	}

	/// WM_INITDIALOG
	BOOL wmInitDialog(HWND /* focus */, LPARAM i_lParam)
	{
		m_data = reinterpret_cast<DlgEditSettingData *>(i_lParam);

		setSmallIcon(m_hwnd, IDI_ICON_nodoka);
		setBigIcon(m_hwnd, IDI_ICON_nodoka);

		CHECK_TRUE(m_hwndNodokaPathName = GetDlgItem(m_hwnd, IDC_EDIT_nodokaPathName));
		CHECK_TRUE(m_hwndNodokaPath = GetDlgItem(m_hwnd, IDC_EDIT_nodokaPath));
		CHECK_TRUE(m_hwndSymbols = GetDlgItem(m_hwnd, IDC_EDIT_symbols));

		SetWindowText(m_hwndNodokaPathName, m_data->m_name.c_str());
		SetWindowText(m_hwndNodokaPath, m_data->m_filename.c_str());
		SetWindowText(m_hwndSymbols, m_data->m_symbols.c_str());

		restrictSmallestSize();

		// set layout manager
		typedef LayoutManager LM;

		addItem(GetDlgItem(m_hwnd, IDC_STATIC_nodokaPathName));
		addItem(GetDlgItem(m_hwnd, IDC_EDIT_nodokaPathName),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_TOP_EDGE,
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_TOP_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_STATIC_nodokaPathNameComment),
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_TOP_EDGE,
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_TOP_EDGE);

		addItem(GetDlgItem(m_hwnd, IDC_STATIC_nodokaPath));
		addItem(GetDlgItem(m_hwnd, IDC_EDIT_nodokaPath),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_TOP_EDGE,
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_TOP_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_browse),
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_TOP_EDGE,
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_TOP_EDGE);

		addItem(GetDlgItem(m_hwnd, IDC_STATIC_symbols));
		addItem(GetDlgItem(m_hwnd, IDC_EDIT_symbols),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_TOP_EDGE,
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_TOP_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_STATIC_symbolsComment),
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_TOP_EDGE,
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_TOP_EDGE);

		addItem(GetDlgItem(m_hwnd, IDOK),
				LM::ORIGIN_CENTER, LM::ORIGIN_TOP_EDGE,
				LM::ORIGIN_CENTER, LM::ORIGIN_TOP_EDGE);
		addItem(GetDlgItem(m_hwnd, IDCANCEL),
				LM::ORIGIN_CENTER, LM::ORIGIN_TOP_EDGE,
				LM::ORIGIN_CENTER, LM::ORIGIN_TOP_EDGE);

		restrictSmallestSize(LM::RESTRICT_BOTH);
		restrictLargestSize(LM::RESTRICT_VERTICALLY);

		return TRUE;
	}

	/// WM_CLOSE
	BOOL wmClose()
	{
		CHECK_TRUE(EndDialog(m_hwnd, 0));
		return TRUE;
	}

	/// WM_COMMAND
	BOOL wmCommand(int /* i_notify_code */, int i_id, HWND /* i_hwnd_control */)
	{
		_TCHAR buf[GANA_MAX_PATH];
		switch (i_id)
		{
		case IDC_BUTTON_browse:
		{
			tstring title = loadString(IDS_openNodoka);
			tstring filter = loadString(IDS_openNodokaFilter);
			for (size_t i = 0; i < filter.size(); ++i)
				if (filter[i] == _T('|'))
					filter[i] = _T('\0');

			_tcscpy_s(buf, _countof(buf), _T(".nodoka"));
			OPENFILENAME of;
			memset(&of, 0, sizeof(of));
			of.lStructSize = sizeof(of);
			of.hwndOwner = m_hwnd;
			of.lpstrFilter = filter.c_str();
			of.nFilterIndex = 1;
			of.lpstrFile = buf;
			of.nMaxFile = NUMBER_OF(buf);
			of.lpstrTitle = title.c_str();
			of.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST |
					   OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
			if (GetOpenFileName(&of))
				SetWindowText(m_hwndNodokaPath, buf);
			return TRUE;
		}

		case IDOK:
		{
			GetWindowText(m_hwndNodokaPathName, buf, NUMBER_OF(buf));
			m_data->m_name = buf;
			GetWindowText(m_hwndNodokaPath, buf, NUMBER_OF(buf));
			m_data->m_filename = buf;
			GetWindowText(m_hwndSymbols, buf, NUMBER_OF(buf));
			m_data->m_symbols = buf;
			CHECK_TRUE(EndDialog(m_hwnd, 1));
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
INT_PTR CALLBACK dlgEditSetting_dlgProc(HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam)
{
	DlgEditSetting *wc;
	getUserData(i_hwnd, &wc);
	if (!wc)
		switch (i_message)
		{
		case WM_INITDIALOG:
			wc = setUserData(i_hwnd, new DlgEditSetting(i_hwnd));
			return wc->wmInitDialog(
				reinterpret_cast<HWND>(i_wParam), i_lParam);
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
		default:
			return wc->defaultWMHandler(i_message, i_wParam, i_lParam);
		}
	return FALSE;
}
