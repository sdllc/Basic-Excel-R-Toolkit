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

#define _ATL_NO_DEBUG_CRT

#include <atlbase.h>
#include <atlcom.h>
#include <atlsafe.h>

#include <iostream>

#include <Rinternals.h>
#include <Rembedded.h>

#include "RCOM.h"

static bool initialized = false;

int __stdcall BERTAPI_Startup() {
	if (initialized) return 0;
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (SUCCEEDED(hr)) {
		initialized = true;
		return 0;
	}
	return -1;
}

void __stdcall BERTAPI_Shutdown() {

	// the problem with this is that we're loaded into another 
	// process, and some other part of that process may have also
	// called coinitialize.  if we call this are we potentially 
	// breaking some other code?  are these calls stacked or 
	// counted in some fashion?

	if (!initialized) return;
	CoUninitialize();
	initialized = false;
}

SEXP __stdcall BERTAPI_CreateDispatch(const char *progid) {

	CLSID clsid;
	IDispatch *pdisp;
	SEXP result = R_NilValue;

	int len = MultiByteToWideChar(CP_UTF8, 0, progid, -1, 0, 0);
	WCHAR *wprogid = new WCHAR[len + 1];

	if (!wprogid) return result; // FIXME: error 

	MultiByteToWideChar(CP_UTF8, 0, progid, -1, wprogid, len + 1);
	CLSIDFromProgID(wprogid, &clsid);
	HRESULT hr = CoCreateInstance(clsid, 0, CLSCTX_LOCAL_SERVER, IID_IDispatch, (void**)&pdisp);

	if (SUCCEEDED(hr)) {

		// pdisp->AddRef();
		// int rc = pdisp->Release();
		// std::cout << "cocreateinstance succeeded (rc " << rc << ")" << std::endl;
		
		result = wrapDispatch((ULONG_PTR)pdisp);
		pdisp->Release(); // see below

	}	
	else {
		// std::cout << "cocreateinstance failed" << std::endl;
		// FIXME: error
	}

	delete[] wprogid;
	return result;
}

SEXP __stdcall BERTAPI_CreateExcelInstance() {

	CLSID clsid;
	IDispatch *pdisp;
	int err = 0;

	CLSIDFromProgID(L"Excel.Application", &clsid);
	HRESULT hr = CoCreateInstance(clsid, 0, CLSCTX_LOCAL_SERVER, IID_IDispatch, (void**)&pdisp);
	if (SUCCEEDED(hr)) {

		pdisp->AddRef();
		int rc = pdisp->Release();
		
		//std::cout << "cocreateinstance succeeded (rc " << rc << ")" << std::endl;

		// create an outside env to hold the application and enums
		SEXP e = PROTECT(R_tryEval(Rf_lang1(Rf_install("new.env")), R_GlobalEnv, &err));
		if (e) {

			// wrap the object.  wrapdispatch adds a ref, which is usually what we 
			// want but not in this case; release the extra ref.

			SEXP obj = wrapDispatch((ULONG_PTR)pdisp);
			if (obj) {
				pdisp->Release();
				Rf_defineVar(Rf_install("Application"), obj, e);
			}

			// add enums to the excel env, rather than stuffing them into the application object
			mapEnums((ULONG_PTR)pdisp, e);
			UNPROTECT(1);

			return e;
		}
	}

	return Rf_ScalarInteger(0);

}
