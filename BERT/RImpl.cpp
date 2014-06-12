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
	std::string str = "BERT: ";
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

int UpdateR(std::string str)
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

void RInit()
{
	structRstart rp;
	Rstart Rp = &rp;
	char Rversion[25], *RHome;

	sprintf_s(Rversion, 25, "%s.%s", R_MAJOR, R_MINOR);
	if (strcmp(getDLLVersion(), Rversion) != 0) {
		fprintf(stderr, "Error: R.DLL version does not match\n");
		// exit(1);
		return;
	}

	R_setStartTime();
	R_DefParams(Rp);
	if ((RHome = get_R_HOME()) == NULL) {
		fprintf(stderr, "R_HOME must be set in the environment or Registry\n");
		//exit(1);
		return;
	}
	Rp->rhome = RHome;
	Rp->home = getRUser();
	Rp->CharacterMode = LinkDLL;
	Rp->ReadConsole = myReadConsole;
	Rp->WriteConsole = myWriteConsole;
	Rp->CallBack = myCallBack;
//	Rp->ShowMessage = myAskOk;
//	Rp->YesNoCancel = myAskYesNoCancel;
	Rp->ShowMessage = askok;
	Rp->YesNoCancel = askyesnocancel;
	Rp->Busy = myBusy;

	Rp->R_Quiet = TRUE;        /* Default is FALSE */
	Rp->R_Interactive = FALSE; /* Default is TRUE */
	Rp->RestoreAction = SA_RESTORE;
	Rp->SaveAction = SA_NOSAVE;
	R_SetParams(Rp);
	R_set_command_line_arguments(0, 0);// argc, argv);

	FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));

	signal(SIGBREAK, my_onintr);
	GA_initapp(0, 0);
	//	readconsolecfg();
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

LPXLOPER12 RExec(LPXLOPER12 code)
{
	// not thread safe!
	static XLOPER12 result;
	static XCHAR *pstr = 0;

	ParseStatus status;
	SEXP cmdSexp, cmdexpr = R_NilValue;
	int i, errorOccurred;

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

	if (!ans) return &result;

	if (Rf_isReal(ans) || Rf_isInteger(ans) || Rf_isNumber(ans))
	{
		result.xltype = xltypeNum;
		result.val.num = Rf_asReal(ans);
	}
	else if (Rf_isValidString(ans))
	{
		/*
		int len = length(ans);
		if (pstr) delete[] pstr;
		pstr = new XCHAR[len + 1];
		pstr[0] = len;

		const char *c = CHAR(ans);
		for (int i = 0; i < len; i++) pstr[i + 1] = c[i];

		result.xltype = xltypeStr;
		result.val.str = pstr;
		*/
	}

	return &result;
}

bool RExec2(LPXLOPER12 rslt, std::string &funcname, std::vector< LPXLOPER12 > &args)
{
	ParseStatus status;
	SEXP arg, sargs;
	SEXP ans = 0;
	int i, errorOccurred;

	PROTECT(sargs = Rf_allocVector(VECSXP, args.size()));
	for (i = 0; i < args.size(); i++)
	{
		XLOPER12 xlArg;
		Excel12(xlCoerce, &xlArg, 1, args[i]);
		switch (xlArg.xltype)
		{
		case xltypeNil:
			SET_VECTOR_ELT(sargs, i, R_NilValue);
			break;

		case xltypeNum:
			SET_VECTOR_ELT(sargs, i, Rf_ScalarReal(xlArg.val.num));
			break;

		case xltypeInt:
			SET_VECTOR_ELT(sargs, i, Rf_ScalarInteger(xlArg.val.w));
			break;
		}
		Excel12(xlFree, 0, 1, (LPXLOPER12)&xlArg);
	}

	SEXP lns = PROTECT(Rf_lang3(Rf_install("do.call"), Rf_mkString(funcname.c_str()), sargs));
	PROTECT(ans = R_tryEval(lns, R_GlobalEnv, &errorOccurred));

	if (!ans)
	{
		rslt->xltype = xltypeErr;
		rslt->val.err = xlerrValue;
	}
	else if (Rf_isReal(ans) || Rf_isInteger(ans) || Rf_isNumber(ans))
	{
		rslt->xltype = xltypeNum;
		rslt->val.num = Rf_asReal(ans);
	}
	else if (Rf_isValidString(ans))
	{
		//result.xltype = xltypeStr | xlbitDLLFree;
		//result.val.str = pstr;
	}

	UNPROTECT(3);
	return true;
}

