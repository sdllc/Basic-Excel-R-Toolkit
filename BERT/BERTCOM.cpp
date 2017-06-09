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

#include "DebugOut.h"
#include "util.h"

#define METHODNAME L"_Run2"

class String3{
public:
	std::string string1;
	std::string string2;
	std::string string3;

	String3(std::string &s1, std::string &s2, std::string &s3) {
		string1 = s1.c_str();
		string2 = s2.c_str();
		string3 = s3.c_str();
	}
	String3(const String3&rhs) {
		string1 = rhs.string1.c_str();
		string2 = rhs.string2.c_str();
		string3 = rhs.string3.c_str();
	}
};

typedef std::vector< String3> SPVECTOR;

SPVECTOR pendingUserButtons;

// typelibs have been completely removed, in favor of using dispatch.invoke.  
// that was a workaround for (at least one, probably more) broken excel install.

/** cached Excel pointer */
LPDISPATCH pdispApp = 0;

/** pointer to the ribbon class */
LPDISPATCH pdispRibbon = 0;

/** cache this value, as it won't change in a single process (or ever, really, it's 806) */
DISPID dispidRun = 0;

/** stream for the marshaled pointer */
IStream *pstream = 0;

extern void installApplicationObject(ULONG_PTR p);

void string2BSTR(CComBSTR &bstr, std::string &str) {

	const char *sz = str.c_str();
	int strlen = MultiByteToWideChar(CP_UTF8, 0, sz, -1, 0, 0);
	if (strlen > 0) {
		WCHAR *wsz = new WCHAR[strlen];
		MultiByteToWideChar(CP_UTF8, 0, sz, -1, wsz, strlen);
		bstr = wsz;
		delete[] wsz;
	}
	else bstr = "";

}

/**
 * call a function on the ribbon menu via dispinvoke
 */
HRESULT RibbonCall(LPDISPATCH pdisp, WCHAR *method, std::vector<CComVariant> &args, CComVariant &cvResult) {

	if (!pdisp) return E_FAIL;

	DISPPARAMS dispparams;
	HRESULT hr = S_OK;
	DISPID dispid;

	hr = pdisp->GetIDsOfNames(IID_NULL, &method, 1, 1033, &dispid);
	if (FAILED(hr)) {
		DebugOut("ERR 0x%x\n", hr);
		return hr;
	}

	dispparams.cArgs = args.size(); 
	dispparams.cNamedArgs = 0;
	dispparams.rgdispidNamedArgs = 0;
	dispparams.rgvarg = args.size() ? &args[0] : 0; 

	hr = pdisp->Invoke(dispid, IID_NULL, 1033, DISPATCH_METHOD, &dispparams, &cvResult, NULL, NULL);

	return hr;

}

int GetUBCount() {
	return pendingUserButtons.size();
}

int GetUB(char *label, int lsize, char *function, int fsize, char *img, int isize, int index) {

	if (index >= pendingUserButtons.size()) return -1;
	String3&sp = pendingUserButtons[index];

	if (lsize > sp.string1.length()) memcpy(label, sp.string1.c_str(), sp.string1.length());
	else return -1;
	label[sp.string1.length()] = 0;

	if (fsize > sp.string2.length()) memcpy(function, sp.string2.c_str(), sp.string2.length());
	else return -1;
	function[sp.string2.length()] = 0;

	if( isize > sp.string3.length()) memcpy(img, sp.string3.c_str(), sp.string3.length());
	else return -1;
	img[sp.string3.length()] = 0;

	return 0;
}

void RibbonClearUserButtons() {
	std::vector< CComVariant > args;
	CComVariant rslt;

	if( pdispRibbon ) RibbonCall(pdispRibbon, L"ClearUserButtons", args, rslt);
	else pendingUserButtons.clear();
}

void RibbonAddUserButton(std::string &strLabel, std::string &strFunc, std::string &strImageMso) {

	if (pdispRibbon) {
		std::vector< CComVariant > args;
		CComVariant rslt;
		CComBSTR bstrLabel, bstrFunc, bstrImg;

		string2BSTR(bstrLabel, strLabel);
		string2BSTR(bstrFunc, strFunc);

		args.push_back(CComVariant(bstrLabel));
		args.push_back(CComVariant(bstrFunc));

		if (strImageMso.length() > 0) {
			string2BSTR(bstrImg, strImageMso);
			args.push_back(CComVariant(bstrImg));
		}

		RibbonCall(pdispRibbon, L"AddUserButton", args, rslt);
	}
	else {
		pendingUserButtons.push_back(String3(strLabel, strFunc, strImageMso));
	}

}

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

/** convenience overload */
HRESULT DispatchCall(LPDISPATCH pdisp, CComVariant &arg1, CComVariant &arg2, CComVariant &arg3, CComVariant &arg4, CComVariant &cvResult) {
	std::vector<CComVariant> vec;
	vec.push_back(arg1);
	vec.push_back(arg2);
	vec.push_back(arg3);
	vec.push_back(arg4);
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
HRESULT SafeCall( SAFECALL_CMD cmd, std::vector< std::string > *vec, long arg, int *presult )
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
				// it seems that excel will reject the eventual Run2 call if any
				// of the strings in this array are > 255.  for some reason, and 
				// not documented.  I suspect it's because of the old XLOPER limitation,
				// although it's not clear why that would be (or why it would persist
				// into Excel 2007 and later).

				// and what does that mean for UTF8?

				// so preprocess strings...
				std::vector <std::string> vec2;

				for (std::vector< std::string >::iterator iter = vec->begin(); iter != vec->end(); iter++) {
					std::string str = iter->c_str();
					while(str.length() >= 255) {

						size_t pivot = str.find_first_of(" \n\t+-;", 128);
						std::string tmp = str.substr(0, pivot);
						DebugOut("TMP %s\n", tmp.c_str());
						vec2.push_back(tmp);
						str = str.substr(pivot);
					}
					DebugOut("STR %s\n", str.c_str());
					vec2.push_back(str);
				}

				CComSafeArray<VARIANT> cc;
				cc.Create(vec2.size());
				std::vector< std::string > :: iterator iter = vec2.begin();

				for (int i = 0; i < vec2.size(); i++)
				{
					int len = MultiByteToWideChar(CP_UTF8, 0, iter->c_str(), -1, 0, 0);
					WCHAR *pwc = new WCHAR[len];
					MultiByteToWideChar(CP_UTF8, 0, iter->c_str(), -1, pwc, len);
					CComBSTR b(pwc);
					delete[] pwc;

					CComVariant v(b);
					cc.SetAt(i, v);
					iter++;
				}

				// internally this calls back to BERT_SafeCall
				hr = DispatchCall(pdisp, CComVariant(CComBSTR("BERT.SafeCall")), CComVariant(arg), CComVariant((LPSAFEARRAY)cc), cvRslt);
				cc.Destroy();
			}
			break;

		case SCC_CONSOLE_WIDTH:
			hr = DispatchCall(pdisp, CComVariant(CComBSTR("BERT.SafeCall")), CComVariant(10L), CComVariant(CComBSTR(vec->begin()->c_str())), cvRslt);
			break;

		case SCC_AUTOCOMPLETE:
			hr = DispatchCall(pdisp, CComVariant(CComBSTR("BERT.SafeCall")), CComVariant(12L), CComVariant(CComBSTR(vec->begin()->c_str())), CComVariant(arg), cvRslt);
			break;

		case SCC_WATCH_NOTIFY:
			hr = DispatchCall(pdisp, CComVariant(CComBSTR("BERT.SafeCall")), CComVariant(4L), CComVariant(CComBSTR(vec->begin()->c_str())), cvRslt);
			break;

		case SCC_BREAK:
			hr = DispatchCall(pdisp, CComVariant(CComBSTR("BERT.SafeCall")), CComVariant(8L), CComVariant(0L), cvRslt);
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
* marshal pointer for use by other threads
*/
HRESULT Marshal()
{
	if (!pdispApp) return E_FAIL;
	if (pstream) return S_OK; // only once
	return AtlMarshalPtrInProc(pdispApp, IID_IDispatch, &pstream);
}

/**
 * cache pointer to excel, for use by the console
 */
void SetExcelPtr(LPVOID p, LPVOID ribbon)
{
	pdispApp = (IDispatch*)p;
	pdispRibbon = (IDispatch*)ribbon;
	installApplicationObject((ULONG_PTR)p);

	Marshal();

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

 