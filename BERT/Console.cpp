/*
* Basic Excel R Toolkit (BERT)
* Copyright (C) 2014-2015 Structured Data, LLC
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
*/

#include "stdafx.h"
#include "BERT.h"

#include "ThreadLocalStorage.h"
#include "resource.h"
#include "Console.h"
#include "Dialogs.h"
#include "RegistryUtils.h"
#include "StringConstants.h"

#include "Scintilla.h"
#include "Objbase.h"

typedef std::vector< std::string> SVECTOR;
typedef SVECTOR::iterator SITER;

SVECTOR cmdVector;
SVECTOR historyVector;

SVECTOR wordList;
SVECTOR moneyList;
std::string moneyToken;

SVECTOR baseWordList;
bool wlInit = false;

bool reopenWindow = false;

int historyPointer = 0;
std::string historyCurrentLine;

extern HWND hWndConsole;
extern HMODULE ghModule;

int minCaret = 0;
WNDPROC lpfnEditWndProc = 0;
sptr_t(*fn)(sptr_t*, int, uptr_t, sptr_t);
sptr_t* ptr;

extern void flush_log();

bool inputlock = false;

RECT rectConsole = { 0, 0, 0, 0 };

extern HRESULT SafeCall(SAFECALL_CMD cmd, std::vector< std::string > *vec, int *presult);
extern HRESULT Marshal();

std::vector< std::string > fontlist;

int promptwidth = 0;
bool autowidth = true;

HWND hWndExcel = 0;

int CALLBACK EnumFontFamExProc(const LOGFONTA *lpelfe, const TEXTMETRICA *lpntme, DWORD FontType, LPARAM lParam)
{
	if (lpelfe->lfFaceName[0] != '@')
		fontlist.push_back(std::string(lpelfe->lfFaceName));
	return -1;
}

bool ColorDlg(HWND hwnd, DWORD &dwColor )
{
	CHOOSECOLOR cc;                 // common dialog box structure 
	static COLORREF acrCustClr[16]; // array of custom colors 

	HBRUSH hbrush;                  // brush handle
	//static DWORD rgbCurrent;        // initial color selection

	bool rslt = false;

	// Initialize CHOOSECOLOR 
	ZeroMemory(&cc, sizeof(cc));
	cc.lStructSize = sizeof(cc);
	cc.hwndOwner = hwnd;
	cc.lpCustColors = (LPDWORD)acrCustClr;
	cc.rgbResult = dwColor;

	cc.Flags = CC_FULLOPEN | CC_RGBINIT ;

	if (ChooseColor(&cc) == TRUE)
	{
		dwColor = cc.rgbResult;
		rslt = true;
	}

	return rslt;
}

DIALOG_RESULT_TYPE CALLBACK ConsoleOptionsDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND hwnd;
	DWORD dw;
	char buffer[64];
	LONG_PTR lp;

	static DWORD dwBack, dwUser, dwMessage;

	switch (message)
	{
	case WM_INITDIALOG:

		if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, buffer, 63, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_FONT)
			|| !strlen(buffer)) strcpy_s(buffer, 64, SCINTILLA_FONT_NAME);

		if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_SIZE)
			|| dw <= 0) dw = SCINTILLA_FONT_SIZE;

		if (fontlist.size() <= 0)
		{
			LOGFONTA lf;
			memset(&lf, 0, sizeof(LOGFONTA));
			lf.lfCharSet = DEFAULT_CHARSET;
			EnumFontFamiliesExA(::GetWindowDC(hwndDlg), &lf, EnumFontFamExProc, 0, 0);

			std::sort(fontlist.begin(), fontlist.end());
			fontlist.erase(std::unique(fontlist.begin(), fontlist.end()), fontlist.end());

		}

		hwnd = ::GetDlgItem(hwndDlg, IDC_COMBO_FONTS);
		if (hwnd)
		{
			RECT rect;
			::GetWindowRect(hwnd, &rect);
			int len = fontlist.size();
			int sel = 0;
			for (int idx = 0; idx < len; idx++)
			{
				if (!fontlist[idx].compare(buffer)) sel = idx;
				::SendMessageA(hwnd, CB_ADDSTRING, 0, (LPARAM)(fontlist[idx].c_str()));
			}
			::SendMessageA(hwnd, CB_SETCURSEL, sel, 0);
			::SetWindowPos(hwnd, 0, 0, 0, rect.right - rect.left, 300, SWP_NOZORDER | SWP_NOMOVE);
		}

		hwnd = ::GetDlgItem(hwndDlg, IDC_COMBO_FONTSIZE);
		if (hwnd)
		{
			RECT rect;
			::GetWindowRect(hwnd, &rect);
			for (int idx = 4; idx <= 28; idx++)
			{
				sprintf_s(buffer, 32, "%d", idx);
				::SendMessageA(hwnd, CB_ADDSTRING, 0, (LPARAM)(buffer));
			}
			::SendMessageA(hwnd, CB_SETCURSEL, dw-4, 0);
			::SetWindowPos(hwnd, 0, 0, 0, rect.right - rect.left, 200, SWP_NOZORDER | SWP_NOMOVE);
		}

		if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dwBack, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_BACK)
			|| dwBack < 0) dwBack = SCINTILLA_BACK_COLOR;
		if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dwUser, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_USER)
			|| dwUser < 0) dwUser = SCINTILLA_USER_TEXT_COLOR;
		if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dwMessage, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_MESSAGE)
			|| dwMessage < 0) dwMessage = SCINTILLA_R_TEXT_COLOR;

		lp = ::GetWindowLongPtr(::GetDlgItem(hwndDlg, IDC_BBACK), GWL_STYLE);
		::SetWindowLongPtr(::GetDlgItem(hwndDlg, IDC_BBACK), GWL_STYLE, lp | BS_OWNERDRAW);

		lp = ::GetWindowLongPtr(::GetDlgItem(hwndDlg, IDC_BUSER), GWL_STYLE);
		::SetWindowLongPtr(::GetDlgItem(hwndDlg, IDC_BUSER), GWL_STYLE, lp | BS_OWNERDRAW);

		lp = ::GetWindowLongPtr(::GetDlgItem(hwndDlg, IDC_BMESSAGE), GWL_STYLE);
		::SetWindowLongPtr(::GetDlgItem(hwndDlg, IDC_BMESSAGE), GWL_STYLE, lp | BS_OWNERDRAW);

		lp = ::GetWindowLongPtr(::GetDlgItem(hwndDlg, IDC_PREVIEW), GWL_STYLE);
		::SetWindowLongPtr(::GetDlgItem(hwndDlg, IDC_PREVIEW), GWL_STYLE, lp | SS_OWNERDRAW);

		if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_WIDTH)
			|| dw < 0) dw = DEFAULT_CONSOLE_WIDTH;
		sprintf_s(buffer, 64, "%d", dw);
		::SetWindowTextA(::GetDlgItem(hwndDlg, IDC_CONSOLE_WIDTH), buffer);

		if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_AUTO_WIDTH)
			|| dw < 0) dw = DEFAULT_CONSOLE_AUTO_WIDTH;

		::SendMessage(::GetDlgItem(hwndDlg, IDC_CB_AUTO), BM_SETCHECK, dw ? BST_CHECKED : BST_UNCHECKED, 0);
		::EnableWindow(::GetDlgItem(hwndDlg, IDC_CONSOLE_WIDTH), !dw);


		::EnableWindow(::GetDlgItem(hwndDlg, IDAPPLY), 0);

		CenterWindow(hwndDlg, ::GetParent(hwndDlg));
		break;

	case WM_DRAWITEM:
	{
		HBRUSH brush = 0;
		HFONT font = 0;
		LOGFONT lf;
		HPEN pen;
		HDC &dc = ((DRAWITEMSTRUCT*)lParam)->hDC;
		RECT &rect = (((DRAWITEMSTRUCT*)lParam)->rcItem);
		RECT rfill;

		switch (wParam)
		{
		case IDC_PREVIEW:
		{
			brush = ::CreateSolidBrush(dwBack);
			::FillRect(dc, &rect, brush);
			memset(&lf, 0, sizeof(LOGFONT));
			::GetWindowText(::GetDlgItem(hwndDlg, IDC_COMBO_FONTS), lf.lfFaceName, 32);
			::GetWindowTextA(::GetDlgItem(hwndDlg, IDC_COMBO_FONTSIZE), buffer, 32);
			dw = atoi(buffer);
			if (dw < 1) dw = 1;
			lf.lfHeight = -MulDiv(dw, GetDeviceCaps(GetWindowDC(hwndDlg), LOGPIXELSY), 72);
			font = ::CreateFontIndirect(&lf);
			HGDIOBJ oldFont = ::SelectObject(dc, font);
			SetTextColor(dc, dwUser);
			SetBkMode(dc, TRANSPARENT);
			RECT r2;
			::CopyRect(&r2, &rect);
			::DrawTextA(dc, "User text; ", -1, &r2, DT_TOP | DT_WORD_ELLIPSIS | DT_SINGLELINE | DT_CALCRECT);
			::DrawTextA(dc, "User text; ", -1, &r2, DT_TOP | DT_WORD_ELLIPSIS | DT_SINGLELINE);
			int left = r2.right;
			::CopyRect(&r2, &rect);
			r2.left = left;
			SetTextColor(dc, dwMessage);
			::DrawTextA(dc, "message text", -1, &r2, DT_TOP | DT_WORD_ELLIPSIS | DT_SINGLELINE);
			::SelectObject(dc, oldFont);
			::DeleteObject(font);
			::DeleteObject(brush);
			return FALSE;
		}
		case IDC_BBACK:
			brush = ::CreateSolidBrush(dwBack);
			break;
		case IDC_BUSER:
			brush = ::CreateSolidBrush(dwUser);
			break;
		case IDC_BMESSAGE:
			brush = ::CreateSolidBrush(dwMessage);
			break;
		}
		if (brush)
		{
			rfill.left = rect.left + 2;
			rfill.top = rect.top + 2;
			rfill.right = rect.right - 2;
			rfill.bottom = rect.bottom - 2;

			pen = ::CreatePen(PS_SOLID, 1, 0);
			MoveToEx(dc, rect.left, rect.top, 0);
			LineTo(dc, rect.right-1, rect.top);
			LineTo(dc, rect.right - 1, rect.bottom - 1);
			LineTo(dc, rect.left, rect.bottom - 1);
			LineTo(dc, rect.left, rect.top);
			::FillRect(dc, &rfill, brush);
			::DeleteObject(brush);
			::DeleteObject(pen);
		}
	}
		break;

	case WM_COMMAND:

		switch (LOWORD(wParam))
		{
		case IDC_COMBO_FONTS:
		case IDC_COMBO_FONTSIZE:
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				::RedrawWindow(::GetDlgItem(hwndDlg, IDC_PREVIEW), 0, 0, RDW_INVALIDATE);
				::EnableWindow(::GetDlgItem(hwndDlg, IDAPPLY), 1);
			}
			break;

		case IDC_BBACK:
			if (ColorDlg(hwndDlg, dwBack))
			{
				::RedrawWindow(::GetDlgItem(hwndDlg, IDC_BBACK), 0, 0, RDW_INVALIDATE);
				::RedrawWindow(::GetDlgItem(hwndDlg, IDC_PREVIEW), 0, 0, RDW_INVALIDATE);
				::EnableWindow(::GetDlgItem(hwndDlg, IDAPPLY), 1);
			}
			break;
		case IDC_BUSER:
			if (ColorDlg(hwndDlg, dwUser))
			{
				::RedrawWindow(::GetDlgItem(hwndDlg, IDC_BUSER), 0, 0, RDW_INVALIDATE);
				::RedrawWindow(::GetDlgItem(hwndDlg, IDC_PREVIEW), 0, 0, RDW_INVALIDATE);
				::EnableWindow(::GetDlgItem(hwndDlg, IDAPPLY), 1);
			}
			break;
		case IDC_BMESSAGE:
			if (ColorDlg(hwndDlg, dwMessage))
			{
				::RedrawWindow(::GetDlgItem(hwndDlg, IDC_BMESSAGE), 0, 0, RDW_INVALIDATE);
				::RedrawWindow(::GetDlgItem(hwndDlg, IDC_PREVIEW), 0, 0, RDW_INVALIDATE);
				::EnableWindow(::GetDlgItem(hwndDlg, IDAPPLY), 1);
			}
			break;

		case IDC_CONSOLE_WIDTH:
			if (HIWORD( wParam) == EN_CHANGE )
				::EnableWindow(::GetDlgItem(hwndDlg, IDAPPLY), 1);
			break;

		case IDC_CB_AUTO:
			::EnableWindow(::GetDlgItem(hwndDlg, IDC_CONSOLE_WIDTH), ::SendMessage(::GetDlgItem(hwndDlg, IDC_CB_AUTO), BM_GETCHECK, 0, 0) != BST_CHECKED);
			::EnableWindow(::GetDlgItem(hwndDlg, IDAPPLY), 1);
			break;

		case IDAPPLY:
		case IDOK:

			::GetWindowTextA(::GetDlgItem(hwndDlg, IDC_COMBO_FONTSIZE), buffer, 64);
			dw = atoi(buffer);
			if (dw < 1) dw = 1;
			fn(ptr, SCI_STYLESETSIZE, STYLE_DEFAULT, dw);

			::GetWindowTextA(::GetDlgItem(hwndDlg, IDC_COMBO_FONTS), buffer, 64);
			fn(ptr, SCI_STYLESETFONT, STYLE_DEFAULT, (sptr_t)buffer);

			fn(ptr, SCI_STYLESETFORE, 1, dwMessage);
			fn(ptr, SCI_STYLESETFORE, 0, dwUser);
			fn(ptr, SCI_STYLESETBACK, 0, dwBack);
			fn(ptr, SCI_STYLESETBACK, 1, dwBack);
			fn(ptr, SCI_STYLESETBACK, 32, dwBack);
			::EnableWindow(::GetDlgItem(hwndDlg, IDAPPLY), 0);

			if (LOWORD(wParam) == IDAPPLY) break;

			// set registry
			CRegistryUtils::SetRegString(HKEY_CURRENT_USER, buffer, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_FONT);
			CRegistryUtils::SetRegDWORD(HKEY_CURRENT_USER, dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_SIZE);
			CRegistryUtils::SetRegDWORD(HKEY_CURRENT_USER, dwBack, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_BACK);
			CRegistryUtils::SetRegDWORD(HKEY_CURRENT_USER, dwMessage, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_MESSAGE);
			CRegistryUtils::SetRegDWORD(HKEY_CURRENT_USER, dwUser, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_USER);

			::GetWindowTextA(::GetDlgItem(hwndDlg, IDC_CONSOLE_WIDTH), buffer, 64);
			dw = atoi(buffer);
			CRegistryUtils::SetRegDWORD(HKEY_CURRENT_USER, dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_WIDTH);
			autowidth = ::SendMessage(::GetDlgItem(hwndDlg, IDC_CB_AUTO), BM_GETCHECK, 0, 0) == BST_CHECKED;
			dw = autowidth ? 1 : 0;
			CRegistryUtils::SetRegDWORD(HKEY_CURRENT_USER, dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_AUTO_WIDTH);
			UpdateConsoleWidth(true);

			EndDialog(hwndDlg, wParam);
			return TRUE;

		case IDCANCEL:
			SetConsoleDefaults();
			EndDialog(hwndDlg, wParam);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void ToggleOnTop(HWND hwnd){

	// we need to close and re-open the console, so we don't
	// do any UI here - just set the registry value

	DWORD dw;
	if (CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_ON_TOP) && dw) dw = 0;
	else dw = 1;
	CRegistryUtils::SetRegDWORD(HKEY_CURRENT_USER, dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_ON_TOP);


}

void ConsoleOptions( HWND hwnd )
{
	::DialogBox(ghModule,
		MAKEINTRESOURCE(IDD_CONSOLE_OPTIONS),
		hwnd, (DLGPROC)ConsoleOptionsDlgProc);
	UpdateConsoleWidth(true);
}

void initWordList()
{
	static int lastWLLen = 2500;

	// fuck it we'll do it live

	wordList.clear();
	wordList.reserve(lastWLLen + 100);

	if (!wlInit)
	{
		HRSRC handle = ::FindResource(ghModule, MAKEINTRESOURCE(IDR_RCDATA2), RT_RCDATA);
		if (handle != 0)
		{
			DWORD len = ::SizeofResource(ghModule, handle);
			HGLOBAL global = ::LoadResource(ghModule, handle);
			if (global != 0 && len > 0)
			{
				char *str = (char*)::LockResource(global);
				std::stringstream ss(str);
				std::string line;

				while (std::getline(ss, line))
				{
					line = Util::trim(line);
					if (line.length() > 0 && line.c_str()[0] != '#')
					{
						Util::split(line, ' ', MIN_WORD_LENGTH, baseWordList);
					}
				}
			}
			wlInit = true;
		}
	}

	//SVECTOR tmp;
	//tmp.reserve(lastWLLen + 100);

	DWORD stat = WaitForSingleObject(muxWordlist, INFINITE);
	if (stat == WAIT_OBJECT_0)
	{
		DebugOut("Length of wl: %d\n", wlist.size());
		lastWLLen = wlist.size();

		for (SVECTOR::iterator iter = wlist.begin(); iter != wlist.end(); iter++)
		{
			std::string str(*iter);
			wordList.push_back(str);
		}
		ReleaseMutex(muxWordlist);
	}
	wordList.insert(wordList.end(), baseWordList.begin(), baseWordList.end());

	std::sort(wordList.begin(), wordList.end());
	wordList.erase(std::unique(wordList.begin(), wordList.end()), wordList.end());
	moneyToken = "";

}

void AppendLog(const char *buffer, int style, int checkoverlap)
{
	// cases: checkoverlap is set UNLESS it's preloading
	// text on console startup (and there is no prompt).
	// if checkoverlap is set, just append and style.

	// the global inputlock is set when a command is running
	// and again, there's no prompt.  (FIXME: consolidate these?)

	// in the other case, there is a prompt and potentially
	// user-entered text that has to be pushed aside.  

	int len = strlen(buffer);

	if (!checkoverlap || inputlock)
	{
		int start = fn(ptr, SCI_GETLENGTH, 0, 0);

		fn(ptr, SCI_SETSEL, -1, -1);
		fn(ptr, SCI_APPENDTEXT, len, (sptr_t)buffer);

		if (style != 0)
		{
			fn(ptr, SCI_STARTSTYLING, start, 0x31);
			fn(ptr, SCI_SETSTYLING, len, style);
		}

	}
	else
	{
		// insert
		int np = minCaret - promptwidth;

		// insert
		fn(ptr, SCI_INSERTTEXT, np, (sptr_t)buffer);

		// update our marker, pos should move
		minCaret += len;

		if (style != 0)
		{
			fn(ptr, SCI_STARTSTYLING, np, 0x31);
			fn(ptr, SCI_SETSTYLING, len, style);
		}

	}

	fn(ptr, SCI_SCROLLCARET, 0, 0);

}

void Prompt(const char *prompt = DEFAULT_PROMPT)
{
	promptwidth = strlen(prompt);
	fn(ptr, SCI_APPENDTEXT, promptwidth, (sptr_t)prompt);
	fn(ptr, SCI_SETXOFFSET, 0, 0);
	fn(ptr, SCI_SETSEL, -1, -1);
	minCaret = fn(ptr, SCI_GETCURRENTPOS, 0, 0);
	historyPointer = 0;
}

void CallComplete(PARSE_STATUS_2 ps, LPARAM lParam)
{
	inputlock = false;
	DebugOut("Complete:\t%d\n", GetTickCount());

	switch (ps)
	{
	case PARSE2_ERROR:
		inputlock = true;
		logMessage(PARSE_ERROR_MESSAGE, strlen(PARSE_ERROR_MESSAGE), true);
		inputlock = false;
		break;

	case PARSE2_EXTERNAL_ERROR:
		inputlock = true;
		logMessage(EXTERNAL_ERROR_MESSAGE, strlen(EXTERNAL_ERROR_MESSAGE), true);
		inputlock = false;
		break;

	case PARSE2_INCOMPLETE:
		Prompt(CONTINUATION_PROMPT);
		return;

	default:
		initWordList();
		break;
	}

	if (!lParam)
	{
		cmdVector.clear();
		Prompt();
	}

	DebugOut("Done:\t%d\n", GetTickCount()); 

}

DWORD WINAPI CallThreadProc(LPVOID lpParameter)
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	HWND hwnd = (HWND)lpParameter;
	int ips = 0;
	SafeCall(SCC_EXEC, &cmdVector, &ips);
	::PostMessage(hwnd, WM_CALL_COMPLETE, ips, 0);
	::CoUninitialize();
	return 0;
}

/*
DWORD WINAPI UtilityThreadProc(LPVOID lpParameter)
{
	HWND hwnd = (HWND)lpParameter;
	int cmd = (int)lpParameter;
	switch (cmd)
	{
	case ID_CONSOLE_RELOADSTARTUPFILE:
		SafeCall(SCC_RELOAD_STARTUP, 0, 0);
		break;
	}
	::PostMessage(hWndConsole, WM_CALL_COMPLETE, PARSE2_OK, 1);
	return 0;
}

void UtilityCall( int cmdid )
{
	DWORD dwThread;
	::CreateThread(0, 0, UtilityThreadProc, (void*)cmdid, 0, &dwThread);
}
*/

void CancelCommand(){

	cmdVector.clear();

	fn(ptr, SCI_APPENDTEXT, 1, (sptr_t)"\n");
	fn(ptr, SCI_AUTOCCANCEL, 0, 0);
	fn(ptr, SCI_CALLTIPCANCEL, 0, 0);

	Prompt();
}

void ProcessCommand()
{
	DebugOut("ProcessCommand:\t%d\n", GetTickCount());

	int len = fn(ptr, SCI_GETLENGTH, 0, 0);

	// close tip, autoc

	fn(ptr, SCI_AUTOCCANCEL, 0, 0);
	fn(ptr, SCI_CALLTIPCANCEL, 0, 0);
	
	int linelen = len - minCaret;
	std::string cmd;

	if (linelen == 0)
	{
		// do nothing
	}
	else
	{
		Sci_TextRange str;
		str.chrg.cpMin = minCaret;
		str.chrg.cpMax = len;
		str.lpstrText = new char[linelen + 1];
		fn(ptr, SCI_GETTEXTRANGE, 0, (sptr_t)(&str));

		cmd = str.lpstrText;
		cmd = Util::trim(cmd);
		if (cmd.length() > 0) historyVector.push_back(cmd);

		delete[] str.lpstrText;
	}

	{
		// why do this twice?
		Sci_TextRange str;
		str.chrg.cpMin = minCaret - 2;
		str.chrg.cpMax = len;
		str.lpstrText = new char[str.chrg.cpMax - str.chrg.cpMin + 2];
		fn(ptr, SCI_GETTEXTRANGE, 0, (sptr_t)(&str));
		int len = strlen(str.lpstrText);
		str.lpstrText[len] = '\n';
		str.lpstrText[len + 1] = 0;
		logMessage(str.lpstrText, len + 1, false);
		delete[] str.lpstrText;
	}

	fn(ptr, SCI_APPENDTEXT, 1, (sptr_t)"\n");
	bool wl = false;

	if (cmd.length() > 0)
	{
		// there is a case where you paste multiple lines; we need that
		// to be handled as multiple lines.

		if (cmd.find("\n") == std::string::npos) cmdVector.push_back(cmd);
		else
		{
			std::istringstream stream(cmd);
			std::string line;
			while (std::getline(stream, line)) {
				line.erase(line.find_last_not_of(" \n\r\t") + 1);
				if (line.length() > 0)
					cmdVector.push_back(line);
			}
		}

		DebugOut("Exec:\t%d\n", GetTickCount());
		inputlock = true;

		DWORD dwThread;
		::CreateThread(0, 0, CallThreadProc, hWndConsole, 0, &dwThread);

	}
	else if (cmdVector.size() > 0)
	{
		Prompt(CONTINUATION_PROMPT);
	}
	else
	{
		Prompt();
	}


	/*
	cmdVector.clear();

	Prompt();

	if (wl) initWordList();

	DebugOut("Done:\t%d\n", GetTickCount());
	*/

}

/**
* clears the buffer history, not the command history
*/
void ClearHistory()
{
	fn(ptr, SCI_SETTEXT, 0, (sptr_t)"");
	clearLogText();
	cmdVector.clear();
	Prompt();
}

void CmdHistory(int scrollBy)
{
	int to = historyPointer + scrollBy;
	if (to > 0) return; // can't go to future
	if (-to > historyVector.size()) return; // too far back
	int end = fn(ptr, SCI_GETLENGTH, 0, 0);
	std::string repl;

	// ok, it's valid.  one thing to check: if on line 0, store it

	if (historyPointer == 0)
	{
		int linelen = end - minCaret;

		Sci_TextRange str;
		str.chrg.cpMin = minCaret;
		str.chrg.cpMax = end;
		str.lpstrText = new char[linelen + 1];
		fn(ptr, SCI_GETTEXTRANGE, 0, (sptr_t)(&str));
		historyCurrentLine = str.lpstrText;
		delete[] str.lpstrText;
	}

	if (to == 0) repl = historyCurrentLine.c_str();
	else
	{
		int check = historyVector.size() + to;
		repl = historyVector[check];
	}

	fn(ptr, SCI_SETSEL, minCaret, end);
	fn(ptr, SCI_REPLACESEL, 0, (sptr_t)(repl.c_str()));
	fn(ptr, SCI_SETSEL, -1, -1);

	historyPointer = to;

}


LRESULT CALLBACK SubClassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static int p;

	switch (msg)
	{

	case WM_CHAR:

		if (inputlock) return 0;

		switch (wParam)
		{
		case 13:
			ProcessCommand();
			return 0;

		default:
			p = fn(ptr, SCI_GETCURRENTPOS, 0, 0);
			if (p < minCaret) fn(ptr, SCI_SETSEL, -1, -1);

		}
		break;

	case WM_KEYUP:

		switch (wParam)
		{
		case VK_UP:
		case VK_DOWN:
		case VK_RETURN:
			return 0;
		}
		break;

	case WM_KEYDOWN:

		if (inputlock) return 0;

		switch (wParam)
		{
		case 'C':
			if (GetKeyState(VK_CONTROL) < 0){
				DebugOut("Control-c\n");
				CancelCommand();
			}
			break;

		case VK_ESCAPE:
			if (!fn(ptr, SCI_AUTOCACTIVE, 0, 0)
				&& !fn(ptr, SCI_CALLTIPACTIVE, 0, 0))
			{
				::PostMessage(::GetParent(hwnd), WM_COMMAND, WM_CLOSE_CONSOLE, 0);
				return 0;
			}
			break;

		case VK_HOME:
			fn(ptr, SCI_SETSEL, minCaret, minCaret);
			return 0;
			break;

		case VK_END:
			fn(ptr, SCI_SETSEL, -1, -1);
			break;

		case VK_LEFT:
		case VK_BACK:
			p = fn(ptr, SCI_GETCURRENTPOS, 0, 0);
			if (p <= minCaret) return 0;
			break;

		case VK_UP:
			if (!fn(ptr, SCI_AUTOCACTIVE, 0, 0))
			{
				CmdHistory(-1);
				return 0;
			}
			break;

		case VK_DOWN:
			if (!fn(ptr, SCI_AUTOCACTIVE, 0, 0))
			{
				CmdHistory(1);
				return 0;
			}
			break;

		case VK_RETURN:
			fn(ptr, SCI_AUTOCCANCEL, 0, 0);
			fn(ptr, SCI_CALLTIPCANCEL, 0, 0);
			return 0;
		}

		break;

	}

	return CallWindowProc(lpfnEditWndProc, hwnd, msg, wParam, lParam);
}

bool isWordChar(char c)
{
	return (((c >= 'a') && (c <= 'z'))
		|| ((c >= 'A') && (c <= 'Z'))
		|| (c == '_')
		|| (c == '$')
		|| (c == '@')
		|| (c == '.')
		);
}

/**
*/
void testAutocomplete()
{
	int len = fn(ptr, SCI_GETCURLINE, 0, 0);
	if (len <= 2) return;
	char *c = new char[len + 1];
	int caret = fn(ptr, SCI_GETCURLINE, len + 1, (sptr_t)c);
	std::string part;

	static std::string lastList;

	// FOR NOW, require that caret is at eol

	if (caret == len - 1)
	{
		bool money = false;
		for (--caret; caret >= 2 && isWordChar(c[caret]); caret--){ if (c[caret] == '$' || c[caret] == '@') money = true; }
		caret++;
		int slen = strlen(&(c[caret]));
		if (slen > 1)
		{
			std::string str = &(c[caret]);

			if (money)
			{
				std::string token = "";
				int midx = 0;
				for (int i = 1; i < slen; i++) if (str[i] == '$' || str[i] == '@') midx = i ;
				if (midx > 0) token = std::string(str.begin(), str.begin() + midx);
				if (token.compare(moneyToken))
				{
					int sc;
					moneyList.clear();
					moneyList.reserve(100);
					std::vector< std::string > sv;
					sv.push_back(token);
					SafeCall(SCC_NAMES, &sv, &sc);
					moneyToken = token;
				}
			}

			SVECTOR::iterator iter = std::lower_bound(money ? moneyList.begin() : wordList.begin(), money ? moneyList.end() : wordList.end(), str);
			c[len - 2]++;
			str = &(c[caret]);
			SVECTOR::iterator iter2 = std::lower_bound(money ? moneyList.begin() : wordList.begin(), money ? moneyList.end() : wordList.end(), str);
			int count = iter2 - iter;

			str = "";
			if (count > 0 && count <= MAX_AUTOCOMPLETE_LIST_LEN)
			{
				DebugOut("count %d\n", count);

				for (count = 0; iter < iter2; iter++, count++)
				{
					if (count) str += " ";
					str += iter->c_str();
				}

				if ( !fn(ptr, SCI_AUTOCACTIVE, 0, 0 ) || lastList.compare(str))
					fn(ptr, SCI_AUTOCSHOW, slen, (sptr_t)(str.c_str()));
				lastList = str;
			}
		}
	}

	// TODO: optimize this over a single bit of typing...

	caret = fn(ptr, SCI_GETCURLINE, len + 1, (sptr_t)c);
	std::string sc(c, c + caret);

	static std::string empty = "";
	static std::regex bparen("\\([^\\(]*?\\)");
	static std::regex rex("[^\\w\\._\\$]([\\w\\._\\$]+)\\s*\\([^\\)\\(]*$");
	static std::string lasttip;

	while (std::regex_search(sc, bparen))
		sc = regex_replace(sc, bparen, empty);

	std::smatch mat;

	bool ctvisible = fn(ptr, SCI_CALLTIPACTIVE, 0, 0);

	// so there is the possibility that we have a tip because we're in one
	// function, but then we close the tip and fall into an enclosing 
	// function... actually my regex doesn't work, then.

	if (std::regex_search(sc, mat, rex) /* && !ctvisible */)
	{
		std::string tip;
		std::string sym(mat[1].first, mat[1].second);

		if (sym.compare(lasttip))
		{
			int sc = 0;
			std::vector< std::string > sv;
			sv.push_back(sym);
			SafeCall(SCC_CALLTIP, &sv, &sc);
			tip = calltip;

			// if(0)// (getCallTip(tip, sym))
			if (sc)
			{
				DebugOut("%s: %s\n", sym.c_str(), tip.c_str());

				int pos = fn(ptr, SCI_GETCURRENTPOS, 0, 0);
				// pos -= caret; // start of line
				pos -= (mat[0].second - mat[0].first - 1); // this is not correct b/c we are munging the string
				fn(ptr, SCI_CALLTIPSHOW, pos, (sptr_t)tip.c_str());
			}
			else if (ctvisible) fn(ptr, SCI_CALLTIPCANCEL, 0, 0);
			lasttip = sym;
		}
	}
	else
	{
		if (ctvisible) fn(ptr, SCI_CALLTIPCANCEL, 0, 0);
		lasttip = "";
	}

	delete[] c;

}

VOID CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	flush_log();
}

void SetConsoleDefaults()
{
	char buffer[64];
	DWORD dw;

	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, buffer, 63, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_FONT)
		|| !strlen(buffer)) strcpy_s(buffer, 64, SCINTILLA_FONT_NAME);
	fn(ptr, SCI_STYLESETFONT, STYLE_DEFAULT, (sptr_t)buffer);

	if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_SIZE)
		|| dw <= 0) dw = SCINTILLA_FONT_SIZE;
	fn(ptr, SCI_STYLESETSIZE, STYLE_DEFAULT, dw);

	if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_MESSAGE)
		|| dw < 0) dw = SCINTILLA_R_TEXT_COLOR;
	fn(ptr, SCI_STYLESETFORE, 1, dw);

	if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_USER)
		|| dw < 0) dw = SCINTILLA_USER_TEXT_COLOR;
	fn(ptr, SCI_STYLESETFORE, 0, dw);

	if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_USER)
		|| dw < 0) dw = SCINTILLA_USER_TEXT_COLOR;
	fn(ptr, SCI_STYLESETFORE, 0, dw);

	if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_BACK)
		|| dw < 0) dw = SCINTILLA_BACK_COLOR;
	fn(ptr, SCI_STYLESETBACK, 0, dw);
	fn(ptr, SCI_STYLESETBACK, 1, dw);
	fn(ptr, SCI_STYLESETBACK, 32, dw);

	if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_AUTO_WIDTH)
		|| dw < 0) dw = DEFAULT_CONSOLE_AUTO_WIDTH;

	autowidth = (dw != 0);

}

void UpdateConsoleWidth( bool force )
{
	char buffer[128];
	DWORD dw;
	LOGFONTA lf;
	TEXTMETRIC tm;
	RECT rc;
	int chars = 0;

	static int lastWidth = 0;
	static int lastChars = 0;

	if (!fn) return;
	if (!force && !autowidth) return;

	::GetClientRect(hWndConsole, &rc);
	int width = rc.right - rc.left;
	if (width <= 0) return;

	if (!force && (lastWidth == width)) return;
	lastWidth = width;

	if (autowidth)
	{
		memset(&lf, 0, sizeof(LOGFONTA));

		if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, lf.lfFaceName, 32, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_FONT)
			|| !strlen(buffer)) strcpy_s(lf.lfFaceName, 32, SCINTILLA_FONT_NAME);

		if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_SIZE)
			|| dw <= 0) dw = SCINTILLA_FONT_SIZE;

		HDC dc = ::GetDC(0);
		lf.lfHeight = -MulDiv(dw, GetDeviceCaps(dc, LOGPIXELSY), 72);

		HFONT font = ::CreateFontIndirectA(&lf);
		HGDIOBJ oldFont = ::SelectObject(dc, font);

		::GetTextMetrics(dc, &tm);

		DWORD pixels = MulDiv(tm.tmAveCharWidth, 72, GetDeviceCaps(dc, LOGPIXELSX));

		::SelectObject(dc, oldFont);
		::DeleteObject(font);
		::ReleaseDC(0, dc);

		// this should not work, as char width is in logical units.
		// 16 represents the scrollbar - too small? check

		chars = (rc.right - rc.left - 16) / (tm.tmAveCharWidth) - 1;
	}
	else
	{
		if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_WIDTH)
			|| dw <= 0) dw = DEFAULT_CONSOLE_WIDTH;
		chars = dw;
	}

	if (chars < 10) chars = 10;
	if (chars > 1000) chars = 1000;

	if (chars == lastChars) return;
	lastChars = chars;

	SVECTOR sv;
	int r;

	sprintf_s(buffer, 128, "options(\"width\"=%d)", chars);
	sv.push_back(std::string(buffer));
	SafeCall(SCC_EXEC, &sv, &r);

	DebugOut("%s\n", buffer);

}

LRESULT CALLBACK WindowProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	const int borderedge = 0; //  8;
	const int topspace = 0;

	static HWND hwndScintilla = 0;
	HWND hWnd = 0;
	char *szBuffer = 0;
	RECT rect;

	static UINT_PTR tid = 0;

	switch (message)
	{
	case WM_INITDIALOG:
	case WM_CREATE:
		::GetClientRect(hwndDlg, &rect);

		{
			HMENU hMenu = (HMENU)::LoadMenu(ghModule, MAKEINTRESOURCE(IDR_CONSOLEMENU));
			if (hMenu != INVALID_HANDLE_VALUE){
				::SetMenu(hwndDlg, hMenu);

				DWORD dw;
				if (CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_ON_TOP) && dw)
				{
					CheckMenuItem(
						hMenu,
						ID_CONSOLE_ALWAYSONTOP ,
						MF_CHECKED
						);
				}
			}
		}


		hwndScintilla = CreateWindowExA(0,
			"Scintilla", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN,
			borderedge,
			borderedge + topspace,
			rect.right - 2 * borderedge,
			rect.bottom - 2 * borderedge - topspace,
			hwndDlg, 0, ghModule, NULL);

		if (hwndScintilla)
		{

			lpfnEditWndProc = (WNDPROC)SetWindowLongPtr(hwndScintilla, GWLP_WNDPROC, (LONG_PTR)SubClassProc);
			::SetFocus(hwndScintilla);

			fn = (sptr_t(__cdecl *)(sptr_t*, int, uptr_t, sptr_t))SendMessage(hwndScintilla, SCI_GETDIRECTFUNCTION, 0, 0);
			ptr = (sptr_t*)SendMessage(hwndScintilla, SCI_GETDIRECTPOINTER, 0, 0);

			char buffer[64];
			DWORD dw;

			fn(ptr, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
			SetConsoleDefaults();
			
			fn(ptr, SCI_SETMARGINWIDTHN, 1, 0);

			std::list< std::string > loglist;
			getLogText(loglist);
			for (std::list< std::string >::iterator iter = loglist.begin(); iter != loglist.end(); iter++)
			{
				if (!strncmp(iter->c_str(), DEFAULT_PROMPT, 2) || !strncmp(iter->c_str(), CONTINUATION_PROMPT, 2))
				{
					AppendLog(iter->c_str(), 0, 0);
				}
				else AppendLog(iter->c_str(), 1, 0);
			}
			Prompt();

			//if (!wlInit) 
			initWordList();

			tid = ::SetTimer(hwndDlg, TIMERID_FLUSHBUFFER, 1000, TimerProc);

		}
		else
		{
			DWORD dwErr = ::GetLastError();
			char sz[64];
			sprintf_s(sz, 64, "FAILED: 0x%x", dwErr);
			OutputDebugStringA(sz);
			::MessageBoxA(0, sz, "ERR", MB_OK);
			EndDialog(hwndDlg, IDCANCEL);
			return TRUE;
		}
		hWndConsole = hwndDlg;

		if (rectConsole.right == rectConsole.left)
		{
			::SetWindowPos(hwndDlg, HWND_TOP, 0, 0, 600, 400, SWP_NOZORDER);
		}
		else
		{
			::SetWindowPos(hwndDlg, HWND_TOP, rectConsole.left, rectConsole.top,
				rectConsole.right - rectConsole.left, rectConsole.bottom - rectConsole.top,
				SWP_NOZORDER);
		}

		break;

	case WM_SIZE:
	case WM_WINDOWPOSCHANGED:
	case WM_SIZING:
		::GetClientRect(hwndDlg, &rect);
		::SetWindowPos(hwndScintilla, HWND_TOP,
			borderedge,
			borderedge + topspace,
			rect.right - 2 * borderedge,
			rect.bottom - 2 * borderedge - topspace,
			SWP_NOMOVE | SWP_NOZORDER | SWP_DEFERERASE);
		if ( message == WM_SIZE ) UpdateConsoleWidth();
		break;

	case WM_ERASEBKGND:
		break;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code)
		{
		case SCN_MODIFIED: // what am I trapping this for? // A: was thinking about handling PASTE
		{
			// SCNotification *scn = (SCNotification*)lParam;
			// DebugOut("Modified: 0x%x\n", scn->modificationType);
		}

		case SCN_CHARADDED:
		{
			SCNotification *scn = (SCNotification*)lParam;
			// DebugOut("CA: %x\n", scn->ch);

			// I think because I switched to utf-8 I'm gettings
			// double notifications

			if (scn->ch) testAutocomplete();
		}
			break;
		}
		break;

	case WM_SETFOCUS:
		::SetFocus(hwndScintilla);
		break;

	case WM_APPEND_LOG:
		AppendLog((const char*)lParam);
		break;

	case WM_DESTROY:
		::GetWindowRect(hwndDlg, &rectConsole);
		if (tid)
		{
			::KillTimer(hwndDlg, tid);
			tid = 0;
		}
		hWndConsole = 0;
		::PostQuitMessage(0);
		break;

	case WM_CALL_COMPLETE:
		CallComplete((PARSE_STATUS_2)wParam, lParam);
		break;

	case WM_COMMAND:

		switch (LOWORD(wParam))
		{
		case ID_CONSOLE_CLEARHISTORY:
			ClearHistory();
			break;

		case ID_CONSOLE_INSTALLPACKAGES:
			SafeCall(SCC_INSTALLPACKAGES, 0, 0);
			break;

		case ID_CONSOLE_HOMEDIRECTORY:
			BERT_HomeDirectory();
			break;

		case ID_CONSOLE_RELOADSTARTUPFILE:
			SafeCall(SCC_RELOAD_STARTUP, 0, 0);
			break;

		case WM_REBUILD_WORDLISTS:
			initWordList();
			break;

		case ID_CONSOLE_CONSOLEOPTIONS:
			ConsoleOptions(hwndDlg);
			break;

		case ID_CONSOLE_ALWAYSONTOP:
			ToggleOnTop(hwndDlg);
			reopenWindow = true;
			::SetFocus(hWndExcel);
			ShowWindow(hwndDlg, SW_HIDE);
			::PostMessage(hwndDlg, WM_DESTROY, 0, 0);
			break;

		case ID_CONSOLE_CLOSECONSOLE:
		case WM_CLOSE_CONSOLE:
		case IDOK:
		case IDCANCEL:
			::GetWindowRect(hwndDlg, &rectConsole);
			if (tid)
			{
				::KillTimer(hwndDlg, tid);
				tid = 0;
			}
			EndDialog(hwndDlg, wParam);
			::CloseWindow(hwndDlg);
			hWndConsole = 0;
			::PostQuitMessage(0);
			return TRUE;


		}
		break;
	}

	return DefWindowProc(hwndDlg, message, wParam, lParam);

	return FALSE;
}

DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
	const wchar_t CLASS_NAME[] = L"BERT Console Window";
	static bool first = true;

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	WNDCLASS wc = {};

	wc.lpfnWndProc = WindowProc;
	wc.hInstance = ghModule;
	wc.lpszClassName = CLASS_NAME;

	if (first && !RegisterClass(&wc))
	{
		DWORD dwErr = GetLastError();
		DebugOut("ERR %x\n", dwErr);
	}

	// Create the window.

	hWndExcel = (HWND)lpParameter;

	// we may close and re-open the window.

	reopenWindow = true;
	while (reopenWindow)
	{
		reopenWindow = false;

		HWND hwndParent = 0;
		DWORD dw;

		if (CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_ON_TOP) && dw)
			hwndParent = hWndExcel;

		HWND hwnd = CreateWindowEx(
			0,                              // Optional window styles.
			CLASS_NAME,                     // Window class
			L"Console",						// Window text
			WS_OVERLAPPEDWINDOW,            // Window style

			// Size and position
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

			hwndParent,		// Parent window    
			NULL,			// Menu
			ghModule,		// Instance handle
			NULL			// Additional application data
			);

		if (hwnd == NULL)
		{
			DWORD dwErr = GetLastError();
			DebugOut("ERR %x\n", dwErr);

			return 0;
		}

		if (first) CenterWindow(hwnd, (HWND)lpParameter);
		first = false;

		ShowWindow(hwnd, SW_SHOW);

		// Run the message loop.

		MSG msg = {};
		while (GetMessage(&msg, NULL, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		hWndConsole = 0;

		::SetFocus(hWndExcel);

	}

	CoUninitialize();
	return 0;
}

void RunThreadedConsole(HWND excel)
{
	if (hWndConsole)
	{
		::ShowWindow(hWndConsole, SW_SHOWDEFAULT);
		::SetWindowPos(hWndConsole, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	}
	else
	{
		DWORD dwID;

		Marshal(); // FIXME: do this elsewhere, and once
		UpdateWordList();
		::CreateThread(0, 0, ThreadProc, excel, 0, &dwID);
	}

}

