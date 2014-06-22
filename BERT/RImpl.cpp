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

#include <sstream>
#include <vector>

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

FDVECTOR RFunctions;


// who does this?
#ifdef length
	#undef length
#endif

#ifdef clear
	#undef clear
#endif

extern HMODULE ghModule;
SEXP g_Environment = 0;

SEXP ExecR(const char *code, int *err = 0, ParseStatus *pStatus = 0);
SEXP ExecR(std::string &str, int *err = 0, ParseStatus *pStatus = 0);
SEXP ExecR(std::vector< std::string > &vec, int *err = 0, ParseStatus *pStatus = 0, bool withVisible = false);

SEXP XLOPER2SEXP(LPXLOPER12 px, int depth = 0);

std::string dllpath;

int myReadConsole(const char *prompt, char *buf, int len, int addtohistory)
{
	fputs(prompt, stdout);
	fflush(stdout);
	if (fgets(buf, len, stdin)) return 1; else return 0;
}

void R_WriteConsole(const char *buf, int len)
{
	logMessage(buf, len);
}

void myCallBack(void)
{
	/* called during i/o, eval, graphics in ProcessEvents */
}

void myBusy(int which)
{
	/* set a busy cursor ... if which = 1, unset if which = 0 */
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

static void my_onintr(int sig) { UserBreak = 1; }

///


int UpdateR(std::string &str)
{
	std::stringstream ss(str);
	std::string line;

	int err;
	ParseStatus status;
	std::vector < std::string > vec;

	while (std::getline(ss, line)){
		line = trim(line);
		if (line.length() > 0) vec.push_back(line);
	}
	if (vec.size() > 0)
	{
		ExecR(vec, &err, &status);
		if (status != PARSE_OK) return -3;
		if (err) return -2;
		return 0;
	}

	return -1;

}

void MapFunctions()
{
	SEXP s = 0;
	ParseStatus status;
	int err;
	char env[MAX_PATH];
	char buffer[MAX_PATH];

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
								if (type == 16)
								{
									dflt += "\"";
									dflt += a;
									dflt += "\"";
								}
								else if (type == 10)
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


}

void LoadStartupFile()
{
	// if there is a startup script, load that now

	char RUser[MAX_PATH];
	char path[MAX_PATH];
	char buffer[MAX_PATH];
	std::string contents;

	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, RUser, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_USER))
		ExpandEnvironmentStringsA( DEFAULT_R_USER, RUser, MAX_PATH );

	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, buffer, MAX_PATH, REGISTRY_KEY, REGISTRY_VALUE_STARTUP))
		strcpy_s(buffer, MAX_PATH, DEFAULT_R_STARTUP);

	if (strlen(buffer) && strlen(RUser))
	{
		sprintf_s(path, MAX_PATH, "%s\\%s", RUser, buffer);
		HANDLE file = ::CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file != INVALID_HANDLE_VALUE)
		{
			DWORD read = 0;
			while (::ReadFile(file, buffer, MAX_PATH - 1, &read, NULL))
			{
				if (read <= 0) break;
				buffer[read] = 0;
				contents += buffer;
			}
			::CloseHandle(file);
		}
	}

	if (contents.length() > 0)
	{
		int rslt = UpdateR(contents);
		if (!rslt) ExcelStatus(0);
		else ExcelStatus("Error reading startup file; check R log");
	}
}

short BERT_InstallPackages()
{
	ExecR("install.packages()");
	return 1;
}

void RInit()
{
	structRstart rp;
	Rstart Rp = &rp;
	char Rversion[25];

	char RHome[MAX_PATH];
	char RUser[MAX_PATH];

	sprintf_s(Rversion, 25, "%s.%s", R_MAJOR, R_MINOR);
	if (strcmp(getDLLVersion(), Rversion) != 0) {
		fprintf(stderr, "Error: R.DLL version does not match\n");
		// exit(1);
		return;
	}

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

	/*
	if ((RHome = get_R_HOME()) == NULL) {
		fprintf(stderr, "R_HOME must be set in the environment or Registry\n");
		//exit(1);
		return;
	}
	Rp->rhome = RHome;
	Rp->home = getRUser();
	*/

	Rp->rhome = RHome;
	Rp->home = RUser;

	Rp->CharacterMode = LinkDLL;
	Rp->ReadConsole = myReadConsole;
	Rp->WriteConsole = R_WriteConsole;
	Rp->CallBack = myCallBack;
	Rp->ShowMessage = R_AskOk;
	Rp->YesNoCancel = R_AskYesNoCancel;
	Rp->Busy = myBusy;

	Rp->R_Quiet = FALSE;// TRUE;        /* Default is FALSE */
	Rp->R_Interactive = TRUE;// FALSE; /* Default is TRUE */
	Rp->RestoreAction = SA_RESTORE;
	Rp->SaveAction = SA_NOSAVE;
	R_SetParams(Rp);
	R_set_command_line_arguments(0, 0);// argc, argv);

	FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));

	signal(SIGBREAK, my_onintr);
	GA_initapp(0, 0);
	//readconsolecfg();
	setup_Rmainloop();
	R_ReplDLLinit();


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
				Rf_defineVar(Rf_install("PATH"), Rf_mkString(p), e);
				int idx = 0;
				for (int i = 0; i < plen-1; i++) if (p[i] == '\\') idx = i + 1;
				Rf_defineVar(Rf_install("MODULE"), Rf_mkString(p+idx), e);
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
			char *str = (char*)::LockResource(global);
			std::stringstream ss(str);
			std::string line;

			int err;
			ParseStatus status;
			std::vector < std::string > vec;

			while (std::getline(ss, line)){
				line = trim(line);
				if (line.length() > 0) vec.push_back(line);
			}
			if (vec.size() > 0 )
			{
				ExecR( vec, &err, &status );
			}


			/*
			XLOPER12 xlTmp;
			XCHAR *xsz = new XCHAR[len + 2];
			xsz[0] = len;
			for (int i = 0; i < len; i++) xsz[i + 1] = str[i];
			xlTmp.xltype = xltypeStr;
			xlTmp.val.str = xsz;
			RExec(&xlTmp);
			delete[] xsz;
			*/
		}
	}

	LoadStartupFile();
	MapFunctions();

}

void RShutdown()
{
	Rf_endEmbeddedR(0);
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
	return ans;

}

SEXP ExecR(std::vector < std::string > &vec, int *err, ParseStatus *pStatus, bool withVisible )
{
	ParseStatus status ;
	SEXP cmdSexp, wv = R_NilValue, cmdexpr = R_NilValue;
	SEXP ans = 0;
	int i, errorOccurred;

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
	return ans;

}

__inline void STRSXP2XLOPER( LPXLOPER12 result, SEXP str )
{
	// this is just for our reference, as we do not call
	// xlFree on this object (also, don't call xlFree on
	// this object).

	result->xltype = xltypeStr | xlbitDLLFree;

	const char *sz = CHAR(Rf_asChar(str));
	int i, len = strlen(sz);

	result->val.str = new XCHAR[len + 2];
	result->val.str[0] = len;

	for (i = 0; i < len; i++) result->val.str[i + 1] = sz[i];

}

int getCallTip(std::string &callTip, const std::string &sym)
{
	int err;
	int ret = 0;
	
	SEXP ex = PROTECT(R_tryEval(Rf_lang2(Rf_install("exists"), Rf_mkString(sym.c_str())), R_GlobalEnv, &err));
	if (!ex || TYPEOF(ex) != 10 || !((LOGICAL(ex))[0]))
	{
		UNPROTECT(1); 
		return 0;
	}
	UNPROTECT(1);


	SEXP args = PROTECT(Rf_lang2(Rf_install("args"), Rf_mkString(sym.c_str())));
	if (args)
	{
		SEXP cap = PROTECT(Rf_lang2(Rf_install("capture.output"), args));
		if (cap)
		{
			//SEXP inv = PROTECT(Rf_lang2(Rf_install("invisible"), cap));
			//if ( inv )
			//{
				SEXP result = PROTECT(R_tryEval(cap, R_GlobalEnv, &err));
				if (result && TYPEOF(result) == 16)
				{
					const char *c = CHAR(STRING_ELT(result,0));
					if (!strncmp(c, "function ", 9))
					{
						callTip = &(c[9]);
						ret = 1;
					}
				}
				UNPROTECT(1);
			//}
			//UNPROTECT(1);
		}
		UNPROTECT(1);
	}
	UNPROTECT(1);
	
	return ret;
}

SVECTOR & getWordList(SVECTOR &wordList)
{
	SEXP rslt = PROTECT(ExecR(std::string("BERTXLL$WordList()")));
	if (rslt)
	{
		// int type = TYPEOF(rslt);
		int len = Rf_length(rslt);
		for (int i = 0; i < len; i++) wordList.push_back(std::string(CHAR(STRING_ELT(rslt, i))));
	}
	UNPROTECT(1);
	return wordList;

}

void ParseResult(LPXLOPER12 rslt, SEXP ans)
{
	// R values (ignoring data frames) are vectors of typed
	// values.  in some cases they are matrices, but these appear
	// to be just lists with some extra attributes.

	// for our purposes the controlling feature should be 
	// length, which affects whether we return 1 value or multiple 
	// values.

	if (!ans)
	{
		OutputDebugStringA("Null value in ans\n");

		rslt->xltype = xltypeErr;
		rslt->val.err = xlerrValue;
		return;
	}

	int len = Rf_length(ans);
	int type = TYPEOF(ans);

	char sz[64];
	sprintf_s(sz, 64, "Type %d, len %d\n", type, len);
	OutputDebugStringA(sz);

	// else if (Rf_isMatrix(ans))
	if ( len > 1 )
	{
		// for normal vector, use rows

		int nc = 1, nr = len;

		// for matrix, guaranteed to be 2 dimensions and we 
		// can use the convenient functions to get r/c

		if (Rf_isMatrix(ans))
		{
			nc = Rf_ncols(ans);
			nr = Rf_nrows(ans);
			//int len = nc*nr;
			//int type = TYPEOF(ans);
		}

		rslt->xltype = xltypeMulti;
		rslt->val.array.rows = nr;
		rslt->val.array.columns = nc;
		rslt->val.array.lparray = new XLOPER12[len];

		// from the header file:

		/* Under the generational allocator the data for vector nodes comes
		immediately after the node structure, so the data address is a
		known offset from the node SEXP. */

		// which means handling this type-specific, I guess...
		// also: have to swap r/c order 

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
					rslt->val.array.lparray[j*nc + i].xltype = xltypeNum;
					rslt->val.array.lparray[j*nc + i].val.num = (REAL(ans))[idx];
					break;

				case STRSXP:	//  16	  /* string vectors */ 
					STRSXP2XLOPER(&(rslt->val.array.lparray[j*nc + i]), STRING_ELT(ans, idx));
					// ParseResult(&(rslt->val.array.lparray[j*nc + i]), STRING_ELT(ans, idx)); // this is lazy
					break;

				case VECSXP:	//  19	  /* generic vectors */
					ParseResult(&(rslt->val.array.lparray[j*nc + i]), VECTOR_ELT(ans, idx));
					break;
				}

				idx++;
			}
		}
	}
	else
	{
		// single value

		if (Rf_isInteger(ans))
		{
			rslt->xltype = xltypeInt;
			rslt->val.num = Rf_asInteger(ans);
		}
		//else if (Rf_isReal(ans) || Rf_isNumber(ans))
		else if (isReal(ans) || Rf_isNumber(ans))
		{
			rslt->xltype = xltypeNum;
			rslt->val.num = Rf_asReal(ans);
		}
		//else if (Rf_isString(ans))
		else if (isString(ans))
		{
			/*
			rslt->xltype = xltypeStr;
			const char *sz = CHAR(Rf_asChar(ans));
			int i, len = strlen(sz);
			rslt->val.str = new XCHAR[len + 2];
			rslt->val.str[0] = len;
			for (i = 0; i < len; i++) rslt->val.str[i + 1] = sz[i];
			*/
			STRSXP2XLOPER(rslt, ans);
		}
	}
}

LPXLOPER12 BERT_RExec(LPXLOPER12 code)
{
	// not thread safe!
	static XLOPER12 result;

	ParseStatus status;
	SEXP cmdSexp, cmdexpr = R_NilValue;
	int i, errorOccurred;

	resetXlOper(&result);

	char *sz = 0;
	if (code->xltype == xltypeStr)
	{
		int i, len = code->val.str[0];
		sz = new char[len + 1];
		for (i = 0; i < len; i++) sz[i] = code->val.str[i + 1] & 0xff;
		sz[i] = 0;
	}

	result.xltype = xltypeErr;
	result.val.err = xlerrValue;

	SEXP ans = ExecR(sz, &errorOccurred, &status);

	if (sz) delete [] sz;

	ParseResult(&result, ans);


	return &result;
}

void NarrowString(std::string &out, LPXLOPER12 pxl)
{
	int i, len = pxl->val.str[0];
	out = "";
	for (i = 0; i < len; i++) out += (pxl->val.str[i + 1] & 0xff);
}

/**
 * this version of the function checks the values first and, if
 * they are all numeric, returns a Real matrix.  this should 
 * simplify function calls on the R side, but check how expensive
 * this is.
 */
SEXP Multi2SEXP2(LPXLOPER12 px)
{
	int rows = px->val.array.rows;
	int cols = px->val.array.columns;

	// pass 1: check.  for now we only accept types num
	// and integer, although in theory booleans and nils
	// could be coerced to numbers as well.

	// FIXME: the check for Integers may be a waste, as this
	// type seems to be pretty rare these days.

	int idx = 0;
	bool numeric = true;

	for (int i = 0; numeric && i < cols; i++)
	{
		for (int j = 0; numeric && j < rows; j++)
		{
			numeric = numeric &&
				(px->val.array.lparray[j*cols + i].xltype == xltypeNum
				|| px->val.array.lparray[j*cols + i].xltype == xltypeInt);
		}
	}

	if (numeric)
	{
		SEXP s = Rf_allocMatrix(REALSXP, rows, cols);
		for (int i = 0; i < cols; i++)
		{
			double *mat = REAL(s);
			for (int j = 0; j < rows; j++)
			{
				if (px->val.array.lparray[j*cols + i].xltype == xltypeNum) mat[idx] = px->val.array.lparray[j*cols + i].val.num;
				else // if (px->val.array.lparray[j*cols + i].xltype == xltypeInt) 
					mat[idx] = px->val.array.lparray[j*cols + i].val.w;
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
				SET_VECTOR_ELT(s, idx, XLOPER2SEXP(&(px->val.array.lparray[j*cols + i]), 0));
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
 * convert an arbitrary excel value to a SEXP
 */
SEXP XLOPER2SEXP( LPXLOPER12 px, int depth )
{
	XLOPER12 xlArg;
	std::string str;
	SEXP result;

	if (depth >= 2)
	{
		OutputDebugStringA("WARNING: deep recursion\n");
		return R_NilValue;
	}

	switch (px->xltype)
	{
	case xltypeMulti:
		return Multi2SEXP2(px);
		break;

	case xltypeRef:
	case xltypeSRef:
		Excel12(xlCoerce, &xlArg, 1, px);
		result = XLOPER2SEXP(&xlArg, depth++);
		Excel12(xlFree, 0, 1, (LPXLOPER12)&xlArg);
		return result;
		break;

	case xltypeStr:
		NarrowString(str, px);
		return Rf_mkString(str.c_str());
		break;

	case xltypeNum:
		return Rf_ScalarReal(px->val.num);
		break;

	case xltypeInt:
		return Rf_ScalarInteger(px->val.w);
		break;

	case xltypeNil:
	default:
		return R_NilValue;
		break;
	}

}

void RExecVector(std::vector<std::string> &vec, int *err, PARSE_STATUS_2 *status, bool printResult)
{
	ParseStatus ps;
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
		SEXP elt = VECTOR_ELT(rslt, 1);
		int *pVisible = LOGICAL(elt);

		//char sz[64];
		//sprintf_s(sz, 64, "len %d; logical[1] %d\n", Rf_length(rslt), pVisible[0]);
		//OutputDebugStringA(sz);

		if (*pVisible ) Rf_PrintValue(VECTOR_ELT(rslt,0));
	}

	UNPROTECT(1);
}

bool RExec2(LPXLOPER12 rslt, std::string &funcname, std::vector< LPXLOPER12 > &args)
{
	ParseStatus status;
	SEXP arg, quot, env, sargs;
	SEXP lns, ans = 0;
	int i, errorOccurred;
	
	resetXlOper(rslt);

	PROTECT(sargs = Rf_allocVector(VECSXP, args.size()));
	for (i = 0; i < args.size(); i++)
	{
		SET_VECTOR_ELT(sargs, i, XLOPER2SEXP(args[i]));
	}

	if (g_Environment)
	{
		lns = PROTECT(Rf_lang5(Rf_install("do.call"), Rf_mkString(funcname.c_str()), sargs, R_MissingArg, g_Environment));
	}
	else
	{
		lns = PROTECT(Rf_lang3(Rf_install("do.call"), Rf_mkString(funcname.c_str()), sargs));
	}
	PROTECT(ans = R_tryEval(lns, R_GlobalEnv, &errorOccurred));

	ParseResult(rslt, ans);

	UNPROTECT(3);
	return true;
}

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
	case 1023:
		CloseConsole();
		break;
	}

	return R_NilValue;
}
