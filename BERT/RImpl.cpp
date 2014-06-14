#include "stdafx.h"

#include <sstream>
#include <vector>

#define Win32
#include <windows.h>
#include <stdio.h>
#include <Rversion.h>
#define LibExtern __declspec(dllimport) extern
#include <Rinternals.h>
#include <Rembedded.h>
#include <graphapp.h>
#include <R_ext/RStartup.h>
#include <signal.h>
#include <R_ext\Parse.h>

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

///

SEXP ExecR(const char *code, int *err = 0, ParseStatus *pStatus = 0);
SEXP ExecR(std::string &str, int *err = 0, ParseStatus *pStatus = 0);
SEXP ExecR(std::vector< std::string > &vec, int *err = 0, ParseStatus *pStatus = 0);

SEXP XLOPER2SEXP(LPXLOPER12 px, int depth = 0);

///

extern HMODULE ghModule;

///

bool _init = false;
const char *programName = "BERT";

///

int myReadConsole(const char *prompt, char *buf, int len, int addtohistory)
{
	fputs(prompt, stdout);
	fflush(stdout);
	if (fgets(buf, len, stdin)) return 1; else return 0;
}

void myWriteConsole(const char *buf, int len)
{
	std::string str = ""; //  "BERT: ";
	str += buf;
	OutputDebugStringA(str.c_str());
	printf("%s", buf);
}

void myCallBack(void)
{
	/* called during i/o, eval, graphics in ProcessEvents */
}

void myBusy(int which)
{
	/* set a busy cursor ... if which = 1, unset if which = 0 */
}


void myAskOk(const char *info) {

}

int myAskYesNoCancel(const char *question) {
	const int yes = 1;
	return yes;
}

static void my_onintr(int sig) { UserBreak = 1; }

///

std::string trim(const std::string& str, const std::string& whitespace = " \t\r\n")
{
	const auto strBegin = str.find_first_not_of(whitespace);
	if (strBegin == std::string::npos)
		return ""; // no content

	const auto strEnd = str.find_last_not_of(whitespace);
	const auto strRange = strEnd - strBegin + 1;

	return str.substr(strBegin, strRange);
}

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
	
	RFunctions.clear();

	SVECTOR fnames;

	s = PROTECT(ExecR("names(.listfunctionargs())", &err, &status));
	if (s)
	{
		int i, len = Rf_length(s);
		for (i = 0; i < len; i++)
		{
			SEXP name = PROTECT(STRING_ELT(s, i));
			std::string func = CHAR(name);
			fnames.push_back(func);
			UNPROTECT(1);
		}
	}
	UNPROTECT(1);

	s = PROTECT(ExecR(".listfunctionargs()", &err, &status));
	if (s)
	{
		int i, j, len = Rf_length(s);
		for (i = 0; i < len; i++)
		{
			SVECTOR funcdesc;
			funcdesc.push_back(fnames[i]);

			SEXP elt = PROTECT(VECTOR_ELT(s, i));
			int type = TYPEOF(elt);
			int arglen = Rf_length(elt);
			for (j = 0; j < arglen; j++)
			{
				SEXP arg = PROTECT(STRING_ELT(elt, j));
				std::string argname = CHAR(arg);
				funcdesc.push_back(argname);
				UNPROTECT(1);
			}
			UNPROTECT(1);

			RFunctions.push_back(funcdesc);

		}
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

	if ( CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, RUser, MAX_PATH - 1, "Software\\BERT", "R_USER")
		&& CRegistryUtils::GetRegString(HKEY_CURRENT_USER, buffer, MAX_PATH, "Software\\BERT", "StartupFile")
		&& strlen(buffer))
	{
		sprintf_s(path, MAX_PATH, "%s\\%s", RUser, buffer);
		HANDLE file = ::CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file != INVALID_HANDLE_VALUE)
		{
			DWORD read = 0;
			while (::ReadFile(file, buffer, MAX_PATH - 1, &read, NULL))
			{
				if (read <= 0) break;
				buffer[read - 1] = 0;
				contents += buffer;
			}
			::CloseHandle(file);
		}
	}

	if (contents.length() > 0) UpdateR(contents);
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

	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, RHome, MAX_PATH - 1, "Software\\BERT", "R_HOME"))
	{
		// err
	}
	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, RUser, MAX_PATH - 1, "Software\\BERT", "R_USER"))
	{
		// err
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
	Rp->WriteConsole = myWriteConsole;
	Rp->CallBack = myCallBack;
//	Rp->ShowMessage = myAskOk;
//	Rp->YesNoCancel = myAskYesNoCancel;
	Rp->ShowMessage = askok;
	Rp->YesNoCancel = askyesnocancel;
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

SEXP ExecR(std::vector < std::string > &vec, int *err, ParseStatus *pStatus )
{
	ParseStatus status;
	SEXP cmdSexp, cmdexpr = R_NilValue;
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

void ParseResult(LPXLOPER12 rslt, SEXP ans)
{
	if (!ans)
	{
		rslt->xltype = xltypeErr;
		rslt->val.err = xlerrValue;
	}
	else if (Rf_isMatrix(ans))
	{
		// if is matrix, guaranteed to be 2x2 and we 
		// can use the convenient functions to get r/c

		int nc = Rf_ncols(ans);
		int nr = Rf_nrows(ans);
		int len = nc*nr;
		int type = TYPEOF(ans);

		rslt->xltype = xltypeMulti;
		rslt->val.array.rows = nr;
		rslt->val.array.columns = nc;
		rslt->val.array.lparray = new XLOPER12[len];

		char sz[64];
		sprintf_s(sz, 64, "is mat; %dx%d; type %d\n", nr, nc, type);
		OutputDebugStringA(sz);

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
					ParseResult(&(rslt->val.array.lparray[j*nc + i]), STRING_ELT(ans, idx)); // this is lazy
					break;

				case VECSXP:	//  19	  /* generic vectors */
					ParseResult(&(rslt->val.array.lparray[j*nc + i]), VECTOR_ELT(ans, idx));
					break;
				}

				idx++;
			}
		}



	}
	else if (Rf_isInteger(ans) )
	{
		rslt->xltype = xltypeInt;
		rslt->val.num = Rf_asInteger(ans);
	}
	else if (Rf_isReal(ans) || Rf_isNumber(ans))
	{
		rslt->xltype = xltypeNum;
		rslt->val.num = Rf_asReal(ans);
	}
	else if (Rf_isString(ans))
	{
		rslt->xltype = xltypeStr;
		const char *sz = CHAR(Rf_asChar(ans));
		int i, len = strlen(sz);
		rslt->val.str = new XCHAR[len + 2];
		rslt->val.str[0] = len;
		for (i = 0; i < len; i++) rslt->val.str[i + 1] = sz[i];
	}
}

LPXLOPER12 RExec(LPXLOPER12 code)
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
 * convert a (rectangular) range to a matrix
 */
SEXP Multi2SEXP(LPXLOPER12 px)
{
	int rows = px->val.array.rows;
	int cols = px->val.array.columns;

	// fixme: want to protect this?
	SEXP s = Rf_allocMatrix(VECSXP, rows, cols);

	// so it looks like a matrix is just a list (same as 
	// Excel, as it happens); but the order is inverse of Excel.

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
		return Multi2SEXP(px);
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

bool RExec2(LPXLOPER12 rslt, std::string &funcname, std::vector< LPXLOPER12 > &args)
{
	ParseStatus status;
	SEXP arg, sargs;
	SEXP ans = 0;
	int i, errorOccurred;
	

	resetXlOper(rslt);

	PROTECT(sargs = Rf_allocVector(VECSXP, args.size()));
	for (i = 0; i < args.size(); i++)
	{
		SET_VECTOR_ELT(sargs, i, XLOPER2SEXP(args[i]));
	}

	SEXP lns = PROTECT(Rf_lang3(Rf_install("do.call"), Rf_mkString(funcname.c_str()), sargs));
	PROTECT(ans = R_tryEval(lns, R_GlobalEnv, &errorOccurred));

	ParseResult(rslt, ans);

	UNPROTECT(3);
	return true;
}

