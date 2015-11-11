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

#define Win32

#include <windows.h>
#include <stdio.h>
#include <Rversion.h>

// #define USE_RINTERNALS

#include <Rinternals.h>
#include <Rembedded.h>
#include <graphapp.h>
#include <R_ext\RStartup.h>
#include <signal.h>
#include <R_ext\Parse.h>
#include <R_ext\Rdynload.h>

#include "RInterface.h"
#include "resource.h"
#include "RegistryUtils.h"
#include "BERT.h"
#include "StringConstants.h"

#include "FileWatcher.h"

#include "RCOM.h"

FDVECTOR RFunctions;



extern HMODULE ghModule;
SEXP g_Environment = 0;

SEXP ExecR(const char *code, int *err = 0, ParseStatus *pStatus = 0);
SEXP ExecR(std::string &str, int *err = 0, ParseStatus *pStatus = 0);
SEXP ExecR(std::vector< std::string > &vec, int *err = 0, ParseStatus *pStatus = 0, bool withVisible = false);

SEXP XLOPER2SEXP(LPXLOPER12 px, int depth = 0, bool missingArguments = false);

std::string dllpath;

bool g_buffering = false;
std::vector< std::string > logBuffer;

HANDLE muxLog;
HANDLE muxExecR;

//
// for whatever reason these are not exposed in the embedded headers.
//
extern "C" {

extern void R_RestoreGlobalEnvFromFile(const char *, Rboolean);
extern void R_SaveGlobalEnvToFile(const char *);

}

extern void RibbonClearUserButtons();
extern void RibbonAddUserButton(std::string &strLabel, std::string &strFunc, std::string &strImgMso);

int R_ReadConsole(const char *prompt, char *buf, int len, int addtohistory)
{
	fputs(prompt, stdout);
	fflush(stdout);
	if (fgets(buf, len, stdin)) return 1; else return 0;
}

void R_WriteConsole(const char *buf, int len)
{
	DWORD rslt = WaitForSingleObject(muxLog, INFINITE);
	if (g_buffering) logBuffer.push_back(buf);
	else logMessage(buf, len);
	ReleaseMutex(muxLog);
}

void flush_log()
{
	DWORD rslt = WaitForSingleObject(muxLog, INFINITE);
	std::string str;
	for (std::vector< std::string > ::iterator iter = logBuffer.begin(); iter != logBuffer.end(); iter++)
	{
		str += *iter;
	}
	logBuffer.clear();

	// this is dumb.  check ^ before iterating.

	if (str.length() > 0 ) 
		logMessage(str.c_str(), str.length());
	::ReleaseMutex(muxLog);
}

void R_CallBack(void)
{
	/* called during i/o, eval, graphics in ProcessEvents */
	// DebugOut("R_CallBack\n");
}

void myBusy(int which)
{
	/* set a busy cursor ... if which = 1, unset if which = 0 */
	DebugOut("busy\n");
}

/** 
 * "ask ok" has no return value.
 */
void R_AskOk(const char *info) {
	
	::MessageBoxA(0, info, "Message from R", MB_OK);

}

/** 
 * 1 (yes) or -1 (no), I believe (based on #defines)
 */
int R_AskYesNoCancel(const char *question) {

	return (IDYES == ::MessageBoxA(0, question, "Message from R", MB_YESNOCANCEL)) ? 1 : -1;

}

static void my_onintr(int sig) { 
	UserBreak = 1; 
}

void userBreak() {

	// FIXME: synchronize (actually that's probably not helpful, unless we
	// can synchronize with whatever is clearing it inside R, which we can't)

	UserBreak = 1;
	logMessage(USER_BREAK_MESSAGE, 0, 1);

}

int UpdateR(std::string &str)
{
	std::stringstream ss(str);
	std::string line;

	int err;
	ParseStatus status;
	std::vector < std::string > vec;

	while (std::getline(ss, line)){
		line = Util::trim(line);
		if (line.length() > 0) vec.push_back(line);
	}
	if (vec.size() > 0)
	{
		ExecR(vec, &err, &status);
		if (status != PARSE_OK)
		{
			logMessage(PARSE_ERROR_MESSAGE, 0, 1);
			return -3;
		}
		if (err) return -2;
		return 0;
	}

	return -1;

}

void MapFunctions()
{
	SEXP s = 0;
	int err;
	char env[MAX_PATH];

	::WaitForSingleObject(muxExecR, INFINITE);

	RFunctions.clear();

	SVECTOR fnames;

	// if we were holding a pointer to the environment,
	// release it here so we can update (if necessary)

	if (g_Environment)
	{
		UNPROTECT_PTR(g_Environment);
		g_Environment = 0;
	}

	// get the environment, if there's one specified

	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, env, MAX_PATH, REGISTRY_KEY, REGISTRY_VALUE_ENVIRONMENT))
		strcpy_s(env, MAX_PATH, DEFAULT_ENVIRONMENT);

	if( strlen(env) > 0)
	{
		SEXP sargs;
		PROTECT(sargs = Rf_allocVector(VECSXP, 1));
		SET_VECTOR_ELT(sargs, 0, Rf_mkString(env));
		
		SEXP lns = PROTECT(Rf_lang3(Rf_install("do.call"), Rf_mkString("get"), sargs));
		g_Environment = R_tryEval(lns, R_GlobalEnv, &err );
		UNPROTECT(2);

		PROTECT(g_Environment); // hold this ref [FIXME: performance?]
	}

	// get all functions in the target environment 
	// (except q and quit, if we're in the global env)

	SEXP cmd = PROTECT(Rf_lang1(Rf_install("lsf.str")));
	if (cmd)
	{
		int i, j, len = 0;
		SEXP useEnv = g_Environment ? g_Environment : R_GlobalEnv;
		SEXP lst = PROTECT(R_tryEval(cmd, useEnv, &err));
		if ( lst ) len = Rf_length(lst);
		SEXP formals = PROTECT(Rf_install("formals"));
		for (i = 0; i < len; i++)
		{
			RFUNCDESC funcdesc;
			std::string funcName = CHAR(STRING_ELT(lst, i));

			if (!g_Environment && strcmp("q", funcName.c_str()) && strcmp("quit", funcName.c_str()))
			{
				funcdesc.push_back(SPAIR(funcName, ""));
				SEXP args = PROTECT(R_tryEval(Rf_lang2(formals, Rf_mkString(funcName.c_str())), useEnv, &err));
				if (args && isList(args))
				{
					SEXP asvec = PROTECT(Rf_PairToVectorList(args));
					SEXP names = PROTECT(R_tryEval(Rf_lang2(R_NamesSymbol, args), useEnv, &err));
					if (names)
					{
						int plen = Rf_length(names);
						for (j = 0; j < plen; j++)
						{
							std::string dflt = "";
							std::string arg = CHAR(STRING_ELT(names, j));
							SEXP vecelt = VECTOR_ELT(asvec, j);
							int type = TYPEOF(vecelt);

							if ( type != 1 )
							{
								dflt = "Default: ";
								const char *a = CHAR(asChar(vecelt));
								if (type == STRSXP)
								{
									dflt += "\"";
									dflt += a;
									dflt += "\"";
								}
								else if (type == LGLSXP)
								{
									if ((LOGICAL(vecelt))[0]) dflt += "TRUE";
									else dflt += "FALSE";
								}
								else dflt += a;
							}

							funcdesc.push_back(SPAIR(arg, dflt));
						}
					}
					UNPROTECT(2);
				}
				UNPROTECT(1);
				RFunctions.push_back(funcdesc);
			}
		}
		UNPROTECT(2);
	}
	UNPROTECT(1);

	::ReleaseMutex(muxExecR);

}

void LoadStartupFile()
{
	// if there is a startup script, load that now

	char RUser[MAX_PATH];
	char path[MAX_PATH];
	char buffer[MAX_PATH];

	::WaitForSingleObject(muxExecR, INFINITE);

	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, RUser, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_USER))
		ExpandEnvironmentStringsA( DEFAULT_R_USER, RUser, MAX_PATH );

	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, buffer, MAX_PATH, REGISTRY_KEY, REGISTRY_VALUE_STARTUP))
		strcpy_s(buffer, MAX_PATH, DEFAULT_R_STARTUP);

	if (strlen(buffer) && strlen(RUser)) 
	{
		time_t t;
		struct tm timeinfo;
		int errorOccurred;

		// using named args so we can set the chdir flag

		SEXP namedargs;
		const char *names[] = { "file", "chdir", "" };

		sprintf_s(path, MAX_PATH, "%s\\%s", RUser, buffer);

		PROTECT(namedargs = mkNamed(VECSXP, names));
		SET_VECTOR_ELT(namedargs, 0, Rf_mkString(path));
		SET_VECTOR_ELT(namedargs, 1, Rf_ScalarLogical(1));
		R_tryEval(Rf_lang3(Rf_install("do.call"), Rf_mkString("source"), namedargs), R_GlobalEnv, &errorOccurred);
		UNPROTECT(1);

		time(&t);
		localtime_s(&timeinfo, &t);

		if( !errorOccurred )
		{
			strftime(buffer, MAX_PATH, "Read startup file OK @ %c\n", &timeinfo);
			logMessage(buffer, 0, 1);
			ExcelStatus(0);
		}
		else
		{
			strftime(buffer, MAX_PATH, "Error reading startup file @ %c\n", &timeinfo);
			logMessage(buffer, 0, 1);
			ExcelStatus("BERT: Error reading startup file; check console");
		}

	}

	::ReleaseMutex(muxExecR);

}

short BERT_InstallPackages()
{
	ExecR("install.packages()");
	return 1;
}

void installApplicationObject(ULONG_PTR p) {

	int err;

	::WaitForSingleObject(muxExecR, INFINITE);

	// create an "excel" env
	SEXP e = PROTECT(R_tryEval(Rf_lang1(Rf_install("new.env")), R_GlobalEnv, &err));
	if (e)
	{
		Rf_defineVar(Rf_install("EXCEL"), e, R_GlobalEnv);

		// wrap the app object
		SEXP obj = wrapDispatch((ULONG_PTR)p);
		if (obj) Rf_defineVar(Rf_install("Application"), obj, e);

		// add enums to the excel env, rather than stuffing them into the application object
		mapEnums(p, e);

		UNPROTECT(1);
	}

	::ReleaseMutex(muxExecR);

}

int RInit()
{
	structRstart rp;
	Rstart Rp = &rp;
	char Rversion[25];

	char RHome[MAX_PATH];
	char RUser[MAX_PATH];

	DWORD dwPreserve = 0;

	sprintf_s(Rversion, 25, "%s.%s", R_MAJOR, R_MINOR);
	if (strcmp(getDLLVersion(), Rversion) != 0) {
		// fprintf(stderr, "Error: R.DLL version does not match\n");
		// exit(1);
		return -1;
	}

	muxLog = ::CreateMutex(0, 0, 0);
	muxExecR = ::CreateMutex(0, 0, 0);

	R_setStartTime();
	R_DefParams(Rp);

	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, RHome, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_HOME))
	{
		ExpandEnvironmentStringsA(DEFAULT_R_HOME, RHome, MAX_PATH);
	}

	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, RUser, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_USER))
	{
		ExpandEnvironmentStringsA(DEFAULT_R_USER, RUser, MAX_PATH);
	}
	
	if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dwPreserve, REGISTRY_KEY, REGISTRY_VALUE_PRESERVE_ENV)) {
		dwPreserve = DEFAULT_R_PRESERVE_ENV;
	}

	Rp->rhome = RHome;
	Rp->home = RUser;

	Rp->CharacterMode = LinkDLL;
	Rp->ReadConsole = R_ReadConsole;
	Rp->WriteConsole = R_WriteConsole;
	Rp->CallBack = R_CallBack;
	Rp->ShowMessage = R_AskOk;
	Rp->YesNoCancel = R_AskYesNoCancel;
	Rp->Busy = myBusy;

	Rp->R_Quiet = FALSE;// TRUE;        /* Default is FALSE */
	Rp->R_Interactive = TRUE;// FALSE; /* Default is TRUE */

	Rp->RestoreAction = SA_RESTORE;
	Rp->SaveAction = SA_NOSAVE;

	R_SetParams(Rp);
	R_set_command_line_arguments(0, 0);

	FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));

	signal(SIGBREAK, my_onintr);
	GA_initapp(0, 0);
	setup_Rmainloop();
	R_ReplDLLinit();

	::WaitForSingleObject(muxExecR);

	// restore session data here, if desired.  note this is done
	// BEFORE the startup file is loaded, so file definitions may
	// overwrite session definitions.  

	// UPDATE: moved to before loading the BaseFunctions.R -- that
	// was causing some confusion.

	if (dwPreserve) {

		std::string path = RUser;
		int len = path.length();
		if (len && path[len - 1] != '\\') path += "\\";
		path += R_WORKSPACE_NAME;
		R_RestoreGlobalEnvFromFile(path.c_str(), TRUE);

	}

	{
		const char *p = dllpath.c_str();
		int errorOccurred;
		int plen = strlen(p);

		SEXP s = PROTECT(Rf_lang2(Rf_install("dyn.load"), Rf_mkString(p)));
		if ( s ) R_tryEval( s, R_GlobalEnv, &errorOccurred );
		UNPROTECT(1);

		s = PROTECT(Rf_lang1(Rf_install("new.env")));
		if (s)
		{
			SEXP e = PROTECT(R_tryEval(s, R_GlobalEnv, &errorOccurred));
			if (e)
			{
				Rf_defineVar(Rf_install(ENV_NAME), e, R_GlobalEnv);
				Rf_defineVar(Rf_install(".PATH"), Rf_mkString(p), e);
				int idx = 0;
				for (int i = 0; i < plen-1; i++) if (p[i] == '\\') idx = i + 1;
				Rf_defineVar(Rf_install(".MODULE"), Rf_mkString(p+idx), e);

				// remove trailing slash, if any, just to normalize
				while (strlen(RUser) > 0 && RUser[strlen(RUser) - 1] == '\\') RUser[strlen(RUser) - 1] = 0;

				Rf_defineVar(Rf_install("HOME"), Rf_mkString(RUser), e);
				Rf_defineVar(Rf_install("R_HOME"), Rf_mkString(RHome), e);

			}
			UNPROTECT(1);
		}
		UNPROTECT(1);


	}

	// run embedded code (if any)

	HRSRC handle = ::FindResource(ghModule, MAKEINTRESOURCE(IDR_RCDATA1), RT_RCDATA);
	if (handle != 0)
	{
		DWORD len = ::SizeofResource(ghModule, handle);
		HGLOBAL global = ::LoadResource(ghModule, handle);
		if (global != 0 && len > 0 )
		{
			char *sz = (char*)::LockResource(global);
			std::string str(sz, 0, len);
			std::stringstream ss(str);
			std::string line;

			int err;
			ParseStatus status;
			std::vector < std::string > vec;

			while (std::getline(ss, line)){
				line = Util::trim(line);
				if (line.length() > 0) vec.push_back(line);
			}
			if (vec.size() > 0 )
			{
				ExecR( vec, &err, &status );
			}

		}
	}

	::ReleaseMutex(muxExecR);

	{
		// BERT banner in the console.  needs to be narrow, though.
		std::string strBanner = "---\n\n";
		int i, len = wcslen(ABOUT_BERT_TEXT);
		for (i = 0; i < len; i++) strBanner += ((char)(ABOUT_BERT_TEXT[i] & 0xff));
		strBanner += "\n\n";
		logMessage(strBanner.c_str(), 0, 1);
	}

	LoadStartupFile();
	MapFunctions();

	return 0;

}

void RShutdown()
{
	char RUser[MAX_PATH];
	DWORD dwPreserve = 0;

	// save session data here, if desired

	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, RUser, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_USER))
	{
		ExpandEnvironmentStringsA(DEFAULT_R_USER, RUser, MAX_PATH);
	}

	if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dwPreserve, REGISTRY_KEY, REGISTRY_VALUE_PRESERVE_ENV)) {
		dwPreserve = DEFAULT_R_PRESERVE_ENV;
	}

	if (dwPreserve) {

		std::string path = RUser;
		int len = path.length();
		if (len && path[len - 1] != '\\') path += "\\";
		path += R_WORKSPACE_NAME;
		R_SaveGlobalEnvToFile(path.c_str());

	}

	Rf_endEmbeddedR(0);
	CloseHandle(muxLog);
	CloseHandle(muxExecR);

}

SEXP ExecR(std::string &str, int *err, ParseStatus *pStatus)
{
	return ExecR(str.c_str(), err, pStatus);
}


SEXP ExecR(const char *code, int *err, ParseStatus *pStatus)
{
	ParseStatus status;
	SEXP cmdSexp, cmdexpr = R_NilValue;
	SEXP ans = 0;
	int i, errorOccurred;

	::WaitForSingleObject(muxExecR);

	PROTECT(cmdSexp = Rf_allocVector(STRSXP, 1));
	SET_STRING_ELT(cmdSexp, 0, Rf_mkChar(code));
	cmdexpr = PROTECT(R_ParseVector(cmdSexp, -1, &status, R_NilValue));

	if (err) *err = 0;
	if (pStatus) *pStatus = status;

	switch (status){
	case PARSE_OK:

		// Loop is needed here as EXPSEXP might be of length > 1
		for (i = 0; i < Rf_length(cmdexpr); i++){
			SEXP cmd = VECTOR_ELT(cmdexpr, i);
			ans = R_tryEval(cmd, R_GlobalEnv, &errorOccurred);
			if (errorOccurred) {
				if (err) *err = errorOccurred;
				UNPROTECT(2);
				::ReleaseMutex(muxExecR);
				return 0;
			}
		}
		break;

	case PARSE_INCOMPLETE:
		break;

	case PARSE_NULL:
		break;

	case PARSE_ERROR:
		break;

	case PARSE_EOF:
		break;

	default:
		break;
	}

	UNPROTECT(2);

	::ReleaseMutex(muxExecR);
	return ans;

}

SEXP ExecR(std::vector < std::string > &vec, int *err, ParseStatus *pStatus, bool withVisible )
{
	ParseStatus status ;
	SEXP cmdSexp, wv = R_NilValue, cmdexpr = R_NilValue;
	SEXP ans = 0;
	int i, errorOccurred;

	if (vec.size() == 0) return R_NilValue;

	::WaitForSingleObject(muxExecR, INFINITE);

	PROTECT(cmdSexp = Rf_allocVector(STRSXP, vec.size()));
	for (i = 0; i < vec.size(); i++)
	{
		SET_STRING_ELT(cmdSexp, i, Rf_mkChar(vec[i].c_str()));
	}
	cmdexpr = PROTECT(R_ParseVector(cmdSexp, -1, &status, R_NilValue));

	if (err) *err = 0;
	if (pStatus) *pStatus = status;

	switch (status){
	case PARSE_OK:

		if (withVisible)
		{
			wv = PROTECT(Rf_lang2(Rf_install("withVisible"), Rf_lang2(Rf_install("eval"), cmdexpr)));
			ans = R_tryEval(wv, R_GlobalEnv, &errorOccurred);
			if (err && errorOccurred) *err = errorOccurred;
			UNPROTECT(1);
		}
		else
		{
			// Loop is needed here as EXPSEXP might be of length > 1
			for (i = 0; i < Rf_length(cmdexpr); i++){
				SEXP cmd = VECTOR_ELT(cmdexpr, i);
				ans = R_tryEval(cmd, R_GlobalEnv, &errorOccurred);
				if (errorOccurred) {
					if (err) *err = errorOccurred;
					UNPROTECT(2);
					::ReleaseMutex(muxExecR);

					return 0;
				}
			}
		}
		break;

	case PARSE_INCOMPLETE:
		break;

	case PARSE_NULL:
		break;

	case PARSE_ERROR:
		break;

	case PARSE_EOF:
		break;

	default:
		break;
	}

	UNPROTECT(2);
	::ReleaseMutex(muxExecR);

	return ans;

}

__inline void CPLXSXP2XLOPER(LPXLOPER12 result, Rcomplex &c)
{
	// thread issues, FIXME: use TLS
	static char sz[MAX_PATH];

	// see STRSXP2XLOPER
	result->xltype = xltypeStr | xlbitDLLFree;

	// FIXME: make this configurable (i/j/etc)

	sprintf_s(sz, MAX_PATH, "%g+%gi", c.r, c.i);

	int i, len = strlen(sz);

	result->val.str = new XCHAR[len + 2];
	result->val.str[0] = len;

	for (i = 0; i < len; i++) result->val.str[i + 1] = sz[i];

}

__inline void STRSXP2XLOPER(LPXLOPER12 result, SEXP str)
{
	// this is just for our reference, as we do not call
	// xlFree on this object (also, don't call xlFree on
	// this object).

	result->xltype = xltypeStr | xlbitDLLFree;

	const char *sz = CHAR(Rf_asChar(str));

	int len = MultiByteToWideChar(CP_UTF8, 0, sz, -1, 0, 0);

	result->val.str = new XCHAR[len + 1];
	result->val.str[0] = len == 0 ? 0 : len - 1;

	MultiByteToWideChar(CP_UTF8, 0, sz, -1, &(result->val.str[1]), len);

}

/**
 * resolve the string representation of an object, where it might
 * be nested within environments, e.g. env$variable$name.  
 * 
 * UPDATE: added @ for S4 slots, but needs more testing
 * FIXME: support [[ ]] indexing?
 *
 * returns a PROTECTED object so be sure to UNPROTECT it
 */
SEXP resolveObject(const std::string &token, SEXP parent = R_GlobalEnv)
{
	int err;
	for (std::string::const_iterator iter = token.begin(); iter != token.end(); iter++)
	{
		if (*iter == '$' || *iter == '@')
		{
			std::string tmp(token.begin(), iter);
			SEXP obj;
			
			if (*iter == '$') obj = PROTECT(R_tryEvalSilent(Rf_lang2(Rf_install("get"), Rf_mkString(tmp.c_str())), parent, &err));
			else obj = PROTECT(R_do_slot(parent, Rf_mkString(tmp.c_str()))); // @slot

			std::string balance(iter + 1, token.end());
			SEXP elt = resolveObject(balance, obj);
			UNPROTECT(1);
			return elt;
		}
	}
	return PROTECT(R_tryEvalSilent(Rf_lang2(Rf_install("get"), Rf_mkString(token.c_str())), parent, &err));
}

int notifyWatch(std::string &path) {

	int err;
	::WaitForSingleObject(muxExecR, INFINITE);

	SEXP env = PROTECT(R_tryEvalSilent(Rf_lang2(Rf_install("get"), Rf_mkString(ENV_NAME)), R_GlobalEnv, &err));
	if (env)
	{
		SEXP rslt = PROTECT(R_tryEval(Rf_lang2(Rf_install(".ExecWatchCallback"), Rf_mkString(path.c_str())), env, &err));
		UNPROTECT(1);
	}
	UNPROTECT(1);
	::ReleaseMutex(muxExecR);

	return 0;

}

/**
 * get names, or, if the object is an environment, ls
 * FIXME: ls all.names=1
 * FIXME: hashes?
 */
int getNames(SVECTOR &vec, const std::string &token)
{
	int err;
	int ret = 0;
	char cc = '$';

	SEXP obj = resolveObject(token);
	if (obj && TYPEOF(obj) != NILSXP)
	{
		// UPDATE to support refClasses.  these appear as s4 objects, but we 
		// actually want to map the environment (ls), not the class slots (slotNames).  
		// also the deref operator for refClasses is $, not @.

		SEXP rslt;
		bool isEnv = false;
		
		if( TYPEOF(obj) == ENVSXP) isEnv = true; // easy
		else if(TYPEOF(obj) == S4SXP ) { // less so
			rslt = PROTECT(R_tryEval(Rf_lang2(Rf_install("is.environment"), obj), R_GlobalEnv, &err));
			if (rslt && TYPEOF(rslt) != NILSXP) {
				isEnv = (INTEGER(rslt)[0]) ? true : false;
			}
			UNPROTECT(1);
		}

		if (isEnv) rslt = PROTECT(R_tryEval(Rf_lang2(Rf_install("ls"), obj), R_GlobalEnv, &err));
		else if (TYPEOF(obj) == S4SXP)
		{
			rslt = PROTECT(R_tryEval(Rf_lang2(Rf_install("slotNames"), obj), R_GlobalEnv, &err));
			//rslt = PROTECT(R_tryEval(Rf_lang2(R_NamesSymbol, Rf_lang2(Rf_install("attributes"), obj)), R_GlobalEnv, &err));
			cc = '@';
		}
		else rslt = PROTECT(R_tryEval(Rf_lang2(R_NamesSymbol, obj), R_GlobalEnv, &err));
		if (rslt && TYPEOF(rslt) == STRSXP)
		{
			int i, len = Rf_length(rslt);
			for (int i = 0; i < len; i++)
			{
				std::string entry = token;
				entry += cc;
				entry += CHAR(STRING_ELT(rslt, i));

				DebugOut("%c: %s\n", cc, entry.c_str());
				vec.push_back(entry);
			}
			std::sort(vec.begin(), vec.end());
			vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
			ret = 1;
		}
		else
		{
			DebugOut("type: 0x%x\n", TYPEOF(rslt));
		}
		UNPROTECT(1);
	}
	UNPROTECT(1);

	return ret;
}

/**
 * get function call tip (if it's a function), using the print representation from "args"
 */
int getCallTip(std::string &callTip, const std::string &sym)
{
	int err, ret = 0;
	SEXP obj = resolveObject(sym);
	if (obj)
	{
		SEXP result = PROTECT(R_tryEval(Rf_lang2(Rf_install("capture.output"), Rf_lang2(Rf_install("args"), obj)), R_GlobalEnv, &err));
		if (result && TYPEOF(result) == STRSXP )
		{
			const char *c = CHAR(STRING_ELT(result, 0));
			if (!strncmp(c, "function ", 9))
			{
				callTip = Util::trim(&(c[9]));
				for (int i = 1; i < Rf_length(result) - 1; i++)
				{
					callTip += " ";
					callTip += Util::trim(CHAR(STRING_ELT(result, i)));
				}
				ret = 1;
			}
		}
		UNPROTECT(1);
	}
	UNPROTECT(1);
	return ret;
}

SVECTOR & getWordList(SVECTOR &wordList)
{
	int err;
	SEXP env = PROTECT(R_tryEvalSilent(Rf_lang2(Rf_install("get"), Rf_mkString(ENV_NAME)), R_GlobalEnv, &err));
	if (env)
	{
		SEXP rslt = PROTECT(R_tryEvalSilent(Rf_lang1(Rf_install("WordList")), env, &err));
		if (rslt)
		{
			int len = Rf_length(rslt);
			for (int i = 0; i < len; i++)
			{
				 wordList.push_back(std::string(CHAR(STRING_ELT(rslt, i))));
			}
		}
		UNPROTECT(1);
	}
	UNPROTECT(1);
	return wordList;

}

bool CheckExcelRef(LPXLOPER12 rslt, SEXP s)
{
	int type, err;
	int rc[4] = { 0, 0, 0, 0 };
	IDSHEET ids = 0;

	type = TYPEOF(s);

	if (TYPEOF(s) != S4SXP) return false;
	if (!Rf_inherits(s, "xlReference")) return false;

	rslt->xltype = xltypeSRef;

	SEXP ans = R_do_slot(s, Rf_mkString("R1"));
	if (ans)
	{
		type = TYPEOF(ans);
		if (type == INTSXP) rc[0] = (INTEGER(ans))[0] - 1;
		else if (type == REALSXP) rc[0] = (int)(REAL(ans))[0] - 1;
		else DebugOut("Unexpected type in check excel ref: %d\n", type);
	}

	ans = R_do_slot(s, Rf_mkString("C1"));
	if (ans)
	{
		type = TYPEOF(ans);
		if (type == INTSXP) rc[1] = (INTEGER(ans))[0] - 1;
		else if (type == REALSXP) rc[1] = (int)(REAL(ans))[0] - 1;
		else DebugOut("Unexpected type in check excel ref: %d\n", type);
	}

	ans = R_do_slot(s, Rf_mkString("R2"));
	if (ans)
	{
		type = TYPEOF(ans);
		if (type == INTSXP) rc[2] = (INTEGER(ans))[0]-1;
		else if (type == REALSXP) rc[2] = (int)(REAL(ans))[0]-1;
		else DebugOut("Unexpected type in check excel ref: %d\n", type);
	}

	ans = R_do_slot(s, Rf_mkString("C2"));
	if (ans)
	{
		type = TYPEOF(ans);
		if (type == INTSXP) rc[3] = (INTEGER(ans))[0] - 1;
		else if (type == REALSXP) rc[3] = (int)(REAL(ans))[0] - 1;
		else DebugOut("Unexpected type in check excel ref: %d\n", type);
	}

	ans = R_do_slot(s, Rf_mkString("SheetID"));
	if (ans)
	{
		type = TYPEOF(ans);
		if (type == INTSXP && Rf_length(ans) == 2)
		{
			int *p = INTEGER(ans);
			ids = p[0];
			ids <<= 32;
			ids |= p[1];
		}
		else DebugOut("Unexpected type in check excel ref: %d\n", type);
	}

	if (rc[2] < rc[0]) rc[2] = rc[0];
	if (rc[3] < rc[1]) rc[3] = rc[1];

	LPXLREF12 pref = 0;

	if (ids > 0)
	{
		// not using this structure as intended... be careful

		rslt->xltype = xltypeRef | xlbitDLLFree;
		rslt->val.mref.idSheet = ids;
		rslt->val.mref.lpmref = new XLMREF12[1];
		rslt->val.mref.lpmref[0].count = 1;
		pref = &(rslt->val.mref.lpmref[0].reftbl[0]);
	}
	else
	{
		rslt->xltype = xltypeSRef;
		pref = &(rslt->val.sref.ref);
		rslt->val.sref.count = (rc[2] - rc[0] + 1) * (rc[3] - rc[1] + 1);
	}

	if ( pref )
	{
		pref->rwFirst = rc[0];
		pref->colFirst = rc[1];
		pref->rwLast = rc[2];
		pref->colLast = rc[3];
	}

	return true;
}

void ParseResult(LPXLOPER12 rslt, SEXP ans)
{
	// there are a couple of different return semantics 
	// that we need to handle here.

	// simple types (scalars) are returned as arrays of 
	// a particular type. 

	// lists are the same thing, except they hold more 
	// than one value.  in this case, return values in a 
	// single column.

	// matrices are the same thing again, except they have
	// defined row and column counts.  

	// and finally vectors of vectors; these are arrays
	// of the generic type (VECSXP) of which each entry 
	// is itself an array.  this is the type used by
	// data frames, but it's also used by things which
	// are not data frames.

	// FOR NOW, we are not returning row names / column 
	// names for things which are not data frames.

	if (!ans)
	{
		rslt->xltype = xltypeErr;
		rslt->val.err = xlerrValue;
		return;
	}

	int len = Rf_length(ans);
	int type = TYPEOF(ans);

	// FIXME: there's starting to be a lot of repeated code here...

	if (Rf_isFrame(ans))
	{
		int err;
		int nc = len + 1; 
		SEXP s = VECTOR_ELT(ans, 0);
		int nr = Rf_length(s) + 1;

		// data frames always have row, column names

		rslt->xltype = xltypeMulti;
		rslt->val.array.rows = nr;
		rslt->val.array.columns = nc;
		rslt->val.array.lparray = new XLOPER12[nr*nc];

		// init or excel will show errors for ALL elements
		// include a "" for the top-left corner

		rslt->val.array.lparray[0].xltype = xltypeStr;
		rslt->val.array.lparray[0].val.str = new XCHAR[1];
		rslt->val.array.lparray[0].val.str[0] = 0;

		for (int i = 1; i < nr*nc; i++) rslt->val.array.lparray[i].xltype = xltypeMissing;
		
		// column (member) names

		SEXP cn = PROTECT(R_tryEval(Rf_lang2(R_NamesSymbol, ans), R_GlobalEnv, &err));
		if (cn)
		{
			len = Rf_length(cn);
			type = TYPEOF(cn);

			if (len != nc-1)
			{
				DebugOut("** Len != NC-1\n");
			}

			for (int c = 0; c < len; c++)
			{
				int idx = c + 1;
				switch (type)
				{
				case INTSXP:	//  13	  /* integer vectors */
					if (!ISNA((INTEGER(cn))[c]))
					{
						rslt->val.array.lparray[idx].xltype = xltypeInt;
						rslt->val.array.lparray[idx].val.w = (INTEGER(cn))[c];
					}
					break;
				case REALSXP:	//  14	  /* real variables */  
					rslt->val.array.lparray[idx].xltype = xltypeNum;
					rslt->val.array.lparray[idx].val.num = (REAL(cn))[c];
					break;
				case STRSXP:	//  16	  /* string vectors - legal? */ 
					STRSXP2XLOPER(&(rslt->val.array.lparray[idx]), STRING_ELT(cn, c));
					break;
				case CPLXSXP:	//	15	  /* complex variables */
					CPLXSXP2XLOPER(&(rslt->val.array.lparray[idx]), (COMPLEX(cn))[c]);
					break;
				default:
					DebugOut("** Unexpected type in data frame (col) names: %d\n", type);
					break;
				}
			}

		}
		UNPROTECT(1);

		// get row names, we'll stick them in column 0

		SEXP rn = PROTECT(R_tryEval(Rf_lang2( R_RowNamesSymbol, ans), R_GlobalEnv, &err));
		if (rn)
		{
			len = Rf_length(rn);
			type = TYPEOF(rn);

			if (len != nr-1)
			{
				DebugOut("** Len != NR-1\n");
			}
			for (int r = 0; r < len; r++)
			{
				int idx = (r+1) * nc + 0;
				switch (type)
				{
				case INTSXP:	//  13	  /* integer vectors */
					rslt->val.array.lparray[idx].xltype = xltypeInt;
					rslt->val.array.lparray[idx].val.w = (INTEGER(rn))[r];
					break;
				case STRSXP:	//  16	  /* string vectors - legal? */ 
					STRSXP2XLOPER(&(rslt->val.array.lparray[idx]), STRING_ELT(rn, r));
					break;
				case CPLXSXP:	//	15	  /* complex variables */
					CPLXSXP2XLOPER(&(rslt->val.array.lparray[idx]), (COMPLEX(rn))[r]);
					break;

				default:
					DebugOut( "** Unexpected type in data frame row names: %d\n", type);
					break;
				}
			}
		}
		UNPROTECT(1);

		for (int i = 0; i < nc - 1; i++) 
		{
			s = VECTOR_ELT(ans, i);
			type = TYPEOF(s);
			len = Rf_length(s);

			if (len != nr)
			{
				DebugOut("** Len != NR\n");
			}

			for (int r = 0; r < len; r++)
			{
				// offset for column names, row names
				int idx = (r+1) * nc + i + 1; 

				switch (type)
				{
				case INTSXP:	//  13	  /* integer vectors */
					rslt->val.array.lparray[idx].xltype = xltypeInt;
					rslt->val.array.lparray[idx].val.w = (INTEGER(s))[r];
					break;
				case REALSXP:	//  14	  /* real variables */  
					rslt->val.array.lparray[idx].xltype = xltypeNum;
					rslt->val.array.lparray[idx].val.num = (REAL(s))[r];
					break;
				case STRSXP:	//  16	  /* string vectors - legal? */ 
					STRSXP2XLOPER(&(rslt->val.array.lparray[idx]), STRING_ELT(s, r));
					break;
				case CPLXSXP:	//	15	  /* complex variables */
					CPLXSXP2XLOPER(&(rslt->val.array.lparray[idx]), (COMPLEX(s))[r]);
					break;

				default:
					DebugOut("** Unexpected type in data frame: %d\n", type);
					break;
				}
			}

		}

	}
	else if (type == VECSXP)
	{
		// this could be a vector-of-vectors, or it could 
		// be a matrix.  for a matrix, we can only handle
		// two dimensions.

		int nc = len, nr = 0;

		if (Rf_isMatrix(ans))
		{
			nr = Rf_nrows(ans);
			nc = Rf_ncols(ans);

			rslt->xltype = xltypeMulti;
			rslt->val.array.rows = nr;
			rslt->val.array.columns = nc;
			rslt->val.array.lparray = new XLOPER12[nr*nc];

			int idx = 0;
			for (int i = 0; i < nc; i++)
			{
				for (int j = 0; j < nr; j++)
				{
					SEXP v = VECTOR_ELT(ans, idx);
					type = TYPEOF(v);

					switch (type)
					{
					case INTSXP:	//  13	  /* integer vectors */

						// this is not working as expected...
						if (!ISNA((INTEGER(v))[0]) && (INTEGER(v))[0] != NA_INTEGER )
						{
							rslt->val.array.lparray[j*nc + i].xltype = xltypeInt;
							rslt->val.array.lparray[j*nc + i].val.w = (INTEGER(v))[0];
						}
						else
						{
							rslt->val.array.lparray[j*nc + i].xltype = xltypeStr;
							rslt->val.array.lparray[j*nc + i].val.str = new XCHAR[1];
							rslt->val.array.lparray[j*nc + i].val.str[0] = 0;
						}
						break;

					case REALSXP:	//  14	  /* real variables */  
						rslt->val.array.lparray[j*nc + i].xltype = xltypeNum;
						rslt->val.array.lparray[j*nc + i].val.num = (REAL(v))[0];
						break;

					case STRSXP:	//  16	  /* string vectors */ 
						STRSXP2XLOPER(&(rslt->val.array.lparray[j*nc + i]), STRING_ELT(v, 0));
						// ParseResult(&(rslt->val.array.lparray[j*nc + i]), STRING_ELT(ans, idx)); // this is lazy
						break;

					case CPLXSXP:	//	15	  /* complex variables */
						CPLXSXP2XLOPER(&(rslt->val.array.lparray[j*nc + i]), (COMPLEX(v))[0]);
						break;

					default:
						DebugOut("Unexpected type in list: %d\n", type);
						break;

					}

					idx++;
				}
			}

		}
		else // VofV?
		{
			// need to figure out the max length first

			for (int c = 0; c < nc; c++)
			{
				SEXP s = VECTOR_ELT(ans, c);
				int r = Rf_length(s);
				if (r > nr) nr = r;
			}

			rslt->xltype = xltypeMulti;
			rslt->val.array.rows = nr;
			rslt->val.array.columns = nc;
			rslt->val.array.lparray = new XLOPER12[nr*nc];

			// in the event there are any holes (sparse)

			for (int i = 0; i < nr*nc; i++) rslt->val.array.lparray[i].xltype = xltypeMissing;

			// the rest is just like a data frame but without headers

			for (int c = 0; c < nc; c++)
			{
				SEXP v = VECTOR_ELT(ans, c);
				int vlen = Rf_length(v);
				type = TYPEOF(v);
				for (int r = 0; r < vlen; r++)
				{
					int idx = r * nc + c;
					switch (type)
					{
					case INTSXP:	//  13	  /* integer vectors */
						rslt->val.array.lparray[idx].xltype = xltypeInt;
						rslt->val.array.lparray[idx].val.w = (INTEGER(v))[r];
						break;
					case REALSXP:	//  14	  /* real variables */  
						rslt->val.array.lparray[idx].xltype = xltypeNum;
						rslt->val.array.lparray[idx].val.num = (REAL(v))[r];
						break;
					case STRSXP:	//  16	  /* string vectors - legal? */ 
						STRSXP2XLOPER(&(rslt->val.array.lparray[idx]), STRING_ELT(v, r));
						break;
					case CPLXSXP:	//	15	  /* complex variables */
						CPLXSXP2XLOPER(&(rslt->val.array.lparray[idx]), (COMPLEX(v))[r]);
						break;
					default:
						DebugOut("** Unexpected type in vector/vector: %d\n", type);
						break;
					}
				}
			}
		}
	}
	else if (len > 1)
	{
		// for normal vector, use rows

		int nc = 1, nr = len;

		// for matrix, guaranteed to be 2 dimensions and we 
		// can use the convenient functions to get r/c

		if (Rf_isMatrix(ans))
		{
			nc = Rf_ncols(ans);
			nr = Rf_nrows(ans);
		}

		rslt->xltype = xltypeMulti;
		rslt->val.array.rows = nr;
		rslt->val.array.columns = nc;
		rslt->val.array.lparray = new XLOPER12[nr*nc];
		
		// FIXME: LOOP ON THE INSIDE (CHECK OPTIM)

		int idx = 0;
		for (int i = 0; i < nc; i++)
		{
			for (int j = 0; j < nr; j++)
			{
				switch (type)
				{
				case INTSXP:	//  13	  /* integer vectors */
					rslt->val.array.lparray[j*nc + i].xltype = xltypeInt;
					rslt->val.array.lparray[j*nc + i].val.w = (INTEGER(ans))[idx];
					break;

				case REALSXP:	//  14	  /* real variables */  

					if (ISNA( (REAL(ans))[idx] )) {
						rslt->val.array.lparray[j*nc + i].xltype = xltypeStr;
						rslt->val.array.lparray[j*nc + i].val.str = new XCHAR[1];
						rslt->val.array.lparray[j*nc + i].val.str[0] = 0;
					}
					else {
						rslt->val.array.lparray[j*nc + i].xltype = xltypeNum;
						rslt->val.array.lparray[j*nc + i].val.num = (REAL(ans))[idx];
					}
					break;

				case STRSXP:	//  16	  /* string vectors */ 
					STRSXP2XLOPER(&(rslt->val.array.lparray[j*nc + i]), STRING_ELT(ans, idx));
					// ParseResult(&(rslt->val.array.lparray[j*nc + i]), STRING_ELT(ans, idx)); // this is lazy
					break;

				case CPLXSXP:	//	15	  /* complex variables */
					CPLXSXP2XLOPER(&(rslt->val.array.lparray[j*nc + i]), (COMPLEX(ans))[idx]);
					break;

				default:
					DebugOut("Unexpected type in list: %d\n", type);
					break;

				}

				idx++;
			}
		}
	}
	else
	{
		// single value

		if (isLogical(ans)) 
		{
			// it seems wasteful having this first, 
			// but I think one of the other types is 
			// intercepting it.  figure out how to do
			// tighter checks and then move this down
			// so real->integer->string->logical->NA->?

			// this is weird, but NA seems to be a logical
			// with a particular value.

			int lgl = Rf_asLogical(ans);
			if (lgl == NA_LOGICAL)
			{
				rslt->xltype = xltypeMissing;
			}
			else
			{
				rslt->xltype = xltypeBool;
				rslt->val.xbool = lgl ? true : false;
			}
		}
		else if (Rf_isComplex(ans))
		{
			CPLXSXP2XLOPER(rslt, *(COMPLEX(ans)));
		}
		else if (Rf_isInteger(ans))
		{
			rslt->xltype = xltypeInt;
			rslt->val.w = Rf_asInteger(ans);
		}
		else if (isReal(ans) || Rf_isNumber(ans))
		{
			rslt->xltype = xltypeNum;
			rslt->val.num = Rf_asReal(ans);
		}
		else if (isString(ans))
		{
			STRSXP2XLOPER(rslt, ans);
		}
	}
}

LPXLOPER12 BERT_RExec(LPXLOPER12 code)
{
	// not thread safe!
	static XLOPER12 result;

	ParseStatus status;
	SEXP cmdexpr = R_NilValue;
	int errorOccurred;

	resetXlOper(&result);

	char *sz = 0;
	if (code->xltype == xltypeStr)
	{
		// FIXME: this seems fragile, as it might try to 
		// allocate a really large contiguous block.  better
		// to walk through and find lines?

		int len = WideCharToMultiByte(CP_UTF8, 0, &(code->val.str[1]), code->val.str[0], 0, 0, 0, 0);
		sz = new char[len + 1];
		WideCharToMultiByte(CP_UTF8, 0, &(code->val.str[1]), code->val.str[0], sz, len, 0, 0);
		sz[len] = 0;

	}

	result.xltype = xltypeErr;
	result.val.err = xlerrValue;

	SEXP ans;

	if (sz) {

		// if there are newlines in the string, we need to split it and use the vector method
		std::string scode(sz);
		if (scode.find_first_of("\r\n") != std::string::npos)
		{
			SVECTOR vec;
			Util::split(scode, '\n', 1, vec, true);
			ans = ExecR(vec, &errorOccurred, &status);
		}
		else
		{
			ans = ExecR(sz, &errorOccurred, &status);
		}

		// if (sz) 
		ParseResult(&result, ans);
		delete[] sz;

	}
	else {

		result.xltype = xltypeErr;
		result.val.err = xlerrValue;

	}
	
	return &result;
}


/**
 * this version of the function checks the values first and, if
 * they are all numeric, returns a Real matrix.  this should 
 * simplify function calls on the R side, but check how expensive
 * this is.
 *
 * the function takes offsets so we can use it inside the
 * data frame version of this coercion (wasteful?)
 */
SEXP Multi2Matrix(LPXLOPER12 px, int rowOffset, int colOffset)
{
	int rows = px->val.array.rows - rowOffset;
	int cols = px->val.array.columns - colOffset;

	// pass 1: check.  for now we only accept types num
	// and integer, although in theory booleans and nils
	// could be coerced to numbers as well.

	// FIXME: the check for Integers may be a waste, as this
	// type seems to be pretty rare these days.

	int ptr, idx = 0;
	bool numeric = true;

	for (int i = 0; numeric && i < cols; i++)
	{
		for (int j = 0; numeric && j < rows; j++)
		{
			idx = (j + rowOffset) * cols + i + colOffset;
			numeric = numeric &&
				(px->val.array.lparray[idx].xltype == xltypeNum
				|| px->val.array.lparray[idx].xltype == xltypeInt);
		}
	}

	idx = 0;
	ptr = 0;

	if (numeric)
	{
		SEXP s = Rf_allocMatrix(REALSXP, rows, cols);
		for (int i = 0; i < cols; i++)
		{
			double *mat = REAL(s);
			for (int j = 0; j < rows; j++)
			{
				ptr = (j+rowOffset)*cols + i + colOffset;
				if (px->val.array.lparray[ptr].xltype == xltypeNum) mat[idx] = px->val.array.lparray[ptr].val.num;
				else mat[idx] = px->val.array.lparray[ptr].val.w;
				idx++; 
			}
		}
		return s;
	}
	else
	{
		SEXP s = Rf_allocMatrix(VECSXP, rows, cols);
		for (int i = 0; i < cols; i++)
		{
			for (int j = 0; j < rows; j++)
			{
				ptr = (j+rowOffset) * cols + i + colOffset;
				SET_VECTOR_ELT(s, idx, XLOPER2SEXP(&(px->val.array.lparray[ptr]), 0));
				idx++; // not macro friendly with opt
			}
		}
		return s;
	}

}

/**
 * convert a (rectangular) range to a matrix.
 * perhaps we should convert 1-width ranges
 * to vectors... probably not a big deal.
 */
SEXP Multi2SEXP(LPXLOPER12 px)
{
	int rows = px->val.array.rows;
	int cols = px->val.array.columns;

	// fixme: want to protect this?
	SEXP s = Rf_allocMatrix(VECSXP, rows, cols);

	// so it looks like a matrix is just a list (same as 
	// Excel, as it happens); but the order is inverse of Excel.

	// TODO: if there are no strings in there, it might
	// be useful to use a Real matrix instead.

	int idx = 0;
	for ( int i = 0; i < cols; i++)
	{
		for (int j = 0; j < rows; j++)
		{
			SET_VECTOR_ELT(s, idx, XLOPER2SEXP(&(px->val.array.lparray[j*cols+i]), 0));
			idx++; // not macro friendly with opt
		}
	}

	return s;
}

/**
 * utility method for converting string (in regex submatch) to 
 * real, and allowing for some shorthand values
 */
__inline double number(const std::string &s)
{
	char *c;
	double d = 0;
	if (!s.compare("-")) return -1;
	else if (!s.compare("") || !s.compare("+")) return 1;
	return strtod(s.c_str(), &c);
}

/**
 * convert an arbitrary excel value to a SEXP
 */
SEXP XLOPER2SEXP( LPXLOPER12 px, int depth, bool missingArguments )
{
	XLOPER12 xlArg;
	std::string str;
	SEXP result;

	// might be faster to have 1 regex with an |.  However,
	// given how stl regex capture works, that might require
	// changing the conditionals

	static std::regex cpx1("^([-\\+]?\\d*(\\.\\d*)?([eE][-\\+]?\\d+)?)[ij]$");
	static std::regex cpx2("^([-\\+]?\\d*(\\.\\d*)?([eE][-\\+]?\\d+)?)([-\\+]\\d*(\\.\\d*)?([eE][-\\+]?\\d+)?)[ij]$");

	std::smatch m;
	
	if (depth >= 2)
	{
		DebugOut("WARNING: deep recursion\n");
		return R_NilValue;
	}

	switch (px->xltype)
	{
	case xltypeMulti:
		return Multi2Matrix(px, 0, 0);
		break;

	case xltypeRef:
	case xltypeSRef:
		Excel12(xlCoerce, &xlArg, 1, px);
		result = XLOPER2SEXP(&xlArg, depth++, missingArguments);
		Excel12(xlFree, 0, 1, (LPXLOPER12)&xlArg);
		return result;
		break;

	case xltypeStr:
		NarrowString(str, px);

#ifdef __USE_COMPLEX

		// one issue with this is that it fails on non-complex
		// numbers.  that's an issue because Excel creates complex
		// numbers as strings, even if i=0; and in that case, it 
		// omits the imaginary component BUT it's still a string.

		// that means we are not catching it in these regexes.

		// OTOH, the user might be trying to pass strings of numbers,
		// which we probably don't want to mess with.  not sure
		// how to handle this... 

		// TESTING complex support here via regex.

		if (std::regex_search(str, m, cpx1))
		{
			Rcomplex rc = { 0, number(m[1].str()) };
			return Rf_ScalarComplex(rc);
		}
		else if (std::regex_search(str, m, cpx2))
		{
			Rcomplex rc = { number(m[1].str()), number(m[4].str()) };
			return Rf_ScalarComplex(rc);
		}

		// TODO: perf
		// FIXME: supposedly boost regex is much faster than stl regex.

#endif // #ifdef __USE_COMPLEX

		return Rf_mkString(str.c_str());
		break;

	case xltypeBool:
		return Rf_ScalarLogical(px->val.xbool ? -1 : 0);
		break;

	case xltypeNum:
		return Rf_ScalarReal(px->val.num);
		break;

	case xltypeInt:
		return Rf_ScalarInteger(px->val.w);
		break;

	case xltypeNil:
	default:

		if (missingArguments) return R_MissingArg;

		// changing to NA (from NULL).  not sure about this 
		// either way, but I *think* NA is more appropriate.

		// TODO/FIXME: check?

		//return R_NilValue;
		return Rf_ScalarInteger(NA_LOGICAL);

		break;
	}

}

void RExecVector(std::vector<std::string> &vec, int *err, PARSE_STATUS_2 *status, bool printResult)
{
	ParseStatus ps;

	g_buffering = true;

	SEXP rslt = PROTECT(ExecR(vec, err, &ps, true));

	if (status)
	{
		switch (ps)
		{
		case PARSE_OK: *status = PARSE2_OK; break;
		case PARSE_INCOMPLETE: *status = PARSE2_INCOMPLETE; break;
		case PARSE_ERROR: *status = PARSE2_ERROR; break;
		case PARSE_EOF: *status = PARSE2_EOF; break;
		default:
		case PARSE_NULL:
			*status = PARSE2_NULL; break;
		}
	}

	if (ps == PARSE_OK && printResult && rslt)
	{
		int checkLen = Rf_length(rslt);

		SEXP elt = VECTOR_ELT(rslt, 1);
		int *pVisible = LOGICAL(elt);

		SEXP v0 = VECTOR_ELT(rslt, 0);
		int vt = TYPEOF(v0);

		if (*pVisible ) Rf_PrintValue(VECTOR_ELT(rslt,0));
	}

	UNPROTECT(1);

	g_buffering = false;
	flush_log();
}

bool RExec2(LPXLOPER12 rslt, std::string &funcname, std::vector< LPXLOPER12 > &args)
{
	SEXP sargs;
	SEXP lns, ans = 0;
	int i, errorOccurred;
	
	resetXlOper(rslt);

	::WaitForSingleObject(muxExecR, INFINITE);

	PROTECT(sargs = Rf_allocVector(VECSXP, args.size()));
	for (i = 0; i < args.size(); i++)
	{
		SET_VECTOR_ELT(sargs, i, XLOPER2SEXP(args[i], 0, true));
	}

	SEXP envir = g_Environment ? g_Environment : R_GlobalEnv;

	SVECTOR elts;
	Util::split(funcname, '$', 1, elts);
	if (elts.size() > 1) {
		
		envir = R_tryEvalSilent(Rf_lang2(Rf_install("get"), Rf_mkString(elts[0].c_str())), R_GlobalEnv, &errorOccurred);
		funcname = elts[1];
	}

	lns = PROTECT(Rf_lang5(Rf_install("do.call"), Rf_mkString(funcname.c_str()), sargs, R_MissingArg, envir));
	PROTECT(ans = R_tryEval(lns, R_GlobalEnv, &errorOccurred));

	ParseResult(rslt, ans);

	UNPROTECT(3);
	::ReleaseMutex(muxExecR);

	return true;
}

SEXP BERTWatchFiles(SEXP list) {

	int len = Rf_length(list);
	std::vector< std::string > files;

	// must be list of strings
	if (len > 0 && TYPEOF(list) != STRSXP) {
		R_WriteConsole("Invalid parameter\n", 0);
		return Rf_ScalarLogical(0);
	}
	
	for (int i = 0; i < len; i++) {
		std::string path = CHAR(STRING_ELT(list, i));
		files.push_back(path);
	}
	
	int rslt = watchFiles(files);
	return Rf_ScalarLogical(rslt == 0);

}

SEXP ExcelCall(SEXP cmd, SEXP data)
{
	int dlen = Rf_length(data);
	int fn;
	int type = TYPEOF(cmd);

	XLOPER12 xlRslt;
	LPXLOPER12 *xdata = 0;

	if (type != INTSXP && type != REALSXP)
	{
		Rf_error("Error: invalid command constant\n");
		return R_NilValue;
	}


	if (type == REALSXP) fn = (int)(*(REAL(cmd)));
	else fn = *(INTEGER(cmd));

	xdata = new LPXLOPER12[dlen];

	type = TYPEOF(data);

	for (int i = 0; i < dlen; i++)
	{
		xdata[i] = new XLOPER12;
		switch (type)
		{
		case VECSXP:
			{
				SEXP tmp = VECTOR_ELT(data, i);
				if (!CheckExcelRef(xdata[i], tmp))
					ParseResult(xdata[i], tmp);
			}
			break;
		case STRSXP:
			ParseResult(xdata[i], STRING_ELT(data, i));
			break;
		case INTSXP:
			xdata[i]->xltype = xltypeInt;
			xdata[i]->val.w = (INTEGER(data))[i];
			break;
		case REALSXP:
			xdata[i]->xltype = xltypeNum;
			xdata[i]->val.num = (REAL(data))[i];
			break;
		case LGLSXP:
			xdata[i]->xltype = xltypeBool;
			xdata[i]->val.xbool = (INTEGER(data))[i] ? TRUE : FALSE;
			break;
		}

		DebugOut("parm %d type 0x%x\n", i, xdata[i]->xltype);
		if (xdata[i]->xltype & xltypeStr)
		{
			DebugOut("\tptr: %x\n", xdata[i]->val.str);
		}
		if (xdata[i]->xltype & xltypeRef)
		{
			DebugOut("\tptr: %x\n", xdata[i]->val.mref.lpmref);
		}
	}

	SEXP rv = R_NilValue;
	int rs = Excel12v(fn, &xlRslt, dlen, xdata);

	if (rs)
	{
		Rf_error("Excel call failed (%d)", rs);
	}
	else
	{
		DebugOut("Return type 0x%x\n", xlRslt.xltype);

		// in this call we want refs, so return as the class type

		if (xlRslt.xltype == xltypeRef || xlRslt.xltype == xltypeSRef)
		{
			int err;
			int rc[4] = { 0, 0, 0, 0 };
			IDSHEET ids = 0;

			rv = R_do_new_object(R_getClassDef("xlReference"));

			//rv = R_tryEval(Rf_lang1(Rf_install("new.env")), R_GlobalEnv, &err);
			//Rf_setAttrib(rv, Rf_mkString("class"), Rf_mkString("xlRefClass"));

			LPXLREF12 pref = 0;

			if (xlRslt.xltype == xltypeRef)
			{
				ids = xlRslt.val.mref.idSheet;
				if (xlRslt.val.mref.lpmref && xlRslt.val.mref.lpmref->count > 0) pref = &(xlRslt.val.mref.lpmref->reftbl[0]);
			}
			else // sref
			{
				pref = &(xlRslt.val.sref.ref);
			}

			if (pref)
			{
				rc[0] = pref->rwFirst + 1;
				rc[1] = pref->colFirst + 1;
				rc[2] = pref->rwLast + 1;
				rc[3] = pref->colLast + 1;
			}

			R_do_slot_assign(rv, Rf_mkString("R1"), Rf_ScalarInteger(rc[0]));
			R_do_slot_assign(rv, Rf_mkString("C1"), Rf_ScalarInteger(rc[1]));
			R_do_slot_assign(rv, Rf_mkString("R2"), Rf_ScalarInteger(rc[2]));
			R_do_slot_assign(rv, Rf_mkString("C2"), Rf_ScalarInteger(rc[3]));

			SEXP idv;
			if (sizeof(IDSHEET) > 4)
			{
				idv = Rf_list2( ScalarInteger((INT32)(ids >> 32)), ScalarInteger((INT32)(ids & 0xffffffff)));
			}
			else
			{
				idv = Rf_list2(ScalarInteger(0), ScalarInteger(ids));
			}

			R_do_slot_assign(rv, Rf_mkString("SheetID"), idv);

		}
		else rv = XLOPER2SEXP(&xlRslt);
	}

	// ...

	// free up any allocated 

	for (int i = 0; i < dlen; i++)
	{
		if ((xdata[i]->xltype & xltypeStr ) && xdata[i]->val.str) delete xdata[i]->val.str;
		else if ((xdata[i]->xltype & xltypeMulti ) && xdata[i]->val.array.lparray) delete xdata[i]->val.array.lparray;
		else if (( xdata[i]->xltype & xltypeRef ) && xdata[i]->val.mref.lpmref) delete xdata[i]->val.mref.lpmref;
		else
		{
			DebugOut("Not deleting type 0x%x\n", xdata[i]->xltype);
		}
		delete xdata[i];
	}
	
	Excel12(xlFree, 0, 1, &xlRslt);

	return rv;

}

SEXP BERT_COM_Callback(SEXP name, SEXP calltype, SEXP p, SEXP args) {

	int ctype = 0;
	if (Rf_length(calltype) > 0)
	{
		if (isReal(calltype)) ctype = (REAL(calltype))[0];
		else if (isInteger(calltype)) ctype = (INTEGER(calltype))[0];
	}

	switch (ctype) {
	case MRFLAG_METHOD:
		return COMFunc(name, p, args);
	case MRFLAG_PROPGET:
		return COMPropGet(name, p, args);
	case MRFLAG_PROPPUT:
		return COMPropPut(name, p, args);
	}

	return R_NilValue;
}

void AddUserButton(SEXP args) {

	std::string strLabel = "";
	std::string strFunc = "";
	std::string strImg = "";

	int len = Rf_length(args);

	if (len > 1) {
		strLabel = CHAR(STRING_ELT(args, 0));
		strFunc = CHAR(STRING_ELT(args, 1));
		if (len > 2) strImg = CHAR(STRING_ELT(args, 2));
		if( strLabel.length() && strFunc.length()) RibbonAddUserButton(strLabel, strFunc, strImg);
	}

}

/**
 * callback dispatch function (calling from R)
 */
SEXP BERT_Callback(SEXP cmd, SEXP data, SEXP data2)
{
	int command = 0;
	if (Rf_length(cmd) > 0)
	{
		if (isReal(cmd)) command = (REAL(cmd))[0];
		else if (isInteger(cmd)) command = (INTEGER(cmd))[0];
	}
	switch (command)
	{
	case CC_ADD_USER_BUTTON:
		AddUserButton(data);
		break;

	case CC_CLEAR_USER_BUTTONS:
		RibbonClearUserButtons();
		break;

	case CC_WATCHFILES:
		return BERTWatchFiles(data);

	case CC_EXCEL:
		return ExcelCall(data, data2);

	case CC_CLEAR:
		ClearConsole();
		break;

	case CC_CLOSECONSOLE:
		CloseConsoleAsync();
		break;

	case CC_RELOAD:
		BERT_Reload();
		break;
	}

	return R_NilValue;
}
