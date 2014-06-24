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
#include "ThreadLocalStorage.h"
#include "resource.h"
#include "Dialogs.h"
#include "RegistryUtils.h"

#include "Scintilla.h"
#include <string>
#include <list>
#include <vector>
#include <regex>
#include <sstream>
#include <algorithm>

typedef std::vector< std::string> SVECTOR;
typedef SVECTOR::iterator SITER;

SVECTOR cmdVector;
SVECTOR historyVector;

SVECTOR wordList;
SVECTOR moneyList;
SVECTOR baseWordList;
bool wlInit = false;

int historyPointer = 0;
std::string historyCurrentLine;

extern HWND hWndConsole;
extern HMODULE ghModule;

int minCaret = 0;
WNDPROC lpfnEditWndProc = 0;
sptr_t(*fn)(sptr_t*, int, uptr_t, sptr_t);
sptr_t* ptr;

extern SVECTOR & getWordList(SVECTOR &wordList);
extern int getCallTip(std::string &callTip, const std::string &sym);


RECT rectConsole = { 0, 0, 0, 0 };

SVECTOR & split(const std::string &s, char delim, int minLength, SVECTOR &elems)
{
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) 
	{
		if( !item.empty() && item.length() >= minLength ) elems.push_back(item);
	}
	return elems;
}

void initWordList()
{
	// fuck it we'll do it live

	wordList.clear();

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
					line = trim(line);
					if (line.length() > 0 && line.c_str()[0] != '#')
					{
						split(line, ' ', MIN_WORD_LENGTH, baseWordList);
					}
				}
			}
			wlInit = true;
		}
	}

	SVECTOR tmp;

	tmp.insert(tmp.end(), baseWordList.begin(), baseWordList.end());
	getWordList(tmp);

	for (SVECTOR::iterator iter = tmp.begin(); iter != tmp.end(); iter++)
	{
		if (iter->find('$') == std::string::npos) wordList.push_back(*iter);
		else moneyList.push_back(*iter);
	}

	std::sort(wordList.begin(), wordList.end());
	std::sort(moneyList.begin(), moneyList.end());

	wordList.erase(std::unique(wordList.begin(), wordList.end()), wordList.end());
	moneyList.erase(std::unique(moneyList.begin(), moneyList.end()), moneyList.end());

}

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

		switch (wParam)
		{
		case VK_ESCAPE:
			if (!fn(ptr, SCI_AUTOCACTIVE, 0, 0)
				&& !fn(ptr, SCI_CALLTIPACTIVE, 0, 0))
			{
				::PostMessage(::GetParent(hwnd), WM_COMMAND, WM_CLOSE_CONSOLE, 0);
				return 0;
			}
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

	// FOR NOW, require that caret is at eol

	if (caret == len-1)
	{
		bool money = false;
		for (--caret; caret >= 2 && isWordChar(c[caret]); caret--){ if (c[caret] == '$') money = true; }
		caret++;
		int slen = strlen(&(c[caret]));
		if (slen > 1)
		{
			std::string str = &(c[caret]);
			SVECTOR::iterator iter = std::lower_bound( money ? moneyList.begin() : wordList.begin(), money? moneyList.end(): wordList.end(), str);
			c[len - 2]++;
			str = &(c[caret]);
			SVECTOR::iterator iter2 = std::lower_bound( money? moneyList.begin(): wordList.begin(), money ? moneyList.end(): wordList.end(), str);
			int count = iter2 - iter;

			str = "";
			if (count > 0 && count <= MAX_AUTOCOMPLETE_LIST_LEN)
			{
				for (count = 0; iter < iter2; iter++, count++)
				{
					if( count ) str += " ";
					str += iter->c_str();
				}
				fn(ptr, SCI_AUTOCSHOW, slen, (sptr_t)(str.c_str()));
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

	if (std::regex_search(sc, mat, rex) /* && !ctvisible */ )
	{
		std::string tip;
		std::string sym(mat[1].first, mat[1].second);

		if (sym.compare(lasttip))
		{
			if (getCallTip(tip, sym))
			{
				OutputDebugStringA(sym.c_str());
				OutputDebugStringA(": ");
				OutputDebugStringA(tip.c_str());
				OutputDebugStringA("\n");

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

	delete [] c;

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

			//if (!wlInit) 
			initWordList();

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

		if ( rectConsole.right == rectConsole.left )
			CenterWindow(hwndDlg, ::GetParent(hwndDlg));
		else
		{
			::SetWindowPos(hwndDlg, HWND_TOP, rectConsole.left, rectConsole.top,
				rectConsole.right - rectConsole.left, rectConsole.bottom - rectConsole.top,
				SWP_NOZORDER);
			::GetClientRect(hwndDlg, &rect);
			::SetWindowPos(hwndScintilla, HWND_TOP, borderedge, borderedge, rect.right - 2 * borderedge, rect.bottom - 2 * borderedge,
				SWP_NOMOVE | SWP_NOZORDER | SWP_DEFERERASE);
		}

		break;


	case WM_SIZING:
		::GetClientRect(hwndDlg, &rect);
		::SetWindowPos(hwndScintilla, HWND_TOP, borderedge, borderedge, rect.right - 2 * borderedge, rect.bottom - 2 * borderedge,
			SWP_NOMOVE | SWP_NOZORDER | SWP_DEFERERASE);
		break;

	case WM_ERASEBKGND:
		break;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code)
		{
		case SCN_CHARADDED:
			testAutocomplete();
			break;
		}
		break;

	case WM_COMMAND:

		switch (LOWORD(wParam))
		{
		case WM_CLOSE_CONSOLE:
		case IDOK:
		case IDCANCEL:
			::GetWindowRect(hwndDlg, &rectConsole);
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
