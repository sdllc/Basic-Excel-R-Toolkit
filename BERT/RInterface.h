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

#ifndef __RINTERFACE_H
#define __RINTERFACE_H

#include "XLCALL.H"
#include "util.h"

#include "Autocomplete.h"

#define MAX_LOGLIST_SIZE	80
#define MAX_CMD_HISTORY		2500

#define CONSOLE_HISTORY_FILE ".BERT-Console.history"

#define ENV_NAME "BERT"
#define R_WORKSPACE_NAME ".BERT-RData"

typedef enum
{
	CC_EXCEL =					1,

	CC_ADD_USER_BUTTON =		100,
	CC_CLEAR_USER_BUTTONS =		101,
	
	CC_HISTORY =				200,

	CC_REMAP_FUNCTIONS =		300,

	CC_WATCHFILES =				1020,
	CC_CLEAR =					1021,
	CC_RELOAD =					1022,
	CC_CLOSECONSOLE =			1023,

	LAST_CALLBACK_COMMAND

} CALLBACK_COMMANDS;

typedef std::pair < std::string, std::string > SPAIR;
typedef std::vector< SPAIR > RFUNCDESC;
typedef std::vector< RFUNCDESC > FDVECTOR;

typedef std::unordered_map< std::string, std::string > STRING2STRINGMAP;


/**
 * new class allows (optional) storage of actual functions, plus 
 * (also optional) evaluation environment.  
 */
class RFuncDesc2 {

public:
	void * func;
	void * env;

	std::string r_name;
	std::string function_description;
	std::string function_category;

public:
	std::vector < SPAIR > pairs;

public:

	/* ctor */
	RFuncDesc2(void * func, void * env, const char *name ) : func(func), env(env), r_name(name) {}

	/* dtor */
	~RFuncDesc2() {}

	/* copy constructor (for containers) */
	RFuncDesc2(const RFuncDesc2 &rhs) {

		func = rhs.func;
		env = rhs.env;
		r_name = rhs.r_name;
		function_description = rhs.function_description;
		function_category = rhs.function_category;

		for (std::vector < SPAIR > ::const_iterator iter = rhs.pairs.begin(); iter != rhs.pairs.end(); iter++) {
			pairs.push_back(*iter);
		}

	}

};

typedef std::vector< RFuncDesc2 > FD2VECTOR;


extern std::string dllpath;

extern FD2VECTOR RFunctions;

int RInit();
void RShutdown();

LPXLOPER12 BERT_RExec(LPXLOPER12 code);
bool RExec2(LPXLOPER12 rslt, std::string &funcname, std::vector< LPXLOPER12 > &args);
bool RExec3(LPXLOPER12 rslt, std::string &funcname);
bool RExec4(LPXLOPER12 rslt, RFuncDesc2 &func, std::vector< LPXLOPER12 > &args);

void RExecVector(std::vector < std::string > &vec, int *err = 0, PARSE_STATUS_2 *parseErr = 0, bool printResult = true, bool excludeFromHistory = false, std::string *pjresult = 0);
int UpdateR(std::string &str);

void ClearFunctions();
void MapFunctions();
void LoadStartupFile();

/**
 * asynchronous break, stopping a long-running or runaway function
 */
void userBreak();

int getAutocomplete( AutocompleteData &autocomplete, std::string &line, int caret);

int notifyWatch(std::string &path);
int setWidth(int w);

short BERT_InstallPackages();

void flush_log();

#endif // #ifndef __RINTERFACE_H
