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
#include "..\BERT\ExcelLib\XLCALL.h"
#include "..\BERT\RegistryUtils.h"
#include "..\BERT\RegistryConstants.h"

WCHAR BERTXLL[MAX_PATH] = L"";

int registerSecondXLL( bool reg, LPWSTR secondDll )
{
	XLOPER12 xlRoot, xlPath;
	XLOPER12 xlRes;

	int i, copylen = 0;
	int rslt;
	int len = wcslen(secondDll);

	Excel12(xlGetName, &xlRoot, 0);
	for (i = 0; i < xlRoot.val.str[0]; i++)
	{
		if (xlRoot.val.str[i + 1] == '\\') copylen = i + 1;
	}

	len += copylen;

	xlPath.xltype = xltypeStr;
	xlPath.val.str = new XCHAR[len + 2];
	memcpy(&(xlPath.val.str[1]), &(xlRoot.val.str[1]), sizeof(XCHAR)* copylen);
	memcpy(&(xlPath.val.str[copylen + 1]), &(BERTXLL[0]), sizeof(XCHAR)* wcslen(BERTXLL));

	xlPath.val.str[0] = len;
	
	if( reg ) rslt = Excel12(xlfRegister, &xlRes, 1, &xlPath);
	else rslt = Excel12(xlfUnregister, &xlRes, 1, &xlPath);

	//	char sz[64];
	//	sprintf_s(sz, 64, "Returned: %d\n", rslt);
	//	OutputDebugStringA(sz);

	// FIXME: alert on error

	delete[] xlPath.val.str;
	Excel12(xlFree, 0, 1, &xlRoot);
	Excel12(xlFree, 0, 1, &xlRes);

	return rslt;

}

int BERTLoaded( LPCWSTR compare )
{
	// this is just -1 for the current process
	HANDLE h = GetCurrentProcess();
	HMODULE hMods[1024];
	DWORD cbNeeded;

	int len = wcslen(compare);

	if (EnumProcessModules(h, hMods, sizeof(hMods), &cbNeeded))
	{
		for (int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
		{
			WCHAR szModName[MAX_PATH];
			if (GetModuleFileNameEx(h, hMods[i], szModName, sizeof(szModName) / sizeof(WCHAR)))
			{
				for (int j = 0; j < wcslen(szModName); j++){
					if (!wcsncmp(szModName + j, compare, len)) return -1;
				}
				/*
				//std::string str(szModName);

				// this is fragile, because it's a filename.  

				if (std::string::npos != str.find("BERT")
					&& std::string::npos != str.find(".xll"))
				{
				}
				*/
			}
		}
	}

	return 0;
}

DLLEX BOOL WINAPI xlAutoOpen(void)
{
	char RBin[MAX_PATH];
	char Home[MAX_PATH];

#ifdef _DEBUG 
#ifdef _WIN64
	swprintf_s(BERTXLL, MAX_PATH, L"BERT64D.xll");
#else
	swprintf_s(BERTXLL, MAX_PATH, L"BERT32D.xll");
#endif
#else
#ifdef _WIN64
	swprintf_s(BERTXLL, MAX_PATH, L"BERT64.xll");
#else
	swprintf_s(BERTXLL, MAX_PATH, L"BERT32.xll");
#endif
#endif

	// we have to guard against double-loading here as well

	if (BERTLoaded(BERTXLL)) return true;

	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, RBin, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_HOME, true))
		ExpandEnvironmentStringsA(DEFAULT_R_HOME, RBin, MAX_PATH - 1);

	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, Home, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_USER, true))
		ExpandEnvironmentStringsA(DEFAULT_R_USER, Home, MAX_PATH - 1);

	int len = strlen(RBin);
	if (len > 0)
	{
		if (RBin[len - 1] != '\\') strcat_s(RBin, MAX_PATH - 1, "\\");
	}

	// DERP

#ifdef _WIN64
	strcat_s(RBin, MAX_PATH, "bin\\x64;");
#else
	strcat_s(RBin, MAX_PATH, "bin\\i386;");
#endif

	// set path

	int elen = ::GetEnvironmentVariableA("PATH", 0, 0);
	int blen = strlen(RBin);

	char *buffer = new char[blen+elen+1];
	strcpy_s(buffer, blen+elen+1, RBin);
	if (elen > 0)
	{
		::GetEnvironmentVariableA("PATH", &(buffer[blen]), elen);
	}

	::SetEnvironmentVariableA("PATH", buffer);
	::SetEnvironmentVariableA("HOME", Home);

	delete[] buffer;

	// load xll


	registerSecondXLL(true, (LPWSTR)BERTXLL);
	return true;
}

DLLEX BOOL WINAPI xlAutoClose(void)
{
	registerSecondXLL(false, (LPWSTR)BERTXLL);
	return true;
}

DLLEX LPXLOPER12 WINAPI xlAddInManagerInfo12(LPXLOPER12 xAction)
{
	static XLOPER12 xInfo, xIntAction;
	static XCHAR szAddInStr[128] = L" Basic Excel R Toolkit";

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

	return (LPXLOPER12)&xInfo;
}


