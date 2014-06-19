/*
 * Basic Excel R Toolkit (BERT)
 * Copyright (C) 2014 Structured Data, LLC
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
#include "ExcelFunctions.h"
#include "ThreadLocalStorage.h"
#include "resource.h"
#include "Dialogs.h"
#include "RegistryUtils.h"

#include "Scintilla.h"
#include <string>
#include <list>
#include <vector>

typedef std::vector< std::string> SVECTOR;
typedef SVECTOR::iterator SITER;

SVECTOR cmdVector;
SVECTOR historyVector;
int historyPointer = 0;
std::string historyCurrentLine;

extern HWND hWndConsole;
extern HMODULE ghModule;

int minCaret = 0;
WNDPROC lpfnEditWndProc = 0;
sptr_t(*fn)(sptr_t*, int, uptr_t, sptr_t);
sptr_t* ptr;

void CenterWindow(HWND hWnd, HWND hParent, int offsetX = 0, int offsetY = 0)
{
	RECT rectWnd;
	RECT rectDlg;

	int iWidth, iHeight;
	int iPWidth, iPHeight;

	::GetWindowRect(hParent, &rectWnd);
	::GetWindowRect(hWnd, &rectDlg);

	iWidth = rectDlg.right - rectDlg.left;
	iHeight = rectDlg.bottom - rectDlg.top;

	iPWidth = rectWnd.right - rectWnd.left;
	iPHeight = rectWnd.bottom - rectWnd.top;

	::SetWindowPos(hWnd, HWND_TOP,
		rectWnd.left + offsetX + (iPWidth - iWidth) / 2,
		rectWnd.top + offsetY + (iPHeight - iHeight) / 2,
		0, 0, SWP_NOSIZE);
}

DIALOG_RESULT_TYPE CALLBACK AboutDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:

		::SetWindowTextA(::GetDlgItem(hwndDlg, IDC_STATIC_ABOUT_BERT), ABOUT_BERT_TEXT);
		::SetWindowTextA(::GetDlgItem(hwndDlg, IDC_STATIC_ABOUT_R), ABOUT_R_TEXT);

		CenterWindow(hwndDlg, ::GetParent(hwndDlg));
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

DIALOG_RESULT_TYPE CALLBACK OptionsDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	char buffer[MAX_PATH];
	HWND hWnd;

	switch (message)
	{
	case WM_INITDIALOG:
		CenterWindow(hwndDlg, ::GetParent(hwndDlg));
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
			::GetWindowTextA(::GetDlgItem(hwndDlg, IDC_RHOME), buffer, MAX_PATH - 1);
			CRegistryUtils::SetRegExpandString(HKEY_CURRENT_USER, buffer, REGISTRY_KEY, REGISTRY_VALUE_R_HOME );
			::GetWindowTextA(::GetDlgItem(hwndDlg, IDC_RUSER), buffer, MAX_PATH - 1);
			CRegistryUtils::SetRegExpandString(HKEY_CURRENT_USER, buffer, REGISTRY_KEY, REGISTRY_VALUE_R_USER);
			::GetWindowTextA(::GetDlgItem(hwndDlg, IDC_ENVIRONMENT), buffer, MAX_PATH - 1);
			CRegistryUtils::SetRegString(HKEY_CURRENT_USER, buffer, REGISTRY_KEY, REGISTRY_VALUE_ENVIRONMENT);
			::GetWindowTextA(::GetDlgItem(hwndDlg, IDC_STARTUPFILE), buffer, MAX_PATH - 1);
			CRegistryUtils::SetRegString(HKEY_CURRENT_USER, buffer, REGISTRY_KEY, REGISTRY_VALUE_STARTUP);

		case IDCANCEL:
			EndDialog(hwndDlg, wParam);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void AppendLog(const char *buffer, int style)
{
	// TODO: if there's a current prompt, then
	// need to carry it over (and hide it, maybe)

	int len = strlen(buffer);
	int start = fn(ptr, SCI_GETLENGTH, 0, 0);

	fn(ptr, SCI_SETSEL, -1, -1);
	fn(ptr, SCI_APPENDTEXT, len, (sptr_t)buffer);

	if (style != 0)
	{
		fn(ptr, SCI_STARTSTYLING, start, 0x31);
		fn(ptr, SCI_SETSTYLING, len, style);
	}
}

void Prompt( const char *prompt = DEFAULT_PROMPT )
{
	fn(ptr, SCI_APPENDTEXT, strlen(prompt), (sptr_t)prompt);
	fn(ptr, SCI_SETSEL, -1, -1);
	minCaret = fn(ptr, SCI_GETCURRENTPOS, 0, 0);
	historyPointer = 0;
}

void ProcessCommand()
{
	int len = fn(ptr, SCI_GETLENGTH, 0, 0);
	//int pos = fn(ptr, SCI_GETCURRENTPOS, 0, 0);

	/*

	don't do this, it's wicked annoying

	if (len != pos)
	{
		// scrub remainder of line
		// ...
		fn(ptr, SCI_SETSEL, pos, len);
		fn(ptr, SCI_REPLACESEL, 0, (sptr_t)(""));
	}
	*/

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
		cmd = trim(cmd);
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
		logMessage(str.lpstrText, len+1, false);
		delete[] str.lpstrText;
	}

	fn(ptr, SCI_APPENDTEXT, 1, (sptr_t)"\n");

	if (cmd.length() > 0)
	{
		cmdVector.push_back(cmd);
		PARSE_STATUS_2 ps = RExecVectorBuffered(cmdVector);

		switch (ps)
		{
		case PARSE2_ERROR:
			logMessage(PARSE_ERROR_MESSAGE, strlen(PARSE_ERROR_MESSAGE), true);
			break;

		case PARSE2_INCOMPLETE:
			Prompt(CONTINUATION_PROMPT);
			return;
		}
	}
	else if (cmdVector.size() > 0)
	{
		Prompt(CONTINUATION_PROMPT);
		return;
	}
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
		switch (wParam)
		{
		case 13:
			ProcessCommand();
			return 0;

		default:
			p = fn(ptr, SCI_GETCURRENTPOS, 0, 0);
			if (p <= minCaret) fn(ptr, SCI_SETSEL, -1, -1);
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

		switch (wParam)
		{
		case VK_LEFT:
		case VK_BACK:
			p = fn(ptr, SCI_GETCURRENTPOS, 0, 0);
			if (p <= minCaret) return 0;
			break;

		case VK_UP:
			CmdHistory(-1);
			return 0;

		case VK_DOWN:
			CmdHistory(1);
			return 0;

		case VK_RETURN:
			return 0;
		}

		break;

	}

	return CallWindowProc(lpfnEditWndProc, hwnd, msg, wParam, lParam);
}

DIALOG_RESULT_TYPE CALLBACK ConsoleDlgProc( HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	const int borderedge = 8;
	static HWND hwndScintilla = 0;
	HWND hWnd = 0;
	char *szBuffer = 0;
	RECT rect;

	switch (message)
	{
	//case WM_CREATE:
	case WM_INITDIALOG:
		
		::GetClientRect(hwndDlg, &rect);
		hwndScintilla = CreateWindowExA(0,
			"Scintilla", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN,
			borderedge, borderedge, rect.right - 2 * borderedge, rect.bottom - 2 * borderedge, hwndDlg, 0, ghModule, NULL);

		if (hwndScintilla)
		{
			
			lpfnEditWndProc = (WNDPROC)SetWindowLongPtr(hwndScintilla, GWLP_WNDPROC, (LONG_PTR)SubClassProc);
			::SetFocus(hwndScintilla);

			fn = (sptr_t(__cdecl *)(sptr_t*, int, uptr_t, sptr_t))SendMessage(hwndScintilla, SCI_GETDIRECTFUNCTION, 0, 0);
			ptr = (sptr_t*)SendMessage(hwndScintilla, SCI_GETDIRECTPOINTER, 0, 0);

			fn(ptr, SCI_STYLESETFONT, STYLE_DEFAULT, (sptr_t)SCINTILLA_FONT_NAME);
			fn(ptr, SCI_STYLESETSIZE, STYLE_DEFAULT, SCINTILLA_FONT_SIZE);
			fn(ptr, SCI_STYLESETFORE, 1, SCINTILLA_R_TEXT_COLOR);
			fn(ptr, SCI_STYLESETFORE, 0, SCINTILLA_USER_TEXT_COLOR);

			std::list< std::string > *loglist = getLogText();
			for (std::list< std::string >::iterator iter = loglist->begin(); iter != loglist->end(); iter++)
			{
				if (!strncmp(iter->c_str(), DEFAULT_PROMPT, 2)
					|| !strncmp(iter->c_str(), CONTINUATION_PROMPT, 2))
				{
					AppendLog(iter->c_str(), 0);
				}
				else AppendLog(iter->c_str());
			}
			Prompt();
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
		CenterWindow(hwndDlg, ::GetParent(hwndDlg));
		break;


	case WM_SIZING:
		::GetClientRect(hwndDlg, &rect);
		::SetWindowPos(hwndScintilla, HWND_TOP, borderedge, borderedge, rect.right - 2 * borderedge, rect.bottom - 2 * borderedge,
			SWP_NOMOVE | SWP_NOZORDER | SWP_DEFERERASE);
		break;

	case WM_ERASEBKGND:
		break;

	case WM_NOTIFY:
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

/*
void ConsoleDlg(HINSTANCE hInst)
{
	

	::DialogBox(hInst,
		MAKEINTRESOURCE(IDD_DIALOG1),
		(HWND)xWnd.val.w,
		(DLGPROC)ConsoleDlgProc);

	Excel12(xlFree, 0, 1, (LPXLOPER12)&xWnd);

}
*/
