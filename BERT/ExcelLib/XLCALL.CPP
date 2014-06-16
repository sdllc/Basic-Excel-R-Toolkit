/*
**  Microsoft Excel Developer's Toolkit
**  Version 14.0
**
**  File:           SRC\XLCALL.CPP
**  Description:    Code file for Excel callbacks
**  Platform:       Microsoft Windows
**
**  This file defines the entry points 
**  which are used in the Microsoft Excel C API.
**
*/

#ifndef _WINDOWS_
#include <windows.h>
#endif

#include "XLCALL.H"

/*
** Excel 12 entry points backwards compatible with Excel 11
**
** Excel12 and Excel12v ensure backwards compatibility with Excel 11
** and earlier versions. These functions will return xlretFailed when
** used to callback into Excel 11 and earlier versions
*/

#define cxloper12Max 255
#define EXCEL12ENTRYPT "MdCallBack12"

typedef int (PASCAL *EXCEL12PROC) (int xlfn, int coper, LPXLOPER12 *rgpxloper12, LPXLOPER12 xloper12Res);

HMODULE hmodule;
EXCEL12PROC pexcel12;

__forceinline void FetchExcel12EntryPt(void)
{
	if (pexcel12 == NULL)
	{
		hmodule = GetModuleHandle(NULL);
		if (hmodule != NULL)
		{
			pexcel12 = (EXCEL12PROC) GetProcAddress(hmodule, EXCEL12ENTRYPT);
		}
	}
}

/*
** This function explicitly sets EXCEL12ENTRYPT.
**
** If the XLL is loaded not by Excel.exe, but by a HPC cluster container DLL,
** then GetModuleHandle(NULL) would return the process EXE module handle.
** In that case GetProcAddress would fail, since the process EXE doesn't
** export EXCEL12ENTRYPT ( since it's not Excel.exe).
**
** First try to fetch the known good entry point,
** then set the passed in address.
*/
#ifdef __cplusplus
extern "C"
#endif	
__declspec(dllexport)
void pascal SetExcel12EntryPt(EXCEL12PROC pexcel12New)
{
	FetchExcel12EntryPt();
	if (pexcel12 == NULL)
	{
		pexcel12 = pexcel12New;
	}
}

int _cdecl Excel12(int xlfn, LPXLOPER12 operRes, int count, ...)
{

	LPXLOPER12 rgxloper12[cxloper12Max];
	va_list ap;
	int ioper;
	int mdRet;

	FetchExcel12EntryPt();
	if (pexcel12 == NULL)
	{
		mdRet = xlretFailed;
	}
	else
	{
		mdRet = xlretInvCount;
		if ((count >= 0)  && (count <= cxloper12Max))
		{
			va_start(ap, count);
			for (ioper = 0; ioper < count ; ioper++)
			{
				rgxloper12[ioper] = va_arg(ap, LPXLOPER12);
			}
			va_end(ap);
			mdRet = (pexcel12)(xlfn, count, &rgxloper12[0], operRes);
		}
	}
	return(mdRet);
	
}

int pascal Excel12v(int xlfn, LPXLOPER12 operRes, int count, LPXLOPER12 opers[])
{

	int mdRet;

	FetchExcel12EntryPt();
	if (pexcel12 == NULL)
	{
		mdRet = xlretFailed;
	}
	else
	{
		mdRet = (pexcel12)(xlfn, count, &opers[0], operRes);
	}
	return(mdRet);

}
