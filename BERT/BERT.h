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

#ifndef __BERT_H
#define __BERT_H

#include "BERT_Version.h"
#include "RegistryConstants.h"
#include "util.h"
#include "DebugOut.h"

#include "XLCALL.H"

#define DLLEX extern "C" __declspec(dllexport)

extern HMODULE ghModule;
extern IDispatch *pApp;

extern HANDLE muxWordlist;

extern std::vector< std::string > *wlist;
extern std::string calltip;


static LPWSTR funcTemplates[][16] = {
	{ L"BERT_UpdateScript", L"UU#", L"BERT.UpdateScript", L"R Code", L"2", L"BERT", L"", L"99", L"Update Script", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_RExec", L"UU", L"BERT.Exec", L"R Code", L"2", L"BERT", L"", L"98", L"Exec R Code", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_Configure", L"A#", L"BERT.Configure", L"", L"2", L"BERT", L"", L"97", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_Console", L"A#", L"BERT.Console", L"", L"2", L"BERT", L"", L"96", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_HomeDirectory", L"A#", L"BERT.Home", L"", L"2", L"BERT", L"", L"95", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_InstallPackages", L"A#", L"BERT.InstallPackages", L"", L"2", L"BERT", L"", L"94", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_Reload", L"A#", L"BERT.Reload", L"", L"2", L"BERT", L"", L"93", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_About", L"A#", L"BERT.About", L"", L"2", L"BERT", L"", L"92", L"", L"", L"", L"", L"", L"", L"", L"" },

	{ L"BERT_SafeCall", L"JU#", L"BERT.SafeCall", L"", L"2", L"BERT", L"", L"92", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ 0 }
};

/*

static LPWSTR menuTemplates[][4] = {
	{ L"R Console",				L"BERT.Console",			L"",	L"Open the console" },
	{ L"Home Directory",		L"BERT.Home",				L"",	L"Open the home directory" },
	{ L"Reload Startup File",	L"BERT.Reload",				L"",	L"Reload startup R code" },
	{ L"Install Packages",		L"BERT.InstallPackages",	L"",	L"Install packages via the R GUI" },
	{ L"-", L"", L"", L"" },
	{ L"Configuration",			L"BERT.Configure",			L"",	L"Show configuration options" },
	{ L"About",					L"BERT.About",				L"",	L"About Basic Excel R Toolkit (BERT)" },
	{ 0 }
};

*/

const char WRAP_ERR[] = "Error in eval(expr, envir, enclos) :";

// #define BERT_MENU_NAME L"BERT"


/** 
 * convenience macro for setting an XLOPER12 string.  
 * sets first char to length.  DOES NOT COPY STRING.
 */
#define XLOPER12STR( x, s )			\
	x.xltype = xltypeStr;			\
	s[0] = wcslen((WCHAR*)s + 1);	\
	x.val.str = s; 

#define MAX_FUNCTION_COUNT 100
#define MAX_ARGUMENT_COUNT 16

DLLEX LPXLOPER12 BERT_UpdateScript(LPXLOPER12 script);

std::string trim(const std::string& str, const std::string& whitespace = " \t\r\n");

/**
 * static (or thread-local) XLOPERs may have
 * allocated data, we need to clean it up
 */
void resetXlOper( LPXLOPER12 x );

/**
 * de-register any functions we have previously registered, via the local vector
 */
void UnregisterFunctions();

/** register internal (non-dynamic) functions */
bool RegisterBasicFunctions();

void SysInit();
void SysCleanup();
void UpdateWordList();

/** add or remove the menu (old-school menu style) */
// void SetBERTMenu( bool add = true );

/** run configuration */
short BERT_Configure();

/** show console (log) */
short BERT_Console();

/** */
short BERT_HomeDirectory();

/** */
short BERT_Reload();

void CloseConsole();

PARSE_STATUS_2 RExecVectorBuffered(std::vector<std::string> &cmd );

/** */
void ExcelStatus(const char *message);

/** */
void logMessage(const char *buf, int len, bool console = true);

/** */
void getLogText( std::list< std::string > &list );

/** */
void clearLogText();

/**
* register functions (dynamic)
*/
bool RegisterAddinFunctions();

void NarrowString(std::string &out, LPXLOPER12 pxl);


/**
 * generic call dispatcher function, exported from dll
 */
__inline LPXLOPER12 BERTFunctionCall(
	int findex,
	LPXLOPER12 input_0 = 0
	, LPXLOPER12 input_1 = 0
	, LPXLOPER12 input_2 = 0
	, LPXLOPER12 input_3 = 0
	, LPXLOPER12 input_4 = 0
	, LPXLOPER12 input_5 = 0
	, LPXLOPER12 input_6 = 0
	, LPXLOPER12 input_7 = 0
	, LPXLOPER12 input_8 = 0
	, LPXLOPER12 input_9 = 0
	, LPXLOPER12 input_10 = 0
	, LPXLOPER12 input_11 = 0
	, LPXLOPER12 input_12 = 0
	, LPXLOPER12 input_13 = 0
	, LPXLOPER12 input_14 = 0
	, LPXLOPER12 input_15 = 0
	);

#define BFC(num) \
LPXLOPER12 BERTFunctionCall ## num ( \
	LPXLOPER12 input_0 = 0 \
	, LPXLOPER12 input_1 = 0 \
	, LPXLOPER12 input_2 = 0 \
	, LPXLOPER12 input_3 = 0 \
	, LPXLOPER12 input_4 = 0 \
	, LPXLOPER12 input_5 = 0 \
	, LPXLOPER12 input_6 = 0 \
	, LPXLOPER12 input_7 = 0 \
	, LPXLOPER12 input_8 = 0 \
	, LPXLOPER12 input_9 = 0 \
	, LPXLOPER12 input_10 = 0 \
	, LPXLOPER12 input_11 = 0 \
	, LPXLOPER12 input_12 = 0 \
	, LPXLOPER12 input_13 = 0 \
	, LPXLOPER12 input_14 = 0 \
	, LPXLOPER12 input_15 = 0 \
	){ return BERTFunctionCall( num-1000, input_0, input_1, input_2, input_3, input_4, input_5, input_6, input_7, input_8, input_9, input_10, input_11, input_12, input_13, input_14, input_15 ); }

#endif // #ifndef __BERT_H
