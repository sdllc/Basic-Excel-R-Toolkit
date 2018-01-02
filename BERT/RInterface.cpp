/*
 * Basic Excel R Toolkit (BERT)
 * Copyright (C) 2014-2017 Structured Data, LLC
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
#include "Shlwapi.h"

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
#include "RemoteShell.h"
#include "BERTJSON.h"

#include "BERTGDI.h"

extern void bert_device_init(void *name, void *p);

#ifndef MIN
#define MIN(a,b) ( a < b ? a : b )
#endif 

//FDVECTOR RFunctions;
FD2VECTOR RFunctions;

extern HMODULE ghModule;
SEXP g_Environment = 0;

SEXP ExecR(const char *code, int *err = 0, ParseStatus *pStatus = 0);
SEXP ExecR(std::string &str, int *err = 0, ParseStatus *pStatus = 0);
SEXP ExecR(std::vector< std::string > &vec, int *err = 0, ParseStatus *pStatus = 0, bool withVisible = false);

SEXP XLOPER2SEXP(LPXLOPER12 px, int depth = 0, bool missingArguments = false);
SEXP syncResponse;

std::string dllpath;

bool g_buffering = false;
std::vector< std::string > logBuffer;
std::vector< std::string > cmdBuffer;

HANDLE muxLog;
HANDLE muxExecR;

// this doesn't need to be a lock, just a flag
bool loadingStartupFile = false;

XCHAR emptyStr[] = L"\0\0";

SEXP BERTExternalCallback(int cmd, void* data = 0, void* data2 = 0);
SEXP BERTExternalCOMCallback(SEXP name, SEXP calltype, SEXP p, SEXP args);
SEXP ConsoleHistory(SEXP args);


//
// for whatever reason these are not exposed in the embedded headers.
//
extern "C" {

extern void R_RestoreGlobalEnvFromFile(const char *, Rboolean);
extern void R_SaveGlobalEnvToFile(const char *);

}

extern void RibbonClearUserButtons();
extern void RibbonAddUserButton(std::string &strLabel, std::string &strFunc, std::string &strImgMso);


void setSyncResponse(int rslt) {
	syncResponse = Rf_ScalarInteger(rslt);
}

void setSyncResponse(double rslt) {
	syncResponse = Rf_ScalarReal(rslt);
}

///

int R_ReadConsole(const char *prompt, char *buf, int len, int addtohistory)
{
	fputs(prompt, stdout);
	fflush(stdout);
	if (fgets(buf, len, stdin)) {
		return 1;
	}
	else {
		return 0;
	}
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
	DebugOut("busy? %d\n", which);
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

void MapFunction( const char *name, const char *r_name, SEXP func, SEXP env, SEXP category = NULL ){

	int err = 0;

	// for now, at least, do not use the function pointer.
	// we may come back to that.  for the time being use
	// names.

	RFuncDesc2 funcdesc(0, env, r_name);
	funcdesc.pairs.push_back(SPAIR(name, ""));

	STRING2STRINGMAP argument_descriptions;

	// category attribute: defines the category in the Excel insert function box

	SEXP categoryAttr = PROTECT(getAttrib(func, Rf_install("category")));
	if (categoryAttr) {
		int type = TYPEOF(categoryAttr);
		int len = Rf_length(categoryAttr);
		if (type == 16 && len > 0) {
			funcdesc.function_category = CHAR(STRING_ELT(categoryAttr, 0));
		}
		UNPROTECT(1);
	}

	// alternative (for package mapping): explicit category.  the attribute controls, 
	// this is a fallback.

	if (category && !funcdesc.function_category.length()) {
		int type = TYPEOF(category);
		int len = Rf_length(category);
		if (type == 16 && len > 0) {
			funcdesc.function_category = CHAR(STRING_ELT(category, 0));
		}
	}

	// description attrribute: function (and parameter) documentation

	SEXP descAttr = PROTECT(getAttrib( func, Rf_install("description")));
	if (descAttr) {

		int type = TYPEOF(descAttr);
		int len = Rf_length(descAttr);

		if ( type == 16 && len > 0 ) {
			funcdesc.function_description = CHAR(STRING_ELT(descAttr, 0));
		}
		else if (type == 19 && len > 0) {
			SEXP names = PROTECT(R_tryEval(Rf_lang2(R_NamesSymbol, descAttr), R_GlobalEnv, &err));
			if (names)
			{
				int nameslen = Rf_length(names);
				if (TYPEOF(names) == STRSXP && nameslen > 0 ) {
					for (int i = 0; i < nameslen; i++) {
						SEXP sval = PROTECT( VECTOR_ELT(descAttr, i));
						if (sval) {
							if (TYPEOF(sval) == STRSXP) {
								std::string key = CHAR(STRING_ELT(names, i));
								if (key.length() == 0) funcdesc.function_description = CHAR(STRING_ELT(sval, 0));
								else argument_descriptions[key] = CHAR(STRING_ELT(sval, 0));

								DebugOut("Key %s\n", key.c_str());
							}
							else DebugOut("Unexpected type %d\n", TYPEOF(sval));
							UNPROTECT(1);
						}
					}
				}
				UNPROTECT(1);
			}
		}

		UNPROTECT(1);
	}

	SEXP args = PROTECT(R_tryEval(Rf_lang2(Rf_install("formals"), func), R_GlobalEnv, &err));
	if (args && isList(args))
	{
		SEXP asvec = PROTECT(Rf_PairToVectorList(args));
		SEXP names = PROTECT(R_tryEval(Rf_lang2(R_NamesSymbol, args), R_GlobalEnv, &err));
		if (names)
		{
			int plen = Rf_length(names);
			for (int j = 0; j < plen; j++)
			{
				std::string dflt = "";
				std::string arg = CHAR(STRING_ELT(names, j));

				STRING2STRINGMAP::iterator iter = argument_descriptions.find(arg);

				if (iter != argument_descriptions.end()) {
					dflt = iter->second;
				}
				else {
					SEXP vecelt = VECTOR_ELT(asvec, j);
					int type = TYPEOF(vecelt);

					if (type != 1)
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
				}

				funcdesc.pairs.push_back(SPAIR(arg, dflt));
			}
		}
		UNPROTECT(2);
	}

	UNPROTECT(1);
	RFunctions.push_back(funcdesc);
	
}

void MapEnvironment(SEXP envir, const char *prefix /* placeholder */, const char *category /* placeholder */) {

	int err = 0;

	int i, j, len = 0;
	SEXP lst = PROTECT(R_tryEval(Rf_lang1(Rf_install("lsf.str")), envir, &err));
	if (lst) len = Rf_length(lst);

	for (i = 0; i < len; i++)
	{
		const char *name = CHAR(STRING_ELT(lst, i));
		SEXP rname = Rf_mkString(name);
		SEXP func = R_tryEval(Rf_lang2(Rf_install("get"), rname), envir, &err);
		if (func) MapFunction(name, name, func, envir);
		else {
			DebugOut("Function failed: %s, err %d\n", CHAR(STRING_ELT(lst, i)), err);
		}
	}

	UNPROTECT(1);

}

void ClearFunctions() {

	// this probably doesn't need to be locked, it used to be 
	// part of "MapFunctions".  We're splitting that into a couple
	// of parts to support express function assignment.

	::WaitForSingleObject(muxExecR, INFINITE);
	RFunctions.clear();
	::ReleaseMutex(muxExecR);

}

void MapFunctions()
{
	SEXP s = 0;
	int err;
	char env[MAX_PATH];

	::WaitForSingleObject(muxExecR, INFINITE);

// moved //	RFunctions.clear();

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
	MapEnvironment(g_Environment ? g_Environment : R_GlobalEnv, 0, 0);

	// mapped functions: these are explicitly registered

	SEXP bertenv = PROTECT(R_tryEval(Rf_lang2(Rf_install("get"), Rf_mkString("BERT")), R_GlobalEnv, &err));
	if (bertenv) {
		SEXP mapped = PROTECT(R_tryEval(Rf_lang2(Rf_install("get"), Rf_mkString(".function.map")), bertenv, &err));
		if (mapped) {
			SEXP list = PROTECT(R_tryEval(Rf_lang2(Rf_install("ls"), mapped), R_GlobalEnv, &err));
			if (list) {
				int len = Rf_length(list);
				for (int i = 0; i < len; i++ ){
					const char *name = CHAR(STRING_ELT(list, i));
					SEXP elt = PROTECT(R_tryEval(Rf_lang2(Rf_install("get"), Rf_mkString(name)), mapped, &err));
					if (elt) {
						int type = TYPEOF(elt);
						if (type == VECSXP) {

							// see note in MapFunction 

							SEXP rname = VECTOR_ELT(elt, 1);
							if (TYPEOF(rname) == STRSXP) {
								SEXP env = VECTOR_ELT(elt, 2);
								SEXP expr = R_tryEval(Rf_lang2(Rf_install("get"), rname), env, &err);
								SEXP category = VECTOR_ELT(elt, 3);
								if (expr) MapFunction(name, CHAR(STRING_ELT(rname,0)), expr, env, category);
							}

						}
						UNPROTECT(1);
					}
				}
				UNPROTECT(1);
			}
			UNPROTECT(1);
		}
		UNPROTECT(1);
	}


	::ReleaseMutex(muxExecR);
	
}

/**
 * remap functions and load them into excel, unless code is running --
 * in that case, assume that we're being source()d
 */
SEXP RemapFunctions() {

	if (loadingStartupFile) {
		return Rf_ScalarLogical(0);
	}
	ClearFunctions();
	MapFunctions();
	RegisterAddinFunctions();
	return Rf_ScalarLogical(1);

}

std::string getFunctionsDir() {

	char functionsdir[MAX_PATH];
	std::string path = "";

	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, functionsdir, MAX_PATH, REGISTRY_KEY, REGISTRY_VALUE_FUNCTIONS_DIR))
		strcpy_s(functionsdir, MAX_PATH, DEFAULT_R_FUNCTIONS_DIR);

	if (PathIsRelativeA(functionsdir)) {
		char RUser[MAX_PATH];
		if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, RUser, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_USER))
			ExpandEnvironmentStringsA(DEFAULT_R_USER, RUser, MAX_PATH);
		path = RUser;
		if (RUser[strlen(RUser) - 1] != '\\') path += "\\";
	}

	path += functionsdir;
	if (functionsdir[strlen(functionsdir) - 1] != '\\') path += "\\";
	
	return path;
}

void LoadStartupFile()
{
	// if there is a startup script, load that now

	char RUser[MAX_PATH];
	char path[MAX_PATH];
	char buffer[MAX_PATH];

	int errcount = 0;

	loadingStartupFile = true;
	rshell_block(true);

	DWORD dwStatus = ::WaitForSingleObject(muxExecR, INFINITE);

	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, RUser, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_USER))
		ExpandEnvironmentStringsA( DEFAULT_R_USER, RUser, MAX_PATH );

	// new version -- functions directory

	std::string absoluteFunctionsPath = getFunctionsDir();

	///

	if (absoluteFunctionsPath.length())
	{
		time_t t;
		struct tm timeinfo;
		int errorOccurred;

		WIN32_FIND_DATAA data;
		HANDLE hFind = INVALID_HANDLE_VALUE;
		DWORD dwError = 0;

		// using named args so we can set the chdir flag

		SEXP namedargs;
		const char *names[] = { "file", "chdir", "" };
				
		std::string spath = absoluteFunctionsPath;
		spath += "*";

		hFind = FindFirstFileA(spath.c_str(), &data);

		if (hFind && hFind != INVALID_HANDLE_VALUE) {

			do {
				DebugOut("ListFiles: %s (%X)\n", data.cFileName, data.dwFileAttributes);
				if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )) {

					char* extension = PathFindExtensionA((const char*)data.cFileName);
					if (!Util::icasecompare(extension, ".r")
						|| !Util::icasecompare(extension, ".rsrc")
						|| !Util::icasecompare(extension, ".rscript")) {

						DebugOut("File %s, ext %s\n", data.cFileName, extension);

						std::string filepath = absoluteFunctionsPath;
						filepath += data.cFileName;

						PROTECT(namedargs = mkNamed(VECSXP, names));
						SET_VECTOR_ELT(namedargs, 0, Rf_mkString(filepath.c_str()));
						SET_VECTOR_ELT(namedargs, 1, Rf_ScalarLogical(1));
						R_tryEval(Rf_lang3(Rf_install("do.call"), Rf_mkString("source"), namedargs), R_GlobalEnv, &errorOccurred);
						UNPROTECT(1);

						if (!errorOccurred)
						{
							sprintf_s(buffer, "Sourced file OK: %s\n", data.cFileName );
							logMessage(buffer, 0, 1);
						}
						else
						{
							sprintf_s(buffer, "Error sourcing file: %s\n", data.cFileName);
							logMessage(buffer, 0, 1);
							errcount++;
						}

					}
				}
			} while (FindNextFileA(hFind, &data));
			
			FindClose(hFind);

		}
	}
	
	// functions.R (or whatever the user has changed it to) -- this is the old version.
	// we're doing this second, to make sure what's in here overwrites anything we installed
	// from the directory.

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
		if (PathFileExistsA(path)) {

			PROTECT(namedargs = mkNamed(VECSXP, names));
			SET_VECTOR_ELT(namedargs, 0, Rf_mkString(path));
			SET_VECTOR_ELT(namedargs, 1, Rf_ScalarLogical(1));
			R_tryEval(Rf_lang3(Rf_install("do.call"), Rf_mkString("source"), namedargs), R_GlobalEnv, &errorOccurred);
			UNPROTECT(1);

			time(&t);
			localtime_s(&timeinfo, &t);

			if (!errorOccurred)
			{
				strftime(buffer, MAX_PATH, "Read startup file OK @ %c\n", &timeinfo);
				logMessage(buffer, 0, 1);
				logMessage(FUNCTIONS_R_DEPRECATED, 0, 1);
			}
			else
			{
				strftime(buffer, MAX_PATH, "Error reading startup file @ %c\n", &timeinfo);
				logMessage(buffer, 0, 1);
				errcount++;
			}
		}
	}

	ExcelStatus( errcount ? "BERT: Error reading startup file; check console" : 0);

	::ReleaseMutex(muxExecR);
	rshell_block(false);
	loadingStartupFile = false;

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

SEXP constructBERTVersion(){
	
	const char *keys[] = {
		"build.date",
		"major",
		"minor",
		"patch",
		"version.string",
		"" };

	int i, j, k, index = 1;
	char buffer[256];
	char n[256] = { 0 };

	SEXP version = PROTECT(mkNamed(VECSXP, keys));

	sprintf_s(buffer, "BERT Version ");
	j = strlen(buffer);

	for (i = k = 0; j < 256 && i<= wcslen(BERT_VERSION); i++, j++, k++) {
		buffer[j] = BERT_VERSION[i] & 0xff;
		if (!buffer[j] || buffer[j] == '.') {
			n[k] = 0;
			SET_VECTOR_ELT(version, index++, Rf_ScalarInteger(atoi(n)));
			k = -1;
		}
		else n[k] = buffer[j];
	}
	SET_VECTOR_ELT(version, 4, Rf_mkString(buffer));

//	sprintf_s(buffer, "%s %s", __DATE__, __TIME__);
//	SET_VECTOR_ELT(version, 0, Rf_mkString(buffer));
	SET_VECTOR_ELT(version, 0, Rf_mkString(__TIMESTAMP__));

	return version;
}

/**
 * utility method for longer functions
 */
void execRString(std::string &str) {

	std::stringstream ss(str);
	std::string line;

	int err;
	ParseStatus status;
	std::vector < std::string > vec;

	while (std::getline(ss, line)) {
		line = Util::trim(line);
		if (line.length() > 0) vec.push_back(line);
	}
	if (vec.size() > 0)
	{
		ExecR(vec, &err, &status);
	}

}

void appendLibraryPath(const char *lib) {
	
	// set lib paths to R_LIBS_USER, then our path, then (why not) R_LIBS
	// NOTE: this might not be a good idea, if we want to ensure separation
	// between BERT and other R installs.  maybe optional?

	// OTOH that's the normal behavior, we were just breaking it.  so it 
	// should be OK.  it's an optional env var anyway.

	int i, pos, len = strlen(lib);
	char escaped[MAX_PATH];
	for (i = 0, pos = 0; i <= len && pos < MAX_PATH-1; i++) {
		escaped[pos++] = lib[i];
		if (lib[i] == '\\') escaped[pos++] = '\\';
	}

	std::string r;
	
	r = ".libPaths(c("
		"unlist(strsplit(Sys.getenv(\"R_LIBS_USER\"), \";\")), \"";
	r += escaped;
	r += "\", "
		 "unlist(strsplit(Sys.getenv(\"R_LIBS\"), \";\"))"
		 "));\n";

	execRString(r);

}

int RInit()
{
	structRstart rp;
	Rstart Rp = &rp;
	char Rversion[25];

	char RHome[MAX_PATH];
	char RUser[MAX_PATH];

	char functionsDir[MAX_PATH];

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

	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, functionsDir, MAX_PATH, REGISTRY_KEY, REGISTRY_VALUE_FUNCTIONS_DIR))
		strcpy_s(functionsDir, MAX_PATH, DEFAULT_R_FUNCTIONS_DIR);
	
	// pass this to R via the environment, it might be useful.
	{
		char buffer[64];
		HDC screen = GetDC(NULL);
		double scale = GetDeviceCaps(screen, LOGPIXELSX) / 96.0;
		sprintf_s(buffer, "%f", scale );
		SetEnvironmentVariableA("BERTGraphicsScale", buffer);
		ReleaseDC(NULL, screen);
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

	Rp->RestoreAction = SA_RESTORE;
	Rp->SaveAction = SA_NOSAVE;

	R_SetParams(Rp);
	R_set_command_line_arguments(0, 0);

	FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));

	signal(SIGBREAK, my_onintr);
	GA_initapp(0, 0);
	setup_Rmainloop();
	R_ReplDLLinit();

	::WaitForSingleObject(muxExecR, INFINITE );

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
				Rf_defineVar(Rf_install("STARTUP_FOLDER"), Rf_mkString(getFunctionsDir().c_str()), e);
				Rf_defineVar(Rf_install("R_HOME"), Rf_mkString(RHome), e);

			}
			UNPROTECT(1);
		}
		UNPROTECT(1);


	}

	// construct version (somewhat like R.version).  for now we are dumping this in the 
	// global environment, but it would probably be wiser to construct an environment
	// for this (and any other constants) and `attach` it.

	SEXP version = constructBERTVersion();
	Rf_defineVar(Rf_install("BERT.version"), version, R_GlobalEnv);
	UNPROTECT(1);

	// callbacks

	R_RegisterCCallable("BERT", "BERTExternalCallback", (DL_FUNC)BERTExternalCallback);
	R_RegisterCCallable("BERT", "BERTExternalCOMCallback", (DL_FUNC)BERTExternalCOMCallback);
	
	// (try) to load BERT module.  flag warning for a little later. originally we used the 
	// library path only for this library.  we're now going to set that, using .libPaths, so it 
	// becomes the default. that's an attempt to let libraries survive R version upgrades.

	int module_err = 0;
	{
		std::string libloc = RUser;
		if (strlen(RUser) && RUser[strlen(RUser) - 1] != '\\' && RUser[strlen(RUser) - 1] != '/') libloc += "\\";
		libloc += "lib";
		int err;

		// set path

		appendLibraryPath(libloc.c_str());
		// R_tryEvalSilent(Rf_lang2(Rf_install(".libPaths"), Rf_mkString(libloc.c_str())), R_GlobalEnv, &err);

		// OK, load library

		SEXP namedargs;
		const char *names[] = { "package", "lib.loc", "" };

		PROTECT(namedargs = mkNamed(VECSXP, names));
		SET_VECTOR_ELT(namedargs, 0, Rf_mkString("BERTModule"));
		SET_VECTOR_ELT(namedargs, 1, Rf_mkString(libloc.c_str()));

#ifdef _DEBUG
		R_tryEval
#else 
		R_tryEvalSilent
#endif
			(Rf_lang3(Rf_install("do.call"), Rf_mkString("library"), namedargs), R_GlobalEnv, &module_err);

		UNPROTECT(1);
	}

	::ReleaseMutex(muxExecR);

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

	// BERT banner in the console.  needs to be narrow, though.
	{
		std::string strBanner = "---\n\n";
		int i, len = wcslen(ABOUT_BERT_TEXT);
		for (i = 0; i < len; i++) strBanner += ((char)(ABOUT_BERT_TEXT[i] & 0xff));
		strBanner += "\n\n";
		logMessage(strBanner.c_str(), 0, 1);
	}

	if (module_err) {
		char message[256];
		sprintf_s(message, 
			" * Warning: The BERT functions module could not be loaded.\n" 
			" * Some functions may not be available.  Please check your\n" 
			" * configuration.\n\n");
		logMessage(message, 0, 1);
	}

	ClearFunctions();
	LoadStartupFile();
	MapFunctions();

	return 0;

}

void RShutdown()
{
	char RUser[MAX_PATH];
	DWORD dwPreserve = 0;

	rshell_disconnect();

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

	initGDIplus(false); // shutdown

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

	::WaitForSingleObject(muxExecR, INFINITE);

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

	// see STRSXP2XLOPER.  note that this is never 0-len
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
	// we will manage this memory, but we use a shared
	// resource for 0-len strings (so don't delete them)

	result->xltype = xltypeStr | xlbitDLLFree;
	const char *sz = CHAR(Rf_asChar(str));
	int len = MultiByteToWideChar(CP_UTF8, 0, sz, -1, 0, 0);

	if (len == 0) {
		result->val.str = emptyStr;
	}
	else {
		result->val.str = new XCHAR[len + 1];
		result->val.str[0] = len == 0 ? 0 : len - 1;
		MultiByteToWideChar(CP_UTF8, 0, sz, -1, &(result->val.str[1]), len);
	}

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

	rshell_block(true);

	SEXP env = PROTECT(R_tryEvalSilent(Rf_lang2(Rf_install("get"), Rf_mkString(ENV_NAME)), R_GlobalEnv, &err));
	if (env)
	{
		SEXP rslt = PROTECT(R_tryEval(Rf_lang2(Rf_install(".ExecWatchCallback"), Rf_mkString(path.c_str())), env, &err));
		UNPROTECT(1);
	}
	UNPROTECT(1);

	rshell_block(false);

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
 *
 */
int getAutocomplete(AutocompleteData &ac, std::string &line, int caret)
{

	int err;

	SEXP sline = Rf_mkString(line.c_str());
	SEXP spos = Rf_ScalarInteger(caret);

	ac.addition = "";
	ac.comps = "";
	ac.signature = "";
	ac.token = "";
	ac.function = "";

	ac.tokenIndex = 0;
	ac.file = false;

	SEXP arglist;
	PROTECT(arglist = Rf_allocVector(VECSXP, 2));

	SET_VECTOR_ELT(arglist, 0, Rf_mkString(line.c_str()));
	SET_VECTOR_ELT(arglist, 1, Rf_ScalarInteger(caret));

	SEXP result = PROTECT(R_tryEval(
		Rf_lang3(Rf_install("do.call"),
			Rf_lang3(Rf_install("get"), Rf_mkString(".Autocomplete"), Rf_lang2(Rf_install("asNamespace"), Rf_mkString("BERTModule"))),
			arglist),
		R_GlobalEnv, &err));

	if (!err) {

		if (TYPEOF(result) == VECSXP) {

			SEXP sexp, getelement = PROTECT(Rf_install("getElement"));

			sexp = R_tryEval(Rf_lang3(getelement, result, Rf_mkString("addition")), R_GlobalEnv, &err);
			if( !err && TYPEOF(sexp) == STRSXP) ac.addition = CHAR(STRING_ELT(sexp, 0));

			sexp = R_tryEval(Rf_lang3(getelement, result, Rf_mkString("comps")), R_GlobalEnv, &err);
			if (!err && TYPEOF(sexp) == STRSXP) ac.comps = CHAR(STRING_ELT(sexp, 0));

			sexp = R_tryEval(Rf_lang3(getelement, result, Rf_mkString("function.signature")), R_GlobalEnv, &err);
			if (!err && TYPEOF(sexp) == STRSXP) ac.signature = CHAR(STRING_ELT(sexp, 0));

			sexp = R_tryEval(Rf_lang3(getelement, result, Rf_mkString("token")), R_GlobalEnv, &err);
			if (!err && TYPEOF(sexp) == STRSXP) ac.token = CHAR(STRING_ELT(sexp, 0));

			sexp = R_tryEval(Rf_lang3(getelement, result, Rf_mkString("function")), R_GlobalEnv, &err);
			if (!err && TYPEOF(sexp) == STRSXP) ac.function = CHAR(STRING_ELT(sexp, 0));

			sexp = R_tryEval(Rf_lang3(getelement, result, Rf_mkString("start")), R_GlobalEnv, &err);
			if (!err && TYPEOF(sexp) == INTSXP) ac.tokenIndex = (INTEGER(sexp))[0] - 1;

			sexp = R_tryEval(Rf_lang3(getelement, result, Rf_mkString("end")), R_GlobalEnv, &err);
			if (!err && TYPEOF(sexp) == INTSXP) ac.end = (INTEGER(sexp))[0] - 1;

			sexp = R_tryEval(Rf_lang3(getelement, result, Rf_mkString("in.quotes")), R_GlobalEnv, &err);
			if (!err && ( TYPEOF(sexp) == INTSXP || TYPEOF(sexp) == LGLSXP )) ac.file = (INTEGER(sexp))[0] ? true : false;

			UNPROTECT(1);

		}

	}

	UNPROTECT(2);

	return err;
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

/**
 * reimplementation of translation
 */
void SEXP2XLOPER(LPXLOPER12 xloper, SEXP sexp, bool inner = false, int r_offset = 0, bool api_call = false ) {

	// we want to fill up the space, not showing errors if the data 
	// is smaller than the excel range.  the only way to do that is 
	// with strings ("") -- the types nil and missing don't do it 
	
	// NOTE: while this is good for returning values from functions, it's
	// not good for Excel API functions from the shell, which also use this 
	// function.  in fact it breaks them completely.  so need a flag to repair...

	// (1) API calls should set the "inner" flag to prevent the "caller" lookup.
	//     both because it's time-consuming and because it won't work properly.
	//     actually, use a separate flag...
	//
	// (2) API calls should pass explicit range sizes...
	 
	// check length first.  if the R function returns a scalar,
	// use standard excel behavior (which is to fill up the output range)

	api_call = true; // TESTING

	int len = -1, type = -1;
	if (sexp) {
		len = Rf_length(sexp);
		type = TYPEOF(sexp);
	}

	int xlrows = 1;
	int xlcols = 1;
	int xllen = 1;

	if (!inner && !api_call && len > 1) {

		XLOPER12 xlrslt;
		Excel12(xlfCaller, &xlrslt, 0, 0);
		if (xlrslt.xltype == xltypeSRef) {

			xlrows = xlrslt.val.sref.ref.rwLast - xlrslt.val.sref.ref.rwFirst + 1;
			xlcols = xlrslt.val.sref.ref.colLast - xlrslt.val.sref.ref.colFirst + 1;

			xllen = xlrows * xlcols;
			if (xllen > 1) {
				xloper->xltype = xltypeMulti | xlbitDLLFree;
				xloper->val.array.rows = xlrows;
				xloper->val.array.columns = xlcols;
				xloper->val.array.lparray = new XLOPER12[xlrows * xlcols];
				for (int i = 0; i < xllen; i++) {
					xloper->val.array.lparray[i].xltype = xltypeStr; // no free bit: this is static
					xloper->val.array.lparray[i].val.str = emptyStr;
				}
			}

		}
		else {

			// UPDATE: for issue #55, check if it's a string.  that indicates it has 
			// been called by a button (or other worksheet control) and is running 
			// in VBA.  the string value is the name of the button.  it's not clear 
			// if there are other cases in which a string would be returned.

			// UPDATE 2: we're going to make this the default.  it turns out that 
			// VBA functions called from a ribbon toolbar button return yet another
			// type -- in the example case, an array of numbers (1,0).  we are now 
			// assuming the default is to return the full array; the special case
			// is when we see an SRef, and we map to size.

			// this probably indicates it's being called from VBA -- so we should treat
			// as an API call? FIXME: testing

			// note that this is still less efficient than calling BERT.Call, because that
			// xlfCaller call is rather expensive

			api_call = true;

		}
		Excel12(xlFree, 0, 1, (LPXLOPER12)&xlrslt);

	}

	// always do this (unless it's an array)

	if (xllen == 1) {
		xloper->xltype = xltypeNil;
		xloper->val.num = 0;
	}

	LPXLOPER12 firstRef = xloper->xltype == ( xltypeMulti | xlbitDLLFree ) ? xloper->val.array.lparray : xloper;
	
	// check early and return

	if (!sexp || Rf_isNull( sexp )) {
		resetXlOper(firstRef);
		firstRef->xltype = xltypeErr;
		firstRef->val.err = xlerrNull;
		return;
	}

	// len, type

	//int len = Rf_length(sexp);
	//int type = TYPEOF(sexp);

	int n_rows = len;
	int n_cols = 1;

	// can't handle these in Excel; so print the name 
	// to a string and return that, should indicate to 
	// user what is wrong

	if (Rf_isEnvironment(sexp)) {

		int err;
		SEXP rslt = R_tryEval(Rf_lang2(Rf_install("capture.output"), Rf_lang2(Rf_install("print"), sexp )), R_GlobalEnv, &err);
		if (!err) STRSXP2XLOPER(firstRef, rslt);
		else {
			resetXlOper(firstRef);
			firstRef->xltype = xltypeErr;
			firstRef->val.err = xlerrValue;
		}
		return;

	}

	// check 0-len here, otherwise the environment code 
	// above won't run if environment is empty

	if (len == 0) return;

	// matrices have column counts, but items are in a 
	// single vector.  frames have columns and the length()
	// returned by a frame is the number of columns.  this
	// is a little problematic because excel is row-dominant
	// -- could we transpose?

	if (Rf_isMatrix(sexp)) {
		n_rows = Rf_nrows(sexp);
		n_cols = Rf_ncols(sexp);
	}
	
    // to support lists-of-lists in vba, we need nested ararys.
    // not sure what happens in a spreadsheet if you have this
    // result. should not be harmful (testing)

	if(true){ // if (api_call && !inner) {

		int check_len = n_cols;

		if (Rf_isFrame(sexp)) {
			check_len = n_cols = len + 1;
			SEXP sexp_col = PROTECT(VECTOR_ELT(sexp, 0));
			n_rows = Rf_length(sexp_col) + 1;
			UNPROTECT(1);
		}

		xlrows = n_rows;
		xlcols = check_len; //  n_cols;
		xllen = xlrows * xlcols;

		if (xllen > 1) {
			xloper->xltype = xltypeMulti | xlbitDLLFree;
			xloper->val.array.rows = xlrows;
			xloper->val.array.columns = xlcols;
			xloper->val.array.lparray = new XLOPER12[xlrows * xlcols];
			for (int i = 0; i < xllen; i++) {
				xloper->val.array.lparray[i].xltype = xltypeStr; // no free bit
				xloper->val.array.lparray[i].val.str = emptyStr;
			}
		}
		firstRef = xloper->xltype == (xltypeMulti | xlbitDLLFree) ? xloper->val.array.lparray : xloper;
					
	}

	//n_rows = MIN(n_rows, xlrows);
	//n_cols = MIN(n_cols, xlcols);
	
	int index = 0;

	if (Rf_isLogical(sexp)) {
		for (int r = 0; r < n_rows && r < xlrows; r++) {
			for (int c = 0; c < n_cols && c < xlcols; c++) {
				LPXLOPER12 ref = firstRef + (r * xlcols + c);
				int lgl = (LOGICAL(sexp))[c * n_rows + r + r_offset];
				if (lgl== NA_LOGICAL) {
					ref->xltype = xltypeErr;
					ref->val.err = xlerrNA;
				}
				else {
					ref->xltype = xltypeBool;
					ref->val.xbool = lgl ? true : false;
				}
			}
		}
	}
	else if (Rf_isComplex(sexp)) {
		for (int r = 0; r < n_rows && r < xlrows; r++) {
			for (int c = 0; c < n_cols && c < xlcols; c++) {
				LPXLOPER12 ref = firstRef + (r * xlcols + c);
				Rcomplex cpx = (COMPLEX(sexp))[c * n_rows + r + r_offset];
				CPLXSXP2XLOPER(ref, cpx);
			}
		}
	}
	else if (Rf_isFactor(sexp)) {

		// FIXME: if this comes in from a frame, we are passing
		// one element at a time (for ordering reasons).  should
		// we pass in the levels as well to avoid repeated lookups?

		SEXP levels = Rf_getAttrib(sexp, R_LevelsSymbol);
		int levels_len = Rf_length(levels);

		for (int r = 0; r < n_rows && r < xlrows; r++) {
			for (int c = 0; c < n_cols && c < xlcols; c++) {
				LPXLOPER12 ref = firstRef + (r * xlcols + c);
				int val = (INTEGER(sexp))[c * n_rows + r + r_offset];
				
				if (val == NA_INTEGER) {
					ref->xltype = xltypeErr;
					ref->val.err = xlerrNA;
				}
				else if (val < 1 || val > levels_len) {
					ref->xltype = xltypeErr;
					ref->val.err = xlerrValue;
				}
				else {
					SEXP strsxp = STRING_ELT(levels, val-1); // because this uses 1-based indexing
					if (strsxp == NA_STRING) {
						ref->xltype = xltypeErr;
						ref->val.err = xlerrNA;
					}
					else {
						STRSXP2XLOPER(ref, strsxp);
					}
				}
			}
		}

	}
	else if (Rf_isInteger(sexp)){
		for (int r = 0; r < n_rows && r < xlrows; r++) {
			for (int c = 0; c < n_cols && c < xlcols; c++) {

				LPXLOPER12 ref = firstRef + (r * xlcols + c);
				int val = (INTEGER(sexp))[c * n_rows + r + r_offset];

				if (val == NA_INTEGER) {
					ref->xltype = xltypeErr;
					ref->val.err = xlerrNA;
				}
				else {
					ref->xltype = xltypeInt;
					ref->val.w = val;
				}
			}
		}
	}
	else if (isReal(sexp) || Rf_isNumber(sexp))
	{
		for (int r = 0; r < n_rows && r < xlrows; r++) {
			for (int c = 0; c < n_cols && c < xlcols; c++) {

				LPXLOPER12 ref = firstRef + (r * xlcols + c);
				double dbl = (REAL(sexp))[c * n_rows + r + r_offset];

				if( !R_finite(dbl)){ 
					ref->xltype = xltypeErr;
					ref->val.err = xlerrNA;
				}
				else {
					ref->xltype = xltypeNum;
					ref->val.num = dbl;
				}
			}
		}
	}
	else if (isString(sexp)) {
		for (int r = 0; r < n_rows && r < xlrows; r++) {
			for (int c = 0; c < n_cols && c < xlcols; c++) {
				LPXLOPER12 ref = firstRef + (r * xlcols + c);
				SEXP strsxp = STRING_ELT(sexp, c * n_rows + r + r_offset);
				if (strsxp == NA_STRING) {
					ref->xltype = xltypeErr;
					ref->val.err = xlerrNA;
				}
				else {
					STRSXP2XLOPER(ref, strsxp);
				}
			}
		}
	}
	else if (type == VECSXP) {

		if (Rf_isFrame(sexp)) {

			// special handling for frames because the counts work 
			// differently, and because we deref lists.  also we include
			// row, column names for frames but not for anything else
			// (even if they have them -- FIXME?)

			n_cols = len;

			// data

			for (int c = 0; c < n_cols && c < xlcols-1; c++) {
				SEXP sexp_col = PROTECT(VECTOR_ELT(sexp, c));
				n_rows = Rf_length(sexp_col);
				for (int r = 0; r < n_rows && r < xlrows-1; r++) {
					LPXLOPER12 ref = firstRef + ((r+1) * xlcols + (c+1));
					SEXP2XLOPER(ref, sexp_col, true, r, false); // no API call here; this should work out... ?
				}
				UNPROTECT(1);
			}

			// names

			SEXP rownames = getAttrib(sexp, R_RowNamesSymbol);
			if (rownames && TYPEOF(rownames) != 0) {
				for (int r = 0; r < n_rows && r < xlrows - 1; r++) {
					LPXLOPER12 ref = firstRef + ((r + 1) * xlcols);
					SEXP2XLOPER(ref, rownames, true, r);
				}
			}

			SEXP colnames = getAttrib(sexp, R_NamesSymbol);
			if (colnames && TYPEOF(colnames) != 0) {
				for (int c = 0; c < n_cols && c < xlcols - 1; c++) {
					LPXLOPER12 ref = firstRef + (c + 1);
					SEXP2XLOPER(ref, colnames, true, c);
				}
			}
		}
		else {
			for (int r = 0; r < n_rows && r < xlrows; r++) {
				for (int c = 0; c < n_cols && c < xlcols; c++) {
					LPXLOPER12 ref = firstRef + (r * xlcols + c);
					SEXP vector_elt = VECTOR_ELT(sexp, c * n_rows + r + r_offset);
					SEXP2XLOPER(ref, vector_elt, true, 0, api_call);
				}
			}
		}
	}
	else {

		firstRef->xltype = xltypeErr;
		firstRef->val.err = xlerrValue;

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
		SEXP2XLOPER(&result, ans);
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
		SEXP s;
		PROTECT(s = Rf_allocMatrix(VECSXP, rows, cols));
		for (int i = 0; i < cols; i++)
		{
			for (int j = 0; j < rows; j++)
			{
				ptr = (j+rowOffset) * cols + i + colOffset;
				SET_VECTOR_ELT(s, idx, XLOPER2SEXP(&(px->val.array.lparray[ptr]), 0));
				idx++; // not macro friendly with opt
			}
		}
		UNPROTECT(1);
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

	// fixme: want to protect this? A: yes.
	SEXP s = Rf_allocMatrix(VECSXP, rows, cols);
	PROTECT(s);

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
	UNPROTECT(1);

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

void RExecVector(std::vector<std::string> &vec, int *err, PARSE_STATUS_2 *status, bool printResult, bool excludeFromHistory, std::string *pjresult )
{
	static std::regex rex_empty("^\\s*#.*$");
	ParseStatus ps;
	SEXP rslt = 0;

	// ?? // g_buffering = true;

	// if you want the history() command to appear on the history
	// stack, like bash, you need to add the line(s) to the buffer
	// here; and then potentially remove them if you get an INCOMPLETE
	// response (b/c in that case we'll see it again)

	bool empty = true;
	for (std::vector<std::string>::iterator iter = vec.begin(); empty && iter != vec.end(); iter++) {
		std::string &test = *iter;
		empty = empty && (!test.length() || std::regex_match(*iter, rex_empty));
	}

	if (empty) {
		ps = PARSE_OK;
	}
	else {
		rslt = PROTECT(ExecR(vec, err, &ps, true));
	}


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

	if (!excludeFromHistory) {
		if (ps == PARSE_OK || ps == PARSE_ERROR) {
			while (cmdBuffer.size() >= MAX_CMD_HISTORY) cmdBuffer.erase(cmdBuffer.begin());
			for (std::vector< std::string > ::iterator iter = vec.begin(); iter != vec.end(); iter++)
				cmdBuffer.push_back(iter->c_str());
		}
	}

	if (ps == PARSE_OK && rslt)
	{
		if (printResult) {

			if (!empty) {
				int checkLen = Rf_length(rslt);

				SEXP elt = VECTOR_ELT(rslt, 1);
				int *pVisible = LOGICAL(elt);

				SEXP v0 = VECTOR_ELT(rslt, 0);
				int vt = TYPEOF(v0);

				if (*pVisible) {
					::WaitForSingleObject(muxExecR, INFINITE);
					Rf_PrintValue(VECTOR_ELT(rslt, 0));
					::ReleaseMutex(muxExecR);
				}
			}
		}
		else {
			if( pjresult ) (*pjresult) = SEXP2JSONstring(rslt);
		}
	}
	
	if (!empty) {
		UNPROTECT(1);
	}

	g_buffering = false;
	flush_log();
}

bool RExec4(LPXLOPER12 rslt, RFuncDesc2 &func, std::vector< LPXLOPER12 > &args) {

	SEXP sargs;
	SEXP lns, ans = 0;
	int i, errorOccurred;

	resetXlOper(rslt);

	::WaitForSingleObject(muxExecR, INFINITE);

	DebugOut(" ** Function call\n");
	DebugOut(" ** Function: %s\n", func.r_name.c_str());

	PROTECT(sargs = Rf_allocVector(VECSXP, args.size()));
	for (i = 0; i < args.size(); i++)
	{
		SET_VECTOR_ELT(sargs, i, XLOPER2SEXP(args[i], 0, true));
	}

	SEXP envir = g_Environment ? g_Environment : R_GlobalEnv;
	if (func.env) envir = (SEXP)func.env;

	/*
	SVECTOR elts;
	Util::split(funcname, '$', 1, elts);
	if (elts.size() > 1) {

		envir = R_tryEvalSilent(Rf_lang2(Rf_install("get"), Rf_mkString(elts[0].c_str())), R_GlobalEnv, &errorOccurred);
		funcname = elts[1];
	}
	*/

	SEXP callee = Rf_mkString(func.r_name.length() ? func.r_name.c_str() : func.pairs[0].first.c_str());

	lns = PROTECT(Rf_lang5(Rf_install("do.call"), callee, sargs, R_MissingArg, envir));
	PROTECT(ans = R_tryEval(lns, R_GlobalEnv, &errorOccurred));

	SEXP2XLOPER(rslt, ans);

	UNPROTECT(3);
	 ::ReleaseMutex(muxExecR);

	return true;

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

	SEXP2XLOPER(rslt, ans, false, 0, true);

	UNPROTECT(3);
	 ::ReleaseMutex(muxExecR);

	return true;
}

SEXP BERTWatchFiles(SEXP list) {

	int len = Rf_length(list);
	std::vector< std::string > files;

	// must be list of stringsre
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

SEXP JSONCallback(int command, SEXP data) {

	std::string str = SEXP2JSONstring(data);

	switch (command) {
	case CC_PROGRESS_BAR:
		rshell_push_packet("progress", str.c_str(), false);
		return R_NilValue;
		break;
	case CC_DOWNLOAD:
		rshell_push_packet("download", str.c_str(), true);
		return syncResponse;
		break;
	default:
		DebugOut("Invalid json command: %d\n", command);
	}
	return R_NilValue;

}

SEXP BERTExternalCallback(int cmd, void* data, void* data2) {

	switch (cmd) {
	case 1:
		initGDIplus(true);
		bert_device_init(data, data2);
		return Rf_ScalarInteger(0);

	case 2:
		return Rf_mkString("BERT-Default");

	case 100:
		return JSONCallback(CC_DOWNLOAD, (SEXP)data);

	case 200:
		return JSONCallback(CC_PROGRESS_BAR, (SEXP)data);

	case 300:
		break;

	case 400:
		return ConsoleHistory((SEXP)data);

	}

	return R_NilValue;
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
					SEXP2XLOPER(xdata[i], tmp, false, 0, true );
			}
			break;
		case STRSXP:
			SEXP2XLOPER(xdata[i], STRING_ELT(data, i), false, 0, true);
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

SEXP BERTExternalCOMCallback(SEXP name, SEXP calltype, SEXP p, SEXP args) {

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

/*
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
*/

SEXP ConsoleHistory(SEXP args) {

	std::string pattern = "";

	int len = Rf_length(args);
	int histlen = 25;
	bool rev = false;

	std::vector< std::string > hlist;

	if (len > 1) {

		SEXP s = VECTOR_ELT(args, 0);
		if (TYPEOF(s) == INTSXP) histlen = (INTEGER(s))[0];
		else if (TYPEOF(s) == REALSXP) histlen = (int)(REAL(s))[0];
		else return Rf_ScalarLogical(0);

		s = VECTOR_ELT(args, 1);
		if( TYPEOF(s) == INTSXP || TYPEOF(s) == LGLSXP ) rev = (INTEGER(s))[0];
		else return Rf_ScalarLogical(0);

		if (len > 2)
		{
			s = VECTOR_ELT(args, 2);
			if (TYPEOF(s) == STRSXP) {
				pattern = CHAR(STRING_ELT(s, 0));
			}
		}

		std::vector< std::string > *sv = &cmdBuffer;
		std::vector< std::string > smatch;
		if (pattern.length()) {
			std::regex rex(pattern);
			for (std::vector< std::string > :: iterator iter = cmdBuffer.begin(); iter != cmdBuffer.end(); iter++) {
				if (std::regex_search(*iter, rex)) {
					smatch.push_back(iter->c_str());
				}
			}
			sv = &smatch;
		}

		int count = sv->size();
		int start = count - histlen - 1; 
		if (start < 0) start = 0;

		for (; start < count; start++) {
			hlist.push_back((*sv)[start].c_str());
		}

	}

	len = hlist.size();
	SEXP shistory = Rf_allocVector(STRSXP, len );
	for (int i = 0; i < len; i++ ){
		SET_STRING_ELT(shistory, i, Rf_mkChar(hlist[i].c_str()));
	}

	Rf_setAttrib(shistory, Rf_mkString("class"), Rf_mkString("history.list"));

	return shistory;

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
	case CC_PROGRESS_BAR:
	case CC_DOWNLOAD:
		return JSONCallback(command, data);
		break;

	case CC_REMAP_FUNCTIONS:
		return RemapFunctions();
		break;

	case CC_HISTORY:
		return ConsoleHistory(data);
		break;

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

	case CC_RELOAD:
		BERT_Reload();
		break;

	}

	return R_NilValue;
}

////////////////


