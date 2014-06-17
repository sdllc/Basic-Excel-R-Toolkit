
#include "stdafx.h"
#include "BERT.h"
#include "ExcelFunctions.h"
#include "ThreadLocalStorage.h"
#include "resource.h"
#include "Dialogs.h"
#include "RegistryUtils.h"

#include <RichEdit.h>

extern HWND hWndConsole;

void AppendLog(const char *buffer)
{
	HWND hWnd = ::GetDlgItem(hWndConsole, IDC_LOG_WINDOW);
	if (hWnd)
	{
		CHARRANGE cr;
		cr.cpMin = -1;
		cr.cpMax = -1;

		std::string fmt = buffer;
		fmt += "\r\n";

		::SendMessageA(hWnd, EM_EXSETSEL, 0, (LPARAM)&cr);
		::SendMessageA(hWnd, EM_REPLACESEL, 0, (LPARAM)fmt.c_str());
		::SendMessage(hWnd, WM_VSCROLL, SB_BOTTOM, (LPARAM)NULL);
	}
}

DIALOG_RESULT_TYPE CALLBACK OptionsDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	char buffer[MAX_PATH];
	HWND hWnd;

	switch (message)
	{
	case WM_INITDIALOG:
		// CenterWindow(hwndDlg, ::GetParent(hwndDlg));
		{
			hWnd = ::GetDlgItem(hwndDlg, IDC_RHOME);
			buffer[0] = 0;
			CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, buffer, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_HOME, false);
			SetWindowTextA(hWnd, buffer);

			hWnd = ::GetDlgItem(hwndDlg, IDC_RUSER);
			buffer[0] = 0;
			CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, buffer, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_USER, false);
			SetWindowTextA(hWnd, buffer);

			hWnd = ::GetDlgItem(hwndDlg, IDC_ENVIRONMENT);
			buffer[0] = 0;
			CRegistryUtils::GetRegString(HKEY_CURRENT_USER, buffer, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_ENVIRONMENT);
			SetWindowTextA(hWnd, buffer);

			hWnd = ::GetDlgItem(hwndDlg, IDC_STARTUPFILE);
			buffer[0] = 0;
			CRegistryUtils::GetRegString(HKEY_CURRENT_USER, buffer, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_STARTUP);
			SetWindowTextA(hWnd, buffer);

		}
		break;

	case WM_COMMAND:

		switch (LOWORD(wParam))
		{
		case IDOK:
		case IDCANCEL:
			EndDialog(hwndDlg, wParam);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

DIALOG_RESULT_TYPE CALLBACK ConsoleDlgProc( HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	HWND hWnd = 0;
	char *szBuffer = 0;

	static POINT offsetsRE;
	static POINT offsetsButton;
	static POINT offsetsCmd[2];

	switch (message)
	{
	case WM_INITDIALOG:
		// CenterWindow(hwndDlg, ::GetParent(hwndDlg));
		{
			RECT rectInner;
			RECT rectOuter;
			POINT p;

			hWnd = ::GetDlgItem(hwndDlg, IDC_LOG_WINDOW);
			::GetWindowRect(hwndDlg, &rectOuter);

			::GetWindowRect(hWnd, &rectInner);
			offsetsRE.x = (rectOuter.right - rectOuter.left) - (rectInner.right - rectInner.left);
			offsetsRE.y = (rectOuter.bottom - rectOuter.top) - (rectInner.bottom - rectInner.top);

			std::string logtext;
			getLogText(logtext);
			::SetWindowTextA(hWnd, logtext.c_str());
			::SendMessage(hWnd, WM_VSCROLL, SB_BOTTOM, (LPARAM)NULL);

			hWnd = ::GetDlgItem(hwndDlg, IDC_COMMAND);
			::SendMessage(hWnd, EM_SETEVENTMASK, 0, ENM_KEYEVENTS);
			::GetWindowRect(hWnd, &rectInner);

			offsetsCmd[0].x = offsetsCmd[1].x = rectInner.left;
			offsetsCmd[0].y = offsetsCmd[1].y = rectInner.top;
			::ScreenToClient(hwndDlg, &offsetsCmd[0]);
			::ScreenToClient(hwndDlg, &offsetsCmd[1]);

			offsetsCmd[0].y = rectInner.bottom - rectInner.top;

			hWnd = ::GetDlgItem(hwndDlg, IDOK);
			::GetWindowRect(hWnd, &rectInner);
					
			offsetsButton.x = rectInner.left;
			offsetsButton.y = rectInner.top;

			p.x = rectOuter.right;
			p.y = rectOuter.bottom;

			::ScreenToClient(hwndDlg, &p);

			::ScreenToClient(hwndDlg, &offsetsButton);
			offsetsButton.x -= p.x;
			offsetsButton.y -= p.y;

			offsetsCmd[1].x -= p.x;
			offsetsCmd[1].y -= p.y;

			hWndConsole = hwndDlg;
		}
		break;

	case WM_SIZING:
		{
			RECT rctl;
			RECT rectOuter;
			POINT p;
			RECT *prect = (RECT*)lParam;
			
			hWnd = ::GetDlgItem(hwndDlg, IDC_LOG_WINDOW);
			::GetWindowRect(hWnd, &rctl);
			::SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, (prect->right - prect->left) - offsetsRE.x, (prect->bottom - prect->top) - offsetsRE.y,
				SWP_NOMOVE | SWP_NOZORDER | SWP_DEFERERASE );
			
			hWnd = ::GetDlgItem(hwndDlg, IDOK);

			p.x = prect->right;
			p.y = prect->bottom;
			::ScreenToClient(hwndDlg, &p);

			::SetWindowPos(hWnd, HWND_NOTOPMOST, p.x + offsetsButton.x, p.y + offsetsButton.y, 0, 0,
				SWP_NOSIZE | SWP_NOZORDER | SWP_DEFERERASE);

			hWnd = ::GetDlgItem(hwndDlg, IDC_COMMAND);
			::SetWindowPos(hWnd, HWND_NOTOPMOST, offsetsCmd[0].x, p.y + offsetsCmd[1].y, (prect->right - prect->left) - offsetsRE.x, offsetsCmd[0].y,
				SWP_NOZORDER | SWP_DEFERERASE);

		}
		break;

	case WM_ERASEBKGND:
		// return TRUE;
		break;

	case WM_NOTIFY:
		if(((LPNMHDR)lParam)->idFrom == IDC_COMMAND )
		{ 
			if (((LPNMHDR)lParam)->code == EN_MSGFILTER)
			{
				MSGFILTER* filter = (MSGFILTER*)lParam;
				if ( filter->wParam == '\r' || filter->wParam == '\n' )
				{
					if (filter->msg == WM_KEYUP)
					{
						int len = ::GetWindowTextLengthA(filter->nmhdr.hwndFrom);
						char *text = new char[len + 1];
						::GetWindowTextA(filter->nmhdr.hwndFrom, text, len + 1);
						::SetWindowTextA(filter->nmhdr.hwndFrom, "");

						std::string tmp = "> ";
						tmp += text;
						AppendLog(tmp.c_str());

						tmp = text;
						tmp.erase(tmp.find_last_not_of(" \n\r\t") + 1);

						RExecStringBuffered(tmp.c_str());
						delete[] text;
					}
					return TRUE;
				}
			}
		}
		break;

	case WM_COMMAND:

		switch (LOWORD(wParam))
		{
			/*
		case IDC_COMMAND:
			if (HIWORD(wParam) == EN_CHANGE)
			{
				return FALSE;
			}
			if (HIWORD(wParam) == EN_UPDATE)
			{
				return FALSE;
			}
			break;
			*/
		case IDOK:
		case IDCANCEL:
			EndDialog(hwndDlg, wParam);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void ConsoleDlg(HINSTANCE hInst)
{
	XLOPER12 xWnd;
	Excel12(xlGetHwnd, &xWnd, 0);
	
	// InitRichEdit();

	::DialogBox(hInst,
		MAKEINTRESOURCE(IDD_DIALOG1),
		(HWND)xWnd.val.w,
		(DLGPROC)ConsoleDlgProc);

	Excel12(xlFree, 0, 1, (LPXLOPER12)&xWnd);

}
