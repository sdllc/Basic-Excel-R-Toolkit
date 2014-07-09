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

extern void flush_log();

bool inputlock = false;

RECT rectConsole = { 0, 0, 0, 0 };

extern HRESULT SafeCall(SAFECALL_CMD cmd, std::vector< std::string > *vec, int *presult);
extern HRESULT Marshal();
extern HRESULT Unmarshal();
extern HRESULT ReleaseThreadPtr();

void initWordList()
{
	static int lastWLLen = 2500;

	// fuck it we'll do it live

	wordList.clear();
	moneyList.clear();

	wordList.reserve(lastWLLen + 100);
	moneyList.reserve(lastWLLen + 100);

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

	SVECTOR tmp;
	tmp.reserve(lastWLLen + 100);

	DWORD stat = WaitForSingleObject(muxWordlist, INFINITE);
	if (stat == WAIT_OBJECT_0)
	{
		DebugOut("Length of wl: %d\n", wlist.size());
		lastWLLen = wlist.size();

		for (SVECTOR::iterator iter = wlist.begin(); iter != wlist.end(); iter++)
		{
			std::string str(*iter);
			tmp.push_back(str);
		}
		ReleaseMutex(muxWordlist);
	}
	tmp.insert(tmp.end(), baseWordList.begin(), baseWordList.end());

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

void AppendLog(const char *buffer, int style, int checkoverlap)
{
	// TODO: if there's a current prompt, then
	// need to carry it over (and hide it, maybe)

	Sci_TextRange tr;
	int linelen;
	int cp;

	tr.lpstrText = 0;

	if (checkoverlap && !inputlock)
	{
		int lc = fn(ptr, SCI_GETLINECOUNT, 0, 0);
		int pos = fn(ptr, SCI_POSITIONFROMLINE, lc - 1, 0);

		cp = fn(ptr, SCI_GETCURRENTPOS, 0, 0);
		if (cp <= pos) cp = -1;

		linelen = fn(ptr, SCI_LINELENGTH, lc - 1, 0);

		tr.chrg.cpMin = pos;
		tr.chrg.cpMax = pos + linelen;
		tr.lpstrText = new char[linelen + 1];

		fn(ptr, SCI_GETTEXTRANGE, 0, (sptr_t)&tr);
		fn(ptr, SCI_DELETERANGE, pos, linelen);

	}

	int len = strlen(buffer);
	int start = fn(ptr, SCI_GETLENGTH, 0, 0);

	fn(ptr, SCI_SETSEL, -1, -1);
	fn(ptr, SCI_APPENDTEXT, len, (sptr_t)buffer);

	if (style != 0)
	{
		fn(ptr, SCI_STARTSTYLING, start, 0x31);
		fn(ptr, SCI_SETSTYLING, len, style);
	}

	if (tr.lpstrText)
	{
		fn(ptr, SCI_SETSEL, -1, -1);
		fn(ptr, SCI_APPENDTEXT, linelen, (sptr_t)tr.lpstrText);
		minCaret += len;
		delete[] tr.lpstrText;
		if (cp >= 0) fn(ptr, SCI_SETSEL, cp + len, cp + len);
	}

}

void Prompt(const char *prompt = DEFAULT_PROMPT)
{
	fn(ptr, SCI_APPENDTEXT, strlen(prompt), (sptr_t)prompt);
	fn(ptr, SCI_SETXOFFSET, 0, 0);
	fn(ptr, SCI_SETSEL, -1, -1);
	minCaret = fn(ptr, SCI_GETCURRENTPOS, 0, 0);
	historyPointer = 0;

}

void ProcessCommand()
{
	DebugOut("ProcessCommand:\t%d\n", GetTickCount());

	int len = fn(ptr, SCI_GETLENGTH, 0, 0);

	// close tip, autoc

	fn(ptr, SCI_AUTOCCANCEL, 0, 0);
	fn(ptr, SCI_CALLTIPCANCEL, 0, 0);



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
		//PARSE_STATUS_2 ps = RExecVectorBuffered(cmdVector);

		int ips = 0;
		SafeCall(SCC_EXEC, &cmdVector, &ips);
		PARSE_STATUS_2 ps = (PARSE_STATUS_2)ips;

		inputlock = false;

		DebugOut("Complete:\t%d\n", GetTickCount());

		switch (ps)
		{
		case PARSE2_ERROR:
			logMessage(PARSE_ERROR_MESSAGE, strlen(PARSE_ERROR_MESSAGE), true);
			break;

		case PARSE2_INCOMPLETE:
			Prompt(CONTINUATION_PROMPT);
			return;

		default:
			//DebugOut("Call IWL:\t%d\n", GetTickCount());
			//initWordList(); // every time? (!) 
			wl = true;
			break;
		}

	}
	else if (cmdVector.size() > 0)
	{
		Prompt(CONTINUATION_PROMPT);
		return;
	}
	cmdVector.clear();

	Prompt();

	if (wl) initWordList();

	DebugOut("Done:\t%d\n", GetTickCount());

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

	if (caret == len - 1)
	{
		bool money = false;
		for (--caret; caret >= 2 && isWordChar(c[caret]); caret--){ if (c[caret] == '$') money = true; }
		caret++;
		int slen = strlen(&(c[caret]));
		if (slen > 1)
		{
			std::string str = &(c[caret]);
			SVECTOR::iterator iter = std::lower_bound(money ? moneyList.begin() : wordList.begin(), money ? moneyList.end() : wordList.end(), str);
			c[len - 2]++;
			str = &(c[caret]);
			SVECTOR::iterator iter2 = std::lower_bound(money ? moneyList.begin() : wordList.begin(), money ? moneyList.end() : wordList.end(), str);
			int count = iter2 - iter;

			str = "";
			if (count > 0 && count <= MAX_AUTOCOMPLETE_LIST_LEN)
			{
				for (count = 0; iter < iter2; iter++, count++)
				{
					if (count) str += " ";
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

LRESULT CALLBACK WindowProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
//DIALOG_RESULT_TYPE CALLBACK ConsoleDlgProc( HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam )
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
			if (hMenu != INVALID_HANDLE_VALUE)
				::SetMenu(hwndDlg, hMenu);
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

			if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, buffer, 63, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_FONT)
				|| !strlen(buffer)) strcpy_s(buffer, 64, SCINTILLA_FONT_NAME);
			fn(ptr, SCI_STYLESETFONT, STYLE_DEFAULT, (sptr_t)buffer);

			if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_SIZE)
				|| dw <= 0) dw = SCINTILLA_FONT_SIZE;
			fn(ptr, SCI_STYLESETSIZE, STYLE_DEFAULT, dw);

			if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_CONSOLE_NORMAL)
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
			//CenterWindow(hwndDlg, ::GetParent(hwndDlg));
		}
		else
		{
			::SetWindowPos(hwndDlg, HWND_TOP, rectConsole.left, rectConsole.top,
				rectConsole.right - rectConsole.left, rectConsole.bottom - rectConsole.top,
				SWP_NOZORDER);
			::GetClientRect(hwndDlg, &rect);
			::SetWindowPos(hwndScintilla, HWND_TOP,
				borderedge,
				borderedge + topspace,
				rect.right - 2 * borderedge,
				rect.bottom - 2 * borderedge - topspace,
				SWP_NOMOVE | SWP_NOZORDER | SWP_DEFERERASE);
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
		break;

	case WM_ERASEBKGND:
		break;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code)
		{
		case SCN_MODIFIED:
		{
			SCNotification *scn = (SCNotification*)lParam;
			DebugOut("Modified: 0x%x\n", scn->modificationType);
		}

		case SCN_CHARADDED:
		{
			SCNotification *scn = (SCNotification*)lParam;
			DebugOut("CA: %x\n", scn->ch);

			// I think because I switched to utf-8 I'm getting
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

	case WM_COMMAND:

		switch (LOWORD(wParam))
		{
		case ID_CONSOLE_CLEARHISTORY:
			ClearHistory();
			break;

		case ID_CONSOLE_INSTALLPACKAGES:
			//BERT_InstallPackages();
			SafeCall(SCC_INSTALLPACKAGES, 0, 0);
			break;

		case ID_CONSOLE_HOMEDIRECTORY:
			BERT_HomeDirectory();
			break;

		case ID_CONSOLE_RELOADSTARTUPFILE:
			//BERT_Reload();
			SafeCall(SCC_RELOAD_STARTUP, 0, 0);
			break;

		case WM_REBUILD_WORDLISTS:
			initWordList();
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

/*
void ConsoleDlg(HINSTANCE hInst)
{


::DialogBox(hInst,
MAKEINTRESOURCE(IDD_CONSOLE),
(HWND)xWnd.val.w,
(DLGPROC)ConsoleDlgProc);

Excel12(xlFree, 0, 1, (LPXLOPER12)&xWnd);

}
*/

DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
	const wchar_t CLASS_NAME[] = L"BERT Console Window";
	static bool first = true;

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	Unmarshal();

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

	HWND hwnd = CreateWindowEx(
		0,                              // Optional window styles.
		CLASS_NAME,                     // Window class
		L"Console",						// Window text
		WS_OVERLAPPEDWINDOW,            // Window style

		// Size and position
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

		0, // (HWND)lpParameter,       // Parent window    
		NULL,       // Menu
		ghModule,  // Instance handle
		NULL        // Additional application data
		);

	if (hwnd == NULL)
	{
		DWORD dwErr = GetLastError();
		DebugOut("ERR %x\n", dwErr);

		return 0;
	}

	ShowWindow(hwnd, SW_SHOW);

	if (first) CenterWindow(hwnd, (HWND)lpParameter);
	first = false;

	// Run the message loop.

	MSG msg = {};
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	hWndConsole = 0;

	// FreeStream();
	ReleaseThreadPtr();

	CoUninitialize();
	return 0;
}

HWND RunInThread(HWND excel)
{
	DWORD dwID;

	if (hWndConsole)
	{
		::ShowWindow(hWndConsole, SW_SHOWDEFAULT);
		::SetWindowPos(hWndConsole, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	}
	else
	{
		Marshal();
		UpdateWordList();
		::CreateThread(0, 0, ThreadProc, excel, 0, &dwID);
	}
	//ThreadProc(excel);
	return 0;
}

