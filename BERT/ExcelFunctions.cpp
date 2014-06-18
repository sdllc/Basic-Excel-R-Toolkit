
#include "stdafx.h"
#include "BERT.h"
#include "XLCALL.h"
#include "ExcelFunctions.h"
#include "RInterface.h"

DLLEX BOOL WINAPI xlAutoOpen(void)
{
	// debugLogf("Enter xlAutoOpen\n");

	RInit();

	// loop through the function table and register the functions
	RegisterBasicFunctions();
	RegisterAddinFunctions();

	// debugLogf("Exit xlAutoOpen\n");

	SetBERTMenu(true);

	return true;

}


DLLEX void WINAPI xlAutoFree(LPXLOPER pxFree)
{
	if (pxFree->xltype == (xltypeMulti | xlbitDLLFree))
	{
		// NOTE: ALWAYS NEW ARRAYS (wait, what about multidim? CHECKME FIXME TODO)
		delete[] pxFree->val.array.lparray;
	}
	else if (pxFree->xltype == (xltypeStr | xlbitDLLFree))
	{
		// NOTE: ALWAYS NEW char[] FOR STRINGS
		delete[] pxFree->val.str;
	}
	else
	{
		// debugLogf(LOGLEVEL_ERR, "** ERR: invalid type for xlfree callback? %d\n", pxFree->xltype);
	}
}

DLLEX void WINAPI xlAutoFree12(LPXLOPER12 pxFree)
{
	if (pxFree->xltype == (xltypeMulti | xlbitDLLFree))
	{
		// NOTE: ALWAYS NEW ARRAYS (wait, what about multidim? CHECKME FIXME TODO)
		delete[] pxFree->val.array.lparray;
	}
	else if (pxFree->xltype == (xltypeStr | xlbitDLLFree))
	{
		// NOTE: ALWAYS NEW XCHAR[] FOR STRINGS
		delete[] pxFree->val.str;
	}
	else
	{
		// debugLogf(LOGLEVEL_ERR, "** ERR: invalid type for xlfree callback? %d\n", pxFree->xltype);
	}
}


DLLEX BOOL WINAPI xlAutoClose(void)
{
	// debugLogf("Enter xlAutoClose\n");
	SetBERTMenu(false);

	// clean up...
	RShutdown();
	
	return true;
}

DLLEX LPXLOPER12 WINAPI xlAddInManagerInfo12(LPXLOPER12 xAction)
{
	static XLOPER12 xInfo, xIntAction;
	static XCHAR szAddInStr[128] = L" Basic Excel R Toolkit (Internal)";

	// debugLogf("Enter xlAddInManagerInfo12\n");

	XLOPER12 xlInt;
	xlInt.xltype = xltypeInt;
	xlInt.val.w = xltypeInt; // ??

	Excel12(xlCoerce, &xIntAction, 2, xAction, &xlInt);

	if (xIntAction.val.w == 1)
	{
		xInfo.xltype = xltypeStr;
		szAddInStr[0] = (XCHAR)(wcslen(szAddInStr + 1));
		xInfo.val.str = szAddInStr;
	}
	else
	{
		xInfo.xltype = xltypeErr;
		xInfo.val.err = xlerrValue;
	}

	// debugLogf("Exit xlAddInManagerInfo12\n");

	return (LPXLOPER12)&xInfo;
}


