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

#include "RInterface.h"
#include "Dialogs.h"
#include "Console.h"
#include "resource.h"
#include <Richedit.h>

#include "RegistryUtils.h"

#include <shellapi.h>
#include <strsafe.h>

std::vector < double > functionEntries;
std::list< std::string > loglist;

HWND hWndConsole = 0;

/* mutex for locking the console message history */
HANDLE muxLogList = 0;

AutocompleteData autocomplete;

extern void FreeStream();
extern void SetExcelPtr( LPVOID p, LPVOID ribbon );

LPXLOPER12 BERTFunctionCall( 
	int index
	, LPXLOPER12 input_0
	, LPXLOPER12 input_1
	, LPXLOPER12 input_2
	, LPXLOPER12 input_3
	, LPXLOPER12 input_4
	, LPXLOPER12 input_5
	, LPXLOPER12 input_6
	, LPXLOPER12 input_7
	, LPXLOPER12 input_8
	, LPXLOPER12 input_9
	, LPXLOPER12 input_10
	, LPXLOPER12 input_11
	, LPXLOPER12 input_12
	, LPXLOPER12 input_13
	, LPXLOPER12 input_14
	, LPXLOPER12 input_15
	){

	XLOPER12 * rslt = get_thread_local_xloper12();
	resetXlOper( rslt );

	rslt->xltype = xltypeErr;
	rslt->val.err = xlerrName;

	if (index < 0 || index >= RFunctions.size()) return rslt;

//	RFUNCDESC func = RFunctions[index];
	RFuncDesc2 func = RFunctions[index];

	std::vector< LPXLOPER12 > args;

	// in order to support (...) args, we should allow
	// any number of args.  register every function with 
	// the maximum, and check here for null rather than
	// checking the arity.

	// check for xltypeMissing, not null, although there might 
	// be omitted arguments... 
	
	// we could handle functions with (...) specially, rather
	// than requiring this work for every function.  still
	// it shouldn't be that expensive... although there are
	// better ways to handle it (count and remove >1)

	args.push_back(input_0);
	args.push_back(input_1);
	args.push_back(input_2);
	args.push_back(input_3);
	args.push_back(input_4);
	args.push_back(input_5);
	args.push_back(input_6);
	args.push_back(input_7);
	args.push_back(input_8);
	args.push_back(input_9);
	args.push_back(input_10);
	args.push_back(input_11);
	args.push_back(input_12);
	args.push_back(input_13);
	args.push_back(input_14);
	args.push_back(input_15);

	while (args.size() > 0 && args[args.size() - 1]->xltype == xltypeMissing) args.pop_back();

	//RExec2(rslt, func[0].first, args);
	RExec4(rslt, func, args);

	return rslt;
}

LPXLOPER12 BERT_RCall(
	LPXLOPER12 func
	, LPXLOPER12 input_0
	, LPXLOPER12 input_1
	, LPXLOPER12 input_2
	, LPXLOPER12 input_3
	, LPXLOPER12 input_4
	, LPXLOPER12 input_5
	, LPXLOPER12 input_6
	, LPXLOPER12 input_7 )
{
	XLOPER12 * rslt = get_thread_local_xloper12();
	resetXlOper(rslt);

	rslt->xltype = xltypeErr;
	rslt->val.err = xlerrName;

	std::string funcNarrow;

	if (func->xltype == xltypeStr)
	{
		// FIXME: this seems fragile, as it might try to 
		// allocate a really large contiguous block.  better
		// to walk through and find lines?

		int len = WideCharToMultiByte(CP_UTF8, 0, &(func->val.str[1]), func->val.str[0], 0, 0, 0, 0);
		char *ps = new char[len + 1];
		WideCharToMultiByte(CP_UTF8, 0, &(func->val.str[1]), func->val.str[0], ps, len, 0, 0);
		ps[len] = 0;
		funcNarrow = ps;
	}
	else return rslt;

	std::vector< LPXLOPER12 > args;
	args.push_back(input_0);
	args.push_back(input_1);
	args.push_back(input_2);
	args.push_back(input_3);
	args.push_back(input_4);
	args.push_back(input_5);
	args.push_back(input_6);
	args.push_back(input_7);
	while (args.size() > 0 && args[args.size() - 1]->xltype == xltypeMissing) args.pop_back();
	RExec2(rslt, funcNarrow, args);
	return rslt;
}

void logMessage(const char *buf, int len, bool console)
{
	std::string entry;

	if (!strncmp(buf, WRAP_ERR, sizeof(WRAP_ERR)-1)) // ??
	{
		entry = "Error:";
		entry += (buf + sizeof(WRAP_ERR)-1);
	}
	else entry = buf;

	DWORD stat = WaitForSingleObject(muxLogList, INFINITE);
	if (stat == WAIT_OBJECT_0)
	{
		loglist.push_back(entry);
		while (loglist.size() > MAX_LOGLIST_SIZE) loglist.pop_front();
		ReleaseMutex(muxLogList);
	}

	if (console && hWndConsole)
	{
		::SendMessage(hWndConsole, WM_APPEND_LOG, 0, (LPARAM)entry.c_str());
	}
}

void resetXlOper(LPXLOPER12 x)
{
	if (x->xltype == xltypeStr && x->val.str)
	{
		delete[] x->val.str;
		x->val.str = 0;
	}
	else if (x->xltype == xltypeMulti && x->val.array.lparray)
	{
		// have to consider the case that there are strings
		// in the array, or even nested multis (not that that
		// is a useful thing to do, but it could happen)

		int len = x->val.array.columns * x->val.array.rows;
		for (int i = 0; i < len; i++) resetXlOper(&(x->val.array.lparray[i]));

		delete[] x->val.array.lparray;
		x->val.array.lparray = 0;
	}
	x->val.err = xlerrNull;
	x->xltype = xltypeNil;
}

LPXLOPER BERT_Volatile(LPXLOPER arg)
{
	return arg;
}

short BERT_Reload()
{
	ClearFunctions();
	LoadStartupFile();
	MapFunctions();
	RegisterAddinFunctions();

	return 1;
}

/**
 * open the home directory.  note that this is thread-safe,
 * it can be called directly from the threaded console (and it is)
 */
short BERT_HomeDirectory()
{
	char buffer[MAX_PATH];
	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, buffer, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_USER))
		ExpandEnvironmentStringsA(DEFAULT_R_USER, buffer, MAX_PATH);
	::ShellExecuteA(NULL, "open", buffer, NULL, NULL, SW_SHOWDEFAULT);
	return 1;
}

/**
 * set the excel callback pointer.
 */
int BERT_SetPtr( LPVOID pdisp, LPVOID pribbon )
{
	SetExcelPtr(pdisp, pribbon);
	return 2;
}

short BERT_Configure()
{
	// ::MessageBox(0, L"No", L"Options", MB_OKCANCEL | MB_ICONINFORMATION);

	XLOPER12 xWnd;
	Excel12(xlGetHwnd, &xWnd, 0);

	// InitRichEdit();

	::DialogBox(ghModule,
		MAKEINTRESOURCE(IDD_CONFIG),
		(HWND)xWnd.val.w,
		(DLGPROC)OptionsDlgProc);

	Excel12(xlFree, 0, 1, (LPXLOPER12)&xWnd);

	return 1;
}

void ClearConsole()
{
	if (hWndConsole)
	{
		::PostMessageA(hWndConsole, WM_COMMAND, MAKEWPARAM(WM_CLEAR_BUFFER, 0), 0);
	}
}

void CloseConsole()
{
	if (hWndConsole)
	{
		::PostMessageA(hWndConsole, WM_COMMAND, MAKEWPARAM(WM_CLOSE_CONSOLE,0), 0);
	}
}

void CloseConsoleAsync()
{
	if (hWndConsole)
	{
		::PostMessageA(hWndConsole, WM_COMMAND, MAKEWPARAM(WM_CLOSE_CONSOLE_ASYNC, 0), 0);
	}
}

short BERT_About()
{
	XLOPER12 xWnd;
	Excel12(xlGetHwnd, &xWnd, 0);

	::DialogBox(ghModule,
		MAKEINTRESOURCE(IDD_ABOUT),
		(HWND)xWnd.val.w,
		(DLGPROC)AboutDlgProc);

	Excel12(xlFree, 0, 1, (LPXLOPER12)&xWnd);
	return 1;
}

/**
 * send text to (or clear text in) the excel status bar, via 
 * the Excel API.  this must be called from within an Excel call
 * context.
 */
void ExcelStatus(const char *message)
{
	XLOPER12 xlUpdate;
	XLOPER12 xlMessage;

	xlUpdate.xltype = xltypeBool;

	if (!message)
	{
		xlUpdate.val.xbool = false;
		Excel12(xlcMessage, 0, 1, &xlUpdate );
	}
	else
	{
		int len = MultiByteToWideChar(CP_UTF8, 0, message, -1, 0, 0);

		xlUpdate.val.xbool = true;

		xlMessage.xltype = xltypeStr | xlbitXLFree;
		xlMessage.val.str = new XCHAR[len + 2];
		xlMessage.val.str[0] = len == 0 ? 0 : len - 1;

		MultiByteToWideChar(CP_UTF8, 0, message, -1, &(xlMessage.val.str[1]), len+1);

		Excel12(xlcMessage, 0, 2, &xlUpdate, &xlMessage);
	}

}


void clearLogText()
{
	DWORD stat = WaitForSingleObject(muxLogList, INFINITE);
	if (stat == WAIT_OBJECT_0)
	{
		loglist.clear();
		ReleaseMutex(muxLogList);
	}
}

void getLogText(std::list< std::string > &list)
{
	DWORD stat = WaitForSingleObject(muxLogList, INFINITE);
	if (stat == WAIT_OBJECT_0)
	{
		for (std::list< std::string> ::iterator iter = loglist.begin();
			iter != loglist.end(); iter++)
		{
			list.push_back(*iter);
		}
		ReleaseMutex(muxLogList);
	}
}


PARSE_STATUS_2 RExecVectorBuffered(std::vector<std::string> &cmd, bool excludeFromHistory )
{
	PARSE_STATUS_2 ps2;
	int err;
	RExecVector(cmd, &err, &ps2, true, excludeFromHistory);
	return ps2;
}

short BERT_Console()
{
	static HANDLE hModScintilla = 0;

	XLOPER12 xWnd;
	Excel12(xlGetHwnd, &xWnd, 0);

	if (!hModScintilla)
	{
		char buffer[MAX_PATH];
		if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, buffer, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_USER))
			ExpandEnvironmentStringsA(DEFAULT_R_USER, buffer, MAX_PATH);

		if (strlen(buffer) > 0)
		{
			if (buffer[strlen(buffer) - 1] != '\\') strcat_s(buffer, MAX_PATH, "\\");
		}

#ifdef _WIN64
		strcat_s( buffer, MAX_PATH, "SciLexer64.DLL");
#else // #ifdef _WIN64
		strcat_s(buffer, MAX_PATH, "SciLexer32.DLL");
#endif // #ifdef _WIN64
		hModScintilla = LoadLibraryA(buffer);
		if (!hModScintilla)
		{
			DebugOut(buffer, MAX_PATH, "Failed to load scintilla module: 0x%x\n", ::GetLastError());
			OutputDebugStringA(buffer);
			::MessageBoxA(0, buffer, "ERR", MB_OK);
		}
		else
		{
			//OutputDebugStringA("Scintilla loaded OK\n");
		}
	}

	RunThreadedConsole((HWND)xWnd.val.w);

	Excel12(xlFree, 0, 1, (LPXLOPER12)&xWnd);
	return 1;
}

void SysCleanup()
{
	if (hWndConsole)
	{
		::CloseWindow(hWndConsole);
		::SendMessage(hWndConsole, WM_DESTROY, 0, 0);
	}

	RShutdown();
	FreeStream();

	CloseHandle(muxLogList);

}

void SysInit()
{

	muxLogList = CreateMutex(0, 0, 0);

	if (RInit()) return;

	// loop through the function table and register the functions

	RegisterBasicFunctions();
	RegisterAddinFunctions();

}

/**
 * this is the second part of the context-switching callback
 * used when running functions from the console.  it is only
 * called (indirectly) by the SafeCall function.
 *
 * FIXME: reorganize this around cmdid, clearer switching
 */
long BERT_SafeCall(long cmdid, LPXLOPER12 xl, LPXLOPER xl2)
{
	SVECTOR sv;
	bool excludeFlag = false;

	if (cmdid == 0x08) {
		userBreak();
	}
	else if (xl->xltype & xltypeStr)
	{
		std::string func;
		NarrowString(func, xl);

		if (cmdid == 4) {
			return notifyWatch(func);
		}
		else if (cmdid == 2) {
			return -1; // getNames(moneyList, func);
		}
		else if (cmdid == 10) {
			sv.push_back(func);
			excludeFlag = true;
		}
		else if (cmdid == 12) {
			int caret = xl2->val.num;
			return getAutocomplete( autocomplete, func, caret);
			// return 0;
		}
		else {
			return -1; // getCallTip(calltip, func);
		}
	}
	else if (xl->xltype & xltypeMulti)
	{
		// excel seems to prefer columns for one dimension,
		// but it doesn't actually matter as it's passed
		// as a straight vector

		int r = xl->val.array.rows;
		int c = xl->val.array.columns;
		int m = r * c;

		for (int i = 0; i < m; i++)
		{
			std::string str;
			NarrowString(str, &(xl->val.array.lparray[i]));
			sv.push_back(str);
		}
	}
	if (sv.size())
	{
		return RExecVectorBuffered(sv, excludeFlag);
	}
	return PARSE2_EOF;
}

/**
 * this is not in util because it uses the Excel API,
 * and util should be clean.  FIXME: split into generic
 * and XLOPER-specific versions.
 */
void NarrowString(std::string &out, LPXLOPER12 pxl)
{
	int i, len = pxl->val.str[0];
	int slen = WideCharToMultiByte(CP_UTF8, 0, &(pxl->val.str[1]), len, 0, 0, 0, 0);
	char *s = new char[slen + 1];
	WideCharToMultiByte(CP_UTF8, 0, &(pxl->val.str[1]), len, s, slen, 0, 0);
	s[slen] = 0;
	out = s;
	delete[] s;
}

LPXLOPER12 BERT_UpdateScript(LPXLOPER12 script)
{
	XLOPER12 * rslt = get_thread_local_xloper12();

	rslt->xltype = xltypeErr;
	rslt->val.err = xlerrValue;

	if (script->xltype == xltypeStr)
	{
		ClearFunctions();

		std::string str;
		NarrowString(str, script);

		if (!UpdateR(str))
		{
			rslt->xltype = xltypeBool;
			rslt->val.xbool = true;
			MapFunctions();
			RegisterAddinFunctions();
		}
		else
		{
			rslt->val.err = xlerrValue;
		}
	}

	return rslt;
}


void UnregisterFunctions()
{
	XLOPER xlRegisterID;

	for (std::vector< double > ::iterator iter = functionEntries.begin(); iter != functionEntries.end(); iter++ )
	{
		xlRegisterID.xltype = xltypeNum;
		xlRegisterID.val.num = *iter;
		Excel12(xlfUnregister, 0, 1, &xlRegisterID);
	}

	functionEntries.clear();

}


bool RegisterBasicFunctions()
{
	LPXLOPER12 xlParm[32];
	XLOPER12 xlRegisterID;

	int err;

	static bool fRegisteredOnce = false;

	char szHelpBuffer[512] = " ";
	bool fExcel12 = false;

	// init memory

	for (int i = 0; i< 32; i++) xlParm[i] = new XLOPER12;

	// get the library; store as the first entry in our parameter list

	Excel12(xlGetName, xlParm[0], 0);

	for (int i = 0; funcTemplates[i][0]; i++)
	{
		for (int j = 0; j < 15; j++)
		{
			int len = wcslen(funcTemplates[i][j]);
			xlParm[j + 1]->xltype = xltypeStr;
			xlParm[j + 1]->val.str = new XCHAR[len + 2];

			// strcpy_s(xlParm[j + 1]->val.str + 1, len + 1, funcTemplates[i][j]);
			// for (int k = 0; k < len; k++) xlParm[j + 1]->val.str[k + 1] = funcTemplates[i][j][k];

			wcscpy_s(&(xlParm[j + 1]->val.str[1]), len + 1, funcTemplates[i][j]);

			xlParm[j + 1]->val.str[0] = len;
		}

		xlRegisterID.xltype = xltypeMissing;
		err = Excel12v(xlfRegister, &xlRegisterID, 16, xlParm);

		Excel12(xlFree, 0, 1, &xlRegisterID);

		for (int j = 0; j < 15; j++)
		{
			delete[] xlParm[j + 1]->val.str;
		}

	}

	// clean up (don't forget to free the retrieved dll xloper in parm 0)

	Excel12(xlFree, 0, 1, xlParm[0]);

	for (int i = 0; i< 32; i++) delete xlParm[i];


	// debugLogf("Exit registerAddinFunctions\n");

	// set state and return

	// CRASHER for crash (recovery) testing // Excel4( xlFree, 0, 1, 1000 );

	fRegisteredOnce = true;
	return true;
}

bool RegisterAddinFunctions()
{

//	LPXLOPER12 xlParm[11 + MAX_ARGUMENT_COUNT];

	LPXLOPER12 *xlParm;
	XLOPER12 xlRegisterID;

	int err;
	int alistCount = 12 + MAX_ARGUMENT_COUNT;

	static bool fRegisteredOnce = false;

	//char szHelpBuffer[512] = " ";
	WCHAR wbuffer[512];
	WCHAR wtmp[64];
	int tlen;
	bool fExcel12 = false;

	// init memory

	xlParm = new LPXLOPER12[alistCount];
	for (int i = 0; i< alistCount; i++) xlParm[i] = new XLOPER12;

	// get the library; store as the first entry in our parameter list

	Excel12( xlGetName, xlParm[0], 0 );

	UnregisterFunctions();

	// { "BERTFunctionCall", "UU", "BERTTest", "Input", "1", "BERT", "", "100", "Test function", "", "", "", "", "", "", "" },

	int fcount = RFunctions.size();

	for (int i = 0; i< fcount && i< MAX_FUNCTION_COUNT; i++)
	{
		//RFUNCDESC func = RFunctions[i];
		RFuncDesc2 func = RFunctions[i];
		int scount = 0;

		for (int j = 1; j < alistCount; j++)
		{
			xlParm[j]->xltype = xltypeMissing;
		}

		for (scount = 0; scount < 9; scount++)
		{
			switch (scount)
			{
			case 0:  
				StringCbPrintf( wbuffer, 256, L"BERTFunctionCall%04d", 1000 + i); 
				break;
			case 1: 
				//for (int k = 0; k < func.size(); k++) szHelpBuffer[k] = 'U';
				//szHelpBuffer[func.size()] = 0;
				StringCbPrintf(wbuffer, 256, L"UUUUUUUUUUUUUUUUU");
				break;
			case 2:

				// FIXME: this is correct, if messy, but unecessary: tokens in R can't 
				// use unicode.  if there are high-byte characters in here the function 
				// name is illegal. [FIXME: is that true? I don't think so].

				tlen = MultiByteToWideChar(CP_UTF8, 0, func.pairs[0].first.c_str(), -1, 0, 0);
				if (tlen > 0) MultiByteToWideChar(CP_UTF8, 0, func.pairs[0].first.c_str(), -1, wtmp, 64);
				else wtmp[0] = 0;
				StringCbPrintf(wbuffer, 256, L"R.%s", wtmp); 
				break;
			case 3: 
				wbuffer[0] = 0;
				for (int k = 1; k < func.pairs.size(); k++)
				{
					tlen = MultiByteToWideChar(CP_UTF8, 0, func.pairs[k].first.c_str(), -1, 0, 0);
					if (tlen > 0) MultiByteToWideChar(CP_UTF8, 0, func.pairs[k].first.c_str(), -1, wtmp, 64);
					else wtmp[0] = 0;
					
					if (wcslen(wbuffer)) StringCbCat(wbuffer, 256, L",");
					StringCbCat(wbuffer, 256, wtmp);
				}
				break;
			case 4: StringCbPrintf(wbuffer, 256, L"1"); break;
			case 5: StringCbPrintf(wbuffer, 256, L"Exported R functions"); break;
			case 6: StringCbPrintf(wbuffer, 256, L"%d", 100 + i); break;
			case 7: StringCbPrintf(wbuffer, 256, L"Exported R function"); break; // ??
			default: StringCbPrintf(wbuffer, 256, L""); break;
			}
			
			int len = wcslen(wbuffer);
			xlParm[scount + 1]->xltype = xltypeStr ;
			xlParm[scount + 1]->val.str = new XCHAR[len + 2];

			for (int k = 0; k < len; k++) xlParm[scount + 1]->val.str[k + 1] = wbuffer[k];
			xlParm[scount + 1]->val.str[0] = len;

		}
			

		// so this is supposed to show the default value; but for some
		// reason, it's truncating some strings (one string?) - FALS.  leave
		// it out until we can figure this out.

		// also, quote default strings.

		for (int j = 0; j < func.pairs.size() - 1 && j< MAX_ARGUMENT_COUNT; j++)
		{
			int len = MultiByteToWideChar(CP_UTF8, 0, func.pairs[j + 1].second.c_str(), -1, 0, 0);
			xlParm[scount + 1]->xltype = xltypeStr;
			xlParm[scount + 1]->val.str = new XCHAR[len + 2];
			xlParm[scount + 1]->val.str[0] = len > 0 ? len-1 : 0;
			MultiByteToWideChar(CP_UTF8, 0, func.pairs[j + 1].second.c_str(), -1, &(xlParm[scount + 1]->val.str[1]), len+1);
			scount++;
		}
		
		// it seems that it always truncates the last character of the last argument
		// help string, UNLESS there's another parameter.  so here you go.  could just
		// have an extra missing? CHECK / TODO / FIXME

		xlParm[scount + 1]->xltype = xltypeStr;
		xlParm[scount + 1]->val.str = new XCHAR[2];
		xlParm[scount + 1]->val.str[0] = 0;
		scount++;

		xlRegisterID.xltype = xltypeMissing;
		err = Excel12v(xlfRegister, &xlRegisterID, scount + 1, xlParm);
		if (xlRegisterID.xltype == xltypeNum)
		{
			functionEntries.push_back(xlRegisterID.val.num);
		}
		Excel12(xlFree, 0, 1, &xlRegisterID);

		for (int j = 1; j < alistCount; j++)
		{
			if (xlParm[j]->xltype == xltypeStr)
			{
				delete[] xlParm[j]->val.str;
			}
			xlParm[j]->xltype = xltypeMissing;
		}

	}
	
	// clean up (don't forget to free the retrieved dll xloper in parm 0)

	Excel12(xlFree, 0, 1, xlParm[0]);

	for (int i = 0; i< alistCount; i++) delete xlParm[i];
	delete [] xlParm;

	// debugLogf("Exit registerAddinFunctions\n");

	// set state and return

	// CRASHER for crash (recovery) testing // Excel4( xlFree, 0, 1, 1000 );

	fRegisteredOnce = true;
	return true;
}

#ifdef _DEBUG

int DebugOut(const char *fmt, ...)
{
	static char msg[1024 * 32]; // ??
	int ret;
	va_list args;
	va_start(args, fmt);

	ret = vsprintf_s(msg, fmt, args);
	OutputDebugStringA(msg);

	va_end(args);
	return ret;
}

#endif

BFC(1000);
BFC(1001);
BFC(1002);
BFC(1003);
BFC(1004);
BFC(1005);
BFC(1006);
BFC(1007);
BFC(1008);
BFC(1009);
BFC(1010);
BFC(1011);
BFC(1012);
BFC(1013);
BFC(1014);
BFC(1015);
BFC(1016);
BFC(1017);
BFC(1018);
BFC(1019);
BFC(1020);
BFC(1021);
BFC(1022);
BFC(1023);
BFC(1024);
BFC(1025);
BFC(1026);
BFC(1027);
BFC(1028);
BFC(1029);
BFC(1030);
BFC(1031);
BFC(1032);
BFC(1033);
BFC(1034);
BFC(1035);
BFC(1036);
BFC(1037);
BFC(1038);
BFC(1039);
BFC(1040);
BFC(1041);
BFC(1042);
BFC(1043);
BFC(1044);
BFC(1045);
BFC(1046);
BFC(1047);
BFC(1048);
BFC(1049);
BFC(1050);
BFC(1051);
BFC(1052);
BFC(1053);
BFC(1054);
BFC(1055);
BFC(1056);
BFC(1057);
BFC(1058);
BFC(1059);
BFC(1060);
BFC(1061);
BFC(1062);
BFC(1063);
BFC(1064);
BFC(1065);
BFC(1066);
BFC(1067);
BFC(1068);
BFC(1069);
BFC(1070);
BFC(1071);
BFC(1072);
BFC(1073);
BFC(1074);
BFC(1075);
BFC(1076);
BFC(1077);
BFC(1078);
BFC(1079);
BFC(1080);
BFC(1081);
BFC(1082);
BFC(1083);
BFC(1084);
BFC(1085);
BFC(1086);
BFC(1087);
BFC(1088);
BFC(1089);
BFC(1090);
BFC(1091);
BFC(1092);
BFC(1093);
BFC(1094);
BFC(1095);
BFC(1096);
BFC(1097);
BFC(1098);
BFC(1099);

BFC(1100);
BFC(1101);
BFC(1102);
BFC(1103);
BFC(1104);
BFC(1105);
BFC(1106);
BFC(1107);
BFC(1108);
BFC(1109);
BFC(1110);
BFC(1111);
BFC(1112);
BFC(1113);
BFC(1114);
BFC(1115);
BFC(1116);
BFC(1117);
BFC(1118);
BFC(1119);
BFC(1120);
BFC(1121);
BFC(1122);
BFC(1123);
BFC(1124);
BFC(1125);
BFC(1126);
BFC(1127);
BFC(1128);
BFC(1129);
BFC(1130);
BFC(1131);
BFC(1132);
BFC(1133);
BFC(1134);
BFC(1135);
BFC(1136);
BFC(1137);
BFC(1138);
BFC(1139);
BFC(1140);
BFC(1141);
BFC(1142);
BFC(1143);
BFC(1144);
BFC(1145);
BFC(1146);
BFC(1147);
BFC(1148);
BFC(1149);
BFC(1150);
BFC(1151);
BFC(1152);
BFC(1153);
BFC(1154);
BFC(1155);
BFC(1156);
BFC(1157);
BFC(1158);
BFC(1159);
BFC(1160);
BFC(1161);
BFC(1162);
BFC(1163);
BFC(1164);
BFC(1165);
BFC(1166);
BFC(1167);
BFC(1168);
BFC(1169);
BFC(1170);
BFC(1171);
BFC(1172);
BFC(1173);
BFC(1174);
BFC(1175);
BFC(1176);
BFC(1177);
BFC(1178);
BFC(1179);
BFC(1180);
BFC(1181);
BFC(1182);
BFC(1183);
BFC(1184);
BFC(1185);
BFC(1186);
BFC(1187);
BFC(1188);
BFC(1189);
BFC(1190);
BFC(1191);
BFC(1192);
BFC(1193);
BFC(1194);
BFC(1195);
BFC(1196);
BFC(1197);
BFC(1198);
BFC(1199);

BFC(1200);
BFC(1201);
BFC(1202);
BFC(1203);
BFC(1204);
BFC(1205);
BFC(1206);
BFC(1207);
BFC(1208);
BFC(1209);
BFC(1210);
BFC(1211);
BFC(1212);
BFC(1213);
BFC(1214);
BFC(1215);
BFC(1216);
BFC(1217);
BFC(1218);
BFC(1219);
BFC(1220);
BFC(1221);
BFC(1222);
BFC(1223);
BFC(1224);
BFC(1225);
BFC(1226);
BFC(1227);
BFC(1228);
BFC(1229);
BFC(1230);
BFC(1231);
BFC(1232);
BFC(1233);
BFC(1234);
BFC(1235);
BFC(1236);
BFC(1237);
BFC(1238);
BFC(1239);
BFC(1240);
BFC(1241);
BFC(1242);
BFC(1243);
BFC(1244);
BFC(1245);
BFC(1246);
BFC(1247);
BFC(1248);
BFC(1249);
BFC(1250);
BFC(1251);
BFC(1252);
BFC(1253);
BFC(1254);
BFC(1255);
BFC(1256);
BFC(1257);
BFC(1258);
BFC(1259);
BFC(1260);
BFC(1261);
BFC(1262);
BFC(1263);
BFC(1264);
BFC(1265);
BFC(1266);
BFC(1267);
BFC(1268);
BFC(1269);
BFC(1270);
BFC(1271);
BFC(1272);
BFC(1273);
BFC(1274);
BFC(1275);
BFC(1276);
BFC(1277);
BFC(1278);
BFC(1279);
BFC(1280);
BFC(1281);
BFC(1282);
BFC(1283);
BFC(1284);
BFC(1285);
BFC(1286);
BFC(1287);
BFC(1288);
BFC(1289);
BFC(1290);
BFC(1291);
BFC(1292);
BFC(1293);
BFC(1294);
BFC(1295);
BFC(1296);
BFC(1297);
BFC(1298);
BFC(1299);

BFC(1300);
BFC(1301);
BFC(1302);
BFC(1303);
BFC(1304);
BFC(1305);
BFC(1306);
BFC(1307);
BFC(1308);
BFC(1309);
BFC(1310);
BFC(1311);
BFC(1312);
BFC(1313);
BFC(1314);
BFC(1315);
BFC(1316);
BFC(1317);
BFC(1318);
BFC(1319);
BFC(1320);
BFC(1321);
BFC(1322);
BFC(1323);
BFC(1324);
BFC(1325);
BFC(1326);
BFC(1327);
BFC(1328);
BFC(1329);
BFC(1330);
BFC(1331);
BFC(1332);
BFC(1333);
BFC(1334);
BFC(1335);
BFC(1336);
BFC(1337);
BFC(1338);
BFC(1339);
BFC(1340);
BFC(1341);
BFC(1342);
BFC(1343);
BFC(1344);
BFC(1345);
BFC(1346);
BFC(1347);
BFC(1348);
BFC(1349);
BFC(1350);
BFC(1351);
BFC(1352);
BFC(1353);
BFC(1354);
BFC(1355);
BFC(1356);
BFC(1357);
BFC(1358);
BFC(1359);
BFC(1360);
BFC(1361);
BFC(1362);
BFC(1363);
BFC(1364);
BFC(1365);
BFC(1366);
BFC(1367);
BFC(1368);
BFC(1369);
BFC(1370);
BFC(1371);
BFC(1372);
BFC(1373);
BFC(1374);
BFC(1375);
BFC(1376);
BFC(1377);
BFC(1378);
BFC(1379);
BFC(1380);
BFC(1381);
BFC(1382);
BFC(1383);
BFC(1384);
BFC(1385);
BFC(1386);
BFC(1387);
BFC(1388);
BFC(1389);
BFC(1390);
BFC(1391);
BFC(1392);
BFC(1393);
BFC(1394);
BFC(1395);
BFC(1396);
BFC(1397);
BFC(1398);
BFC(1399);

BFC(1400);
BFC(1401);
BFC(1402);
BFC(1403);
BFC(1404);
BFC(1405);
BFC(1406);
BFC(1407);
BFC(1408);
BFC(1409);
BFC(1410);
BFC(1411);
BFC(1412);
BFC(1413);
BFC(1414);
BFC(1415);
BFC(1416);
BFC(1417);
BFC(1418);
BFC(1419);
BFC(1420);
BFC(1421);
BFC(1422);
BFC(1423);
BFC(1424);
BFC(1425);
BFC(1426);
BFC(1427);
BFC(1428);
BFC(1429);
BFC(1430);
BFC(1431);
BFC(1432);
BFC(1433);
BFC(1434);
BFC(1435);
BFC(1436);
BFC(1437);
BFC(1438);
BFC(1439);
BFC(1440);
BFC(1441);
BFC(1442);
BFC(1443);
BFC(1444);
BFC(1445);
BFC(1446);
BFC(1447);
BFC(1448);
BFC(1449);
BFC(1450);
BFC(1451);
BFC(1452);
BFC(1453);
BFC(1454);
BFC(1455);
BFC(1456);
BFC(1457);
BFC(1458);
BFC(1459);
BFC(1460);
BFC(1461);
BFC(1462);
BFC(1463);
BFC(1464);
BFC(1465);
BFC(1466);
BFC(1467);
BFC(1468);
BFC(1469);
BFC(1470);
BFC(1471);
BFC(1472);
BFC(1473);
BFC(1474);
BFC(1475);
BFC(1476);
BFC(1477);
BFC(1478);
BFC(1479);
BFC(1480);
BFC(1481);
BFC(1482);
BFC(1483);
BFC(1484);
BFC(1485);
BFC(1486);
BFC(1487);
BFC(1488);
BFC(1489);
BFC(1490);
BFC(1491);
BFC(1492);
BFC(1493);
BFC(1494);
BFC(1495);
BFC(1496);
BFC(1497);
BFC(1498);
BFC(1499);


