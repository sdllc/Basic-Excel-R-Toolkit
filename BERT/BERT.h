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

#include <string>
#include <vector>
#include <list>

#include "BERT_Version.h"
#include "RegistryConstants.h"
#include "util.h"
#include "DebugOut.h"

#include "XLCALL.H"

#define DLLEX extern "C" __declspec(dllexport)

extern HMODULE ghModule;

static LPSTR funcTemplates[][16] = {
	{ "BERT_UpdateScript", "UU#", "BERT.UpdateScript", "R Code", "2", "BERT", "", "99", "Update Script", "", "", "", "", "", "", "" },
	{ "BERT_RExec", "UU", "BERT.Exec", "R Code", "2", "BERT", "", "98", "Exec R Code", "", "", "", "", "", "", "" },
	{ "BERT_Configure", "A#", "BERT.Configure", "", "2", "BERT", "", "97", "", "", "", "", "", "", "", "" },
	{ "BERT_Console", "A#", "BERT.Console", "", "2", "BERT", "", "96", "", "", "", "", "", "", "", "" },
	{ "BERT_HomeDirectory", "A#", "BERT.Home", "", "2", "BERT", "", "95", "", "", "", "", "", "", "", "" },
	{ "BERT_InstallPackages", "A#", "BERT.InstallPackages", "", "2", "BERT", "", "94", "", "", "", "", "", "", "", "" },
	{ "BERT_Reload", "A#", "BERT.Reload", "", "2", "BERT", "", "93", "", "", "", "", "", "", "", "" },
	{ "BERT_About", "A#", "BERT.About", "", "2", "BERT", "", "92", "", "", "", "", "", "", "", "" },
	{ 0 }
};

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

const char WRAP_ERR[] = "Error in eval(expr, envir, enclos) :";


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

/** add or remove the menu (old-school menu style) */
void SetBERTMenu( bool add = true );

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
std::list< std::string > * getLogText();

/** */
void clearLogText();

/**
* register functions (dynamic)
*/
bool RegisterAddinFunctions();

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
