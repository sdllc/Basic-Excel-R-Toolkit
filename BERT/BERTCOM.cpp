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

#define _ATL_NO_DEBUG_CRT

#include <atlbase.h>
#include <atlcom.h>
#include <atlsafe.h>

#include "DebugOut.h"
#include "util.h"

#define METHODNAME L"_Run2"

// typelibs have been completely removed, in favor of using dispatch.invoke.  
// that was a workaround for (at least one, probably more) broken excel install.

/** cached Excel pointer */
IDispatch *pdispApp = 0;

/** cache this value, as it won't change in a single process (or ever, really, it's 806) */
DISPID dispidRun = 0;

/** stream for the marshaled pointer */
IStream *pstream = 0;

/**
 * call the excel run2 method via dispatch, in case the typelib is missing.  
 */
HRESULT DispatchCall(LPDISPATCH pdisp, std::vector<CComVariant> &args, CComVariant &cvResult) {

	DISPPARAMS dispparams;
	HRESULT hr = S_OK;

	// Note on 1033: Excel has problems with non-1033 locales, so we always call as 1033.

	if (!dispidRun) {

		WCHAR *member = METHODNAME;
		hr = pdisp->GetIDsOfNames(IID_NULL, &member, 1, 1033, &dispidRun);
		if (FAILED(hr)) {
			DebugOut("ERR 0x%x\n", hr);
			return hr;
		}
	}

	for (int i = args.size(); i < 31; i++) {
		args.push_back(CComVariant(DISP_E_PARAMNOTFOUND, VT_ERROR)); // missing
	}

	// dispparams in reverse order 
	std::reverse(args.begin(), args.end());

	dispparams.cArgs = args.size(); // must be 31
	dispparams.cNamedArgs = 0;
	dispparams.rgdispidNamedArgs = 0;
	dispparams.rgvarg = &args[0]; // spec guarantees contiguous memory.  still feels wrong, though.

	hr = pdisp->Invoke(dispidRun, IID_NULL, 1033, DISPATCH_METHOD, &dispparams, &cvResult, NULL, NULL);

	return hr;

}

/** convenience overload */
HRESULT DispatchCall(LPDISPATCH pdisp, CComVariant &arg, CComVariant &cvResult) {
	std::vector<CComVariant> vec;
	vec.push_back(arg);
	return DispatchCall(pdisp, vec, cvResult);
}

/** convenience overload */
HRESULT DispatchCall(LPDISPATCH pdisp, CComVariant &arg1, CComVariant &arg2, CComVariant &arg3, CComVariant &cvResult) {
	std::vector<CComVariant> vec;
	vec.push_back(arg1);
	vec.push_back(arg2);
	vec.push_back(arg3);
	return DispatchCall(pdisp, vec, cvResult);
}

/**
 * execute an R call through an asynchronous Excel callback.  we have to 
 * do this when making function calls from the console so we can get on 
 * the correct thread.  
 *
 * note that this is not used in spreadsheet function calls, only when
 * calling from the console.
 */
HRESULT SafeCall( SAFECALL_CMD cmd, std::vector< std::string > *vec, int *presult )
{
	HRESULT hr = E_FAIL;
	LPDISPATCH pdisp = 0;
	CComVariant cvRslt;

	hr = AtlUnmarshalPtr(pstream, IID_IDispatch, (LPUNKNOWN*)&pdisp);
	if(SUCCEEDED(hr) && pdisp){

		switch (cmd)
		{
		case SCC_EXEC:
			{
				CComSafeArray<VARIANT> cc;
				cc.Create(vec->size());
				std::vector< std::string > :: iterator iter = vec->begin();

				for (int i = 0; i < vec->size(); i++)
				{
					CComBSTR b = iter->c_str();
					CComVariant v(b);
					cc.SetAt(i, v);
					iter++;
				}

				hr = DispatchCall(pdisp, CComVariant(CComBSTR("BERT.SafeCall")), CComVariant(0L), CComVariant((LPSAFEARRAY)cc), cvRslt);
				cc.Destroy();
			}
			break;

		case SCC_CALLTIP:
			hr = DispatchCall(pdisp, CComVariant(CComBSTR("BERT.SafeCall")), CComVariant(0L), CComVariant(CComBSTR(vec->begin()->c_str())), cvRslt);
			break;

		case SCC_NAMES:
			hr = DispatchCall(pdisp, CComVariant(CComBSTR("BERT.SafeCall")), CComVariant(2L), CComVariant(CComBSTR(vec->begin()->c_str())), cvRslt);
			break;

		case SCC_WATCH_NOTIFY:
			hr = DispatchCall(pdisp, CComVariant(CComBSTR("BERT.SafeCall")), CComVariant(4L), CComVariant(CComBSTR(vec->begin()->c_str())), cvRslt);
			break;

		case SCC_INSTALLPACKAGES:
			hr = DispatchCall(pdisp, CComVariant(CComBSTR("BERT.InstallPackages")), cvRslt);
			break;

		case SCC_RELOAD_STARTUP:
			hr = DispatchCall(pdisp, CComVariant(CComBSTR("BERT.Reload")), cvRslt);
			break;
		}

		if (SUCCEEDED(hr))
		{
			switch (cvRslt.vt)
			{
			case VT_I8:
				if( presult ) *presult = cvRslt.intVal;
				break;
			case VT_R8:
				if (presult) *presult = cvRslt.dblVal;
				break;
			}
		}
		else
		{
			if (presult) *presult = PARSE2_EXTERNAL_ERROR;
		}

	}
	
	if (pdisp) pdisp->Release();

	return hr;
}

/**
 * cache pointer to excel, for use by the console
 */
void SetExcelPtr(LPVOID p)
{
	pdispApp = (IDispatch*)p;
}

/**
 * free the marshaled pointer.  must be done from the thread that
 * called marshal (cf the ms example, which is wrong)
 */
void FreeStream()
{
	if (pstream) AtlFreeMarshalStream(pstream);
	pstream = 0;
}

/**
 * marshal pointer for use by other threads
 */
HRESULT Marshal()
{
	if (!pdispApp) return E_FAIL;
	if (pstream) return S_OK; // only once
	return AtlMarshalPtrInProc(pdispApp, IID_IDispatch, &pstream);
}


