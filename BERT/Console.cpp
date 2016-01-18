/*
* Basic Excel R Toolkit (BERT)
* Copyright (C) 2014-2016 Structured Data, LLC
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

#include "Shlwapi.h"

#include <deque>

typedef std::vector< std::string> SVECTOR;
typedef SVECTOR::iterator SITER;

void showAutocomplete();

SVECTOR cmdVector;
SVECTOR historyVector;

bool wlInit = false;

bool reopenWindow = false;

int historyPointer = 0;
std::string historyCurrentLine;

extern HWND hWndConsole;
extern HMODULE ghModule;

int minCaret = 0;
WNDPROC lpfnEditWndProc = 0;

sptr_t(*sci_fn)(sptr_t*, int, uptr_t, sptr_t);
sptr_t * sci_ptr;

extern void flush_log();

bool inputlock = false;
bool exitpending = false;
bool appappend = false;
std::string pastebuffer;
std::deque < std::string > pastelines;

RECT rectConsole = { 0, 0, 0, 0 };

extern HRESULT SafeCall(SAFECALL_CMD cmd, std::vector< std::string > *vec, long arg, int *presult);
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

COLORREF getCaretColor(COLORREF backgroundColor){
	double Y = 0.299 * GetRValue(backgroundColor)
		+ 0.587 * GetGValue(backgroundColor)
		+ 0.114 * GetBValue(backgroundColor);
	if (Y < 128) return RGB(255, 255, 255);
	return 0;
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

		if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_SAVE_HISTORY)
			|| dw < 0) dw = 0;
		::SendMessage(::GetDlgItem(hwndDlg, IDC_SAVE_HISTORY), BM_SETCHECK, dw ? BST_CHECKED : BST_UNCHECKED, 0);

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
			sci_fn(sci_ptr, SCI_STYLESETSIZE, STYLE_DEFAULT, dw);

			::GetWindowTextA(::GetDlgItem(hwndDlg, IDC_COMBO_FONTS), buffer, 64);
			sci_fn(sci_ptr, SCI_STYLESETFONT, STYLE_DEFAULT, (sptr_t)buffer);

			sci_fn(sci_ptr, SCI_STYLESETFORE, 1, dwMessage);
			sci_fn(sci_ptr, SCI_STYLESETFORE, 0, dwUser);
			sci_fn(sci_ptr, SCI_STYLESETBACK, 0, dwBack);
			sci_fn(sci_ptr, SCI_STYLESETBACK, 1, dwBack);
			sci_fn(sci_ptr, SCI_STYLESETBACK, 32, dwBack);
			::EnableWindow(::GetDlgItem(hwndDlg, IDAPPLY), 0);

			// caret color.  for dark backgrounds, set caret to white.
			sci_fn(sci_ptr, SCI_SETCARETFORE, getCaretColor(dwBack), 0);

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

			dw = (::SendMessage(::GetDlgItem(hwndDlg, IDC_SAVE_HISTORY), BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
			CRegistryUtils::SetRegDWORD(HKEY_CURRENT_USER, dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_SAVE_HISTORY);

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
	appappend = true;

	if (!checkoverlap || inputlock)
	{
		int start = sci_fn(sci_ptr, SCI_GETLENGTH, 0, 0);

		sci_fn(sci_ptr, SCI_SETSEL, -1, -1);
		sci_fn(sci_ptr, SCI_APPENDTEXT, len, (sptr_t)buffer);

		if (style != 0)
		{
			sci_fn(sci_ptr, SCI_STARTSTYLING, start, 0x31);
			sci_fn(sci_ptr, SCI_SETSTYLING, len, style);
		}

	}
	else
	{
		// insert
		int np = minCaret - promptwidth;

		// insert
		appappend = true;
		sci_fn(sci_ptr, SCI_INSERTTEXT, np, (sptr_t)buffer);
		appappend = false;

		// update our marker, pos should move
		minCaret += len;

		if (style != 0)
		{
			sci_fn(sci_ptr, SCI_STARTSTYLING, np, 0x31);
			sci_fn(sci_ptr, SCI_SETSTYLING, len, style);
		}

	}
	appappend = false;

	sci_fn(sci_ptr, SCI_SCROLLCARET, 0, 0);

}

void Prompt(const char *prompt = DEFAULT_PROMPT)
{
	appappend = true;
	promptwidth = strlen(prompt);
	sci_fn(sci_ptr, SCI_APPENDTEXT, promptwidth, (sptr_t)prompt);
	sci_fn(sci_ptr, SCI_SETXOFFSET, 0, 0);
	sci_fn(sci_ptr, SCI_SETSEL, -1, -1);
	minCaret = sci_fn(sci_ptr, SCI_GETCURRENTPOS, 0, 0);
	historyPointer = 0;
	appappend = false;
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
		break;
	}

	if (!lParam)
	{
		cmdVector.clear();
		Prompt();
	}

}

DWORD WINAPI CallThreadProc(LPVOID lpParameter)
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	HWND hwnd = (HWND)lpParameter;
	int ips = 0;
	SafeCall(SCC_EXEC, &cmdVector, 0, &ips);
	::PostMessage(hwnd, WM_CALL_COMPLETE, ips, 0);
	::CoUninitialize();
	if (exitpending){
		exitpending = false;
		::PostMessage(hwnd, WM_COMMAND, WM_CLOSE_CONSOLE, 0);
	}
	return 0;
}

void CancelCommand(){

	cmdVector.clear();

	appappend = true;
	sci_fn(sci_ptr, SCI_APPENDTEXT, 1, (sptr_t)"\n");
	sci_fn(sci_ptr, SCI_AUTOCCANCEL, 0, 0);
	sci_fn(sci_ptr, SCI_CALLTIPCANCEL, 0, 0);
	appappend = false;

	Prompt();
}

void ProcessCommand()
{
	DebugOut("ProcessCommand:\t%d\n", GetTickCount());

	int len = sci_fn(sci_ptr, SCI_GETLENGTH, 0, 0);

	// close tip, autoc

	sci_fn(sci_ptr, SCI_AUTOCCANCEL, 0, 0);
	sci_fn(sci_ptr, SCI_CALLTIPCANCEL, 0, 0);
	
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
		sci_fn(sci_ptr, SCI_GETTEXTRANGE, 0, (sptr_t)(&str));

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
		sci_fn(sci_ptr, SCI_GETTEXTRANGE, 0, (sptr_t)(&str));
		int len = strlen(str.lpstrText);
		str.lpstrText[len] = '\n';
		str.lpstrText[len + 1] = 0;
		logMessage(str.lpstrText, len + 1, false);
		delete[] str.lpstrText;
	}

	appappend = true;
	sci_fn(sci_ptr, SCI_APPENDTEXT, 1, (sptr_t)"\n");
	appappend = false;
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


}

/**
* clears the buffer history, not the command history
*/
void ClearHistory()
{
	sci_fn(sci_ptr, SCI_SETTEXT, 0, (sptr_t)"");
	clearLogText();
	cmdVector.clear();
	Prompt();
}

void CmdHistory(int scrollBy)
{
	int to = historyPointer + scrollBy;
	if (to > 0) return; // can't go to future
	if (-to > historyVector.size()) return; // too far back
	int end = sci_fn(sci_ptr, SCI_GETLENGTH, 0, 0);
	std::string repl;

	// ok, it's valid.  one thing to check: if on line 0, store it

	if (historyPointer == 0)
	{
		int linelen = end - minCaret;

		Sci_TextRange str;
		str.chrg.cpMin = minCaret;
		str.chrg.cpMax = end;
		str.lpstrText = new char[linelen + 1];
		sci_fn(sci_ptr, SCI_GETTEXTRANGE, 0, (sptr_t)(&str));
		historyCurrentLine = str.lpstrText;
		delete[] str.lpstrText;
	}

	if (to == 0) repl = historyCurrentLine.c_str();
	else
	{
		int check = historyVector.size() + to;
		repl = historyVector[check];
	}

	sci_fn(sci_ptr, SCI_SETSEL, minCaret, end);
	sci_fn(sci_ptr, SCI_REPLACESEL, 0, (sptr_t)(repl.c_str()));
	sci_fn(sci_ptr, SCI_SETSEL, -1, -1);

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

		case VK_TAB:
			if (!sci_fn(sci_ptr, SCI_AUTOCACTIVE, 0, 0))
			{
				showAutocomplete();
				return 0;
			}

		default:
			p = sci_fn(sci_ptr, SCI_GETCURRENTPOS, 0, 0);
			if (p < minCaret) sci_fn(sci_ptr, SCI_SETSEL, -1, -1);

		}
		break;

	case WM_KEYUP:

		switch (wParam)
		{
		case VK_UP:
		case VK_DOWN:
		case VK_RETURN:
			return 0;

		//case VK_TAB:
		//	return 0;

		}
		break;

	case WM_KEYDOWN:

		if (inputlock) {

			// FIXME: buffer typing for after command is complete?  that's 
			// standard behavior in some shells, unless the process grabs 
			// iostreams 

			if (wParam == 'C' && GetKeyState(VK_CONTROL) < 0) {
				SafeCall(SCC_BREAK, 0, 0, 0);
			}
			return 0;
		}

		switch (wParam)
		{

		// don't know if I prefer this as copy or cancel.  ultimately copy 
		// is available via the menu, whilst we need a key for cancel.  
		// FIXME? how about, if there's a selection then copy; otherwise cancel.  
		// too confusing? actually I think it works, it's contextual.

		case 'C':
			if (GetKeyState(VK_CONTROL) < 0){
				DebugOut("Control-c\n");
				int start = sci_fn(sci_ptr, SCI_GETSELECTIONSTART, 0, 0);
				int end = sci_fn(sci_ptr, SCI_GETSELECTIONEND, 0, 0);
				if (end == start){
					DebugOut("Cancel");
					CancelCommand();
				}
			}
			break;

		case VK_ESCAPE:
			if (!sci_fn(sci_ptr, SCI_AUTOCACTIVE, 0, 0)
				&& !sci_fn(sci_ptr, SCI_CALLTIPACTIVE, 0, 0))
			{
				::PostMessage(::GetParent(hwnd), WM_COMMAND, WM_CLOSE_CONSOLE, 0);
				return 0;
			}
			break;

		case VK_HOME:
			sci_fn(sci_ptr, SCI_SETSEL, minCaret, minCaret);
			return 0;
			break;

		case VK_END:
			sci_fn(sci_ptr, SCI_SETSEL, -1, -1);
			break;

		case VK_LEFT:
		case VK_BACK:
			p = sci_fn(sci_ptr, SCI_GETCURRENTPOS, 0, 0);
			if (p <= minCaret) return 0;
			break;

		case VK_UP:
			if (!sci_fn(sci_ptr, SCI_AUTOCACTIVE, 0, 0))
			{
				CmdHistory(-1);
				return 0;
			}
			break;

		case VK_DOWN:
			if (!sci_fn(sci_ptr, SCI_AUTOCACTIVE, 0, 0))
			{
				CmdHistory(1);
				return 0;
			}
			break;

		case VK_RETURN:
			sci_fn(sci_ptr, SCI_AUTOCCANCEL, 0, 0);
			sci_fn(sci_ptr, SCI_CALLTIPCANCEL, 0, 0);
			return 0;

		case VK_TAB:
			if (!sci_fn(sci_ptr, SCI_AUTOCACTIVE, 0, 0))
			{
				return 0;
			}
			break;

		}

		break;

	}

	return CallWindowProc(lpfnEditWndProc, hwnd, msg, wParam, lParam);
}

bool isWordChar(char c)
{
	return (((c >= 'a') && (c <= 'z'))
		|| ((c >= 'A') && (c <= 'Z'))
		|| ((c >= '0') && (c <= '9'))
		|| (c == '_')
		|| (c == '.')
		|| (c == '=') // for parameter symbols
		);
}

const char *lastWord(const char *str) {
	int len = strlen(str);
	for (; len > 0; len--) {
		if (!isWordChar(str[len - 1])) break;
	}
	return str + len;
}

void showAutocomplete()
{
	int len = sci_fn(sci_ptr, SCI_GETCURLINE, 0, 0);
	if (len <= 2) return;
	char *c = new char[len + 1];
	int caret = sci_fn(sci_ptr, SCI_GETCURLINE, len + 1, (sptr_t)c);

	std::string strline = c;

	int sc;
	std::vector< std::string > sv;
	sv.push_back(c + 2);

	SafeCall(SCC_AUTOCOMPLETE, &sv, caret-1, &sc);

	if (autocomplete.signature.length() && autocomplete.token.length() == 0 ) {

		int cp = sci_fn(sci_ptr, SCI_GETCURRENTPOS, 0, 0);
		int lfp = sci_fn(sci_ptr, SCI_LINEFROMPOSITION, cp, 0);
		int pfl = sci_fn(sci_ptr, SCI_POSITIONFROMLINE, lfp, 0);
		
		std::string function = autocomplete.function;
		int index = autocomplete.signature.find_first_of(" (");
		if (index != std::string::npos) function = autocomplete.signature.substr(0, index);

		index = strline.find(function.c_str());

		sci_fn(sci_ptr, SCI_CALLTIPSHOW, pfl + index, (sptr_t)autocomplete.signature.c_str());

	}
	else if (autocomplete.file && autocomplete.token.length()) {

		/*

		if (autocomplete.token.find(":\\") != std::string::npos
			|| (autocomplete.token.length() > 3) && !autocomplete.token.substr(0, 2).compare("\\\\")) {

			int len = MultiByteToWideChar(CP_UTF8, 0, autocomplete.token.c_str(), -1, 0, 0);
			if (len > 0) {

				SVECTOR clist;
				WCHAR wsz[MAX_PATH + 8];
				WCHAR wszFile[MAX_PATH + 8];
				
				MultiByteToWideChar(CP_UTF8, 0, autocomplete.token.c_str(), -1, wsz, MAX_PATH + 8);
				wcscpy_s(wszFile, wsz);

				PathRemoveFileSpecW(wsz);
				PathStripPath(wszFile);

				DebugOut("token: %s\n", autocomplete.token.c_str());

				// PathCchRemoveBackslash is below our target version, and the other function
				// doesn't work with network paths, so... manually?

				int wlen = wcslen(wsz);
				while (wlen > 0 && wsz[wlen - 1] == '\\') {
					wlen--;
					wsz[wlen] = 0;
				}
				wcscat_s(wsz, len + 8, L"\\*");

				HANDLE hFind;
				WIN32_FIND_DATA data;

				char sz[MAX_PATH * 2];

				hFind = FindFirstFileW( wsz, &data );
				if( hFind != INVALID_HANDLE_VALUE ) {

					do {
						WideCharToMultiByte(CP_UTF8, 0, data.cFileName, -1, sz, MAX_PATH * 2, 0, 0);
						clist.push_back(sz);
					} 
					while (FindNextFile(hFind, &data));
					FindClose(hFind);
				}

				if (clist.size()) {

					std::sort(clist.begin(), clist.end());

					char sep[] = "\0";
					std::string newlist = "";
					for (int i = 0; i < clist.size(); i++) {
						const char *sz = clist[i].c_str();
						if (strlen(sz) && sz[0] != '.' && sz[0] != '$') {
							newlist.append(sep);
							newlist.append(sz);
							sep[0] = '\n'; // every time?
						} 
					}

					if (newlist.length()) {
						//sci_fn(sci_ptr, SCI_AUTOCSHOW, caret - X, (sptr_t)(newlist.c_str()));
						sci_fn(sci_ptr, SCI_AUTOCSHOW, wcslen(wszFile), (sptr_t)(newlist.c_str()));
					}
				}

			}


		}

		*/


	}
	else if (autocomplete.comps.length()) {

		int testactive = sci_fn(sci_ptr, SCI_AUTOCACTIVE, 0, 0);

		// there's a case where you are entering a function call, type open paren 
		// (autocomplete shows parameters) then press space for some breathing room -- 
		// this will close the ac list because it's not a character of any of the entries.
		// we want to keep the list open in that case.  hence the space check.

		if (!sci_fn(sci_ptr, SCI_AUTOCACTIVE, 0, 0) || (caret > 0 && !isWordChar( c[caret - 1] )))
		{
			int x = caret;
			for (; x >= 2; x--) {
				if (!isWordChar(c[x - 1])) break;
			}

			SVECTOR clist;
			Util::split(autocomplete.comps, '\n', 1, clist, true);

			// sort symbols (NOTE: this is more expensive than necessary because we're
			// doing it before dropping prefixes; but that might be an overoptimization)

			// UPDATE: I can't figure out why this is necessary, and it's bad for function
			// definitions because it breaks order.

			// UPDATE2: we now use call tips for function definitions which preserve order.
			// sort the ac list so it behaves properly.

			std::sort(clist.begin(), clist.end());

			// here we're dropping prefixes and symbols that start with a dot (typically
			// hidden in R)

			char sep[] = "\0";
			std::string newlist = "";
			for (int i = 0; i < clist.size(); i++) {
				const char *sz = lastWord(clist[i].c_str());

				// FIXME: make this optional? dropping hidden (.) symbols
				if (strlen(sz) && sz[0] != '.' ) {
					newlist.append(sep);
					newlist.append(sz);
					sep[0] = '\n'; // every time?
				}
			}

			if( newlist.length()) sci_fn(sci_ptr, SCI_AUTOCSHOW, caret - x, (sptr_t)(newlist.c_str()));

		}
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
	sci_fn(sci_ptr, SCI_STYLESETFONT, STYLE_DEFAULT, (sptr_t)buffer);

	if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_SIZE)
		|| dw <= 0) dw = SCINTILLA_FONT_SIZE;
	sci_fn(sci_ptr, SCI_STYLESETSIZE, STYLE_DEFAULT, dw);

	if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_MESSAGE)
		|| dw < 0) dw = SCINTILLA_R_TEXT_COLOR;
	sci_fn(sci_ptr, SCI_STYLESETFORE, 1, dw);

	if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_USER)
		|| dw < 0) dw = SCINTILLA_USER_TEXT_COLOR;
	sci_fn(sci_ptr, SCI_STYLESETFORE, 0, dw);

	if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_USER)
		|| dw < 0) dw = SCINTILLA_USER_TEXT_COLOR;
	sci_fn(sci_ptr, SCI_STYLESETFORE, 0, dw);

	if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_BACK)
		|| dw < 0) dw = SCINTILLA_BACK_COLOR;
	sci_fn(sci_ptr, SCI_STYLESETBACK, 0, dw);
	sci_fn(sci_ptr, SCI_STYLESETBACK, 1, dw);
	sci_fn(sci_ptr, SCI_STYLESETBACK, 32, dw);

	// caret color.  for dark backgrounds, set caret to white.
	sci_fn(sci_ptr, SCI_SETCARETFORE, getCaretColor(dw), 0);

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

	if (!sci_fn) return;
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
	SafeCall(SCC_CONSOLE_WIDTH, &sv, 0, &r);

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

		exitpending = false;

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

			sci_fn = (sptr_t(__cdecl *)(sptr_t*, int, uptr_t, sptr_t))SendMessage(hwndScintilla, SCI_GETDIRECTFUNCTION, 0, 0);
			sci_ptr = (sptr_t*)SendMessage(hwndScintilla, SCI_GETDIRECTPOINTER, 0, 0);

			sci_fn(sci_ptr, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
			SetConsoleDefaults();
			
			sci_fn(sci_ptr, SCI_SETMARGINWIDTHN, 1, 0);
			sci_fn(sci_ptr, SCI_CLEARCMDKEY, 'Z' | ((SCMOD_CTRL) << 16), 0); // has no meaning in shell context

			// we use newline as a separator in the AC list so we can support
			// windows paths (with spaces)

			sci_fn(sci_ptr, SCI_AUTOCSETSEPARATOR, '\n', 0);

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

	case WM_CLEANUP_PASTE:
	{
		DebugOut("Cleanup paste: %d len %d\n", wParam, lParam);
		appappend = true;
		sci_fn(sci_ptr, SCI_UNDO, 0, 0);
		sci_fn(sci_ptr, SCI_SETSEL, -1, -1);

		while(pastelines.size() > 0 && !inputlock) {
			std::string &line = pastelines[0];
			appappend = true;
			sci_fn(sci_ptr, SCI_APPENDTEXT, line.length(), (sptr_t)line.c_str());
			appappend = false;
			pastelines.pop_front();
			sci_fn(sci_ptr, SCI_SETSEL, -1, -1);
			ProcessCommand();
		}

		appappend = false;
		break;
	}

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code)
		{
		/*
		case SCN_UPDATEUI:
		{
			SCNotification *scn = (SCNotification*)lParam;
			DebugOut("UD %d\n", scn->updated);
			break;
		}
		*/
		case SCN_MODIFIED: // what am I trapping this for? // A: was thinking about handling PASTE
		{
			SCNotification *scn = (SCNotification*)lParam;
			int line = sci_fn(sci_ptr, SCI_LINEFROMPOSITION, scn->position, 0);
			int last = sci_fn(sci_ptr, SCI_GETLINECOUNT, 0, 0);
			if (!appappend && (scn->modificationType & 0x01) && line != last - 1) {
				DebugOut("0x%x Paste in line %d (%d), LEN IS %d\n", scn->modificationType, line, last, scn->length);
				pastebuffer.assign(scn->text, scn->length);
				if (scn->length != pastebuffer.length()) {
					DebugOut("garbage?\n");
				}
				DebugOut("---\n%s\n---\n", pastebuffer.c_str());

				std::istringstream stream(pastebuffer);
				std::string line;
				while (std::getline(stream, line)) {
					pastelines.push_back(line);
				}

				::PostMessage(hwndDlg, WM_CLEANUP_PASTE, scn->position, scn->length);
			}
			break;
		}

		case SCN_CHARADDED:
		{
			SCNotification *scn = (SCNotification*)lParam;
			// DebugOut("CA: %x\n", scn->ch);

			// I think because I switched to utf-8 I'm gettings
			// double notifications

			if (scn->ch) showAutocomplete();
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
		while (pastelines.size() > 0 && !inputlock) {
			std::string &line = pastelines[0];
			appappend = true;
			sci_fn(sci_ptr, SCI_APPENDTEXT, line.length(), (sptr_t)line.c_str());
			appappend = false;
			pastelines.pop_front();
			sci_fn(sci_ptr, SCI_SETSEL, -1, -1);
			ProcessCommand();
		}

		break;

	case WM_COMMAND:

		switch (LOWORD(wParam))
		{
		case WM_CLEAR_BUFFER:
		case ID_CONSOLE_CLEARHISTORY:
			ClearHistory();
			break;

		case ID_CONSOLE_INSTALLPACKAGES:
			SafeCall(SCC_INSTALLPACKAGES, 0, 0, 0);
			break;

		case ID_CONSOLE_HOMEDIRECTORY:
			BERT_HomeDirectory();
			break;

		case ID_CONSOLE_RELOADSTARTUPFILE:
			SafeCall(SCC_RELOAD_STARTUP, 0, 0, 0);
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

		// this message is used in case the close console command
		// is called interactively; in that event we will be in the middle
		// of a COM call and we don't want to break here.  wait until the
		// call is complete.  if it's called some other way (such as from
		// excel, which it should not be), then you can close.
		case WM_CLOSE_CONSOLE_ASYNC:
			if (inputlock) {
				exitpending = true;
				break;
			}

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
			CONSOLE_WINDOW_TITLE,			// Window text
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
		::CreateThread(0, 0, ThreadProc, excel, 0, &dwID);
	}

}

