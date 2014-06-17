
#ifndef __BERT_H
#define __BERT_H

#include <string>
#include <vector>
#include <list>

#include "XLCALL.H"

#define DLLEX extern "C" __declspec(dllexport)

extern HMODULE ghModule;

#define REGISTRY_KEY					"Software\\BERT"
#define REGISTRY_VALUE_ENVIRONMENT		"Environment"
#define REGISTRY_VALUE_R_USER			"R_USER"
#define REGISTRY_VALUE_R_HOME			"R_HOME"
#define REGISTRY_VALUE_STARTUP			"StartupFile"

#define DEFAULT_ENVIRONMENT				""
#define DEFAULT_R_USER					"%APPDATA%\\BERT"
#define DEFAULT_R_HOME					"%APPDATA%\\BERT\\R-3.1.0"
#define DEFAULT_R_STARTUP				"Functions.R"

static LPSTR funcTemplates[][16] = {
	{ "UpdateScript", "UU#", "BERT.UpdateScript", "R Code", "1", "BERT", "", "99", "Update Script", "", "", "", "", "", "", "" },
	{ "RExec", "UU", "BERT.Exec", "R Code", "1", "BERT", "", "98", "Exec R Code", "", "", "", "", "", "", "" },
	{ "Configure", "A#", "BERT.Configure", "", "1", "BERT", "", "97", "", "", "", "", "", "", "", "" },
	{ "Console", "A#", "BERT.Console", "", "1", "BERT", "", "96", "", "", "", "", "", "", "", "" },
	{ "HomeDirectory", "A#", "BERT.Home", "", "1", "BERT", "", "95", "", "", "", "", "", "", "", "" },
	{ "InstallPackages", "A#", "BERT.InstallPackages", "", "1", "BERT", "", "94", "", "", "", "", "", "", "", "" },
	{ "Reload", "A#", "BERT.Reload", "", "1", "BERT", "", "93", "", "", "", "", "", "", "", "" },
	{ 0 }
};

/** 
 * convenience macro for setting an XLOPER12 string.  
 * sets first char to length.  DOES NOT COPY STRING.
 */
#define XLOPER12STR( x, s )			\
	x.xltype = xltypeStr;			\
	s[0] = wcslen((WCHAR*)s + 1);	\
	x.val.str = s; 

#define MAX_FUNCTION_COUNT 100

LPXLOPER12 UpdateScript(LPXLOPER12 script);

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
short Configure();

/** show console (log) */
short Console();

/** */
short HomeDirectory();

/** */
short Reload();

void RExecStringBuffered(const char *buffer);

/** */
void ExcelStatus(const char *message);

/** */
void logMessage(const char *buf, int len);

void getLogText(std::string &str);

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
