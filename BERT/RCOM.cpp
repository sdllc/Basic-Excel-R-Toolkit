/*
* Basic Excel R Toolkit (BERT)
* Copyright (C) 2014-2016 Structured Data, LLC
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

#include <windows.h>
#include <stdio.h>
#include <Rversion.h>

#include <hash_map>

// #define USE_RINTERNALS

#include <Rinternals.h>
#include <Rembedded.h>

#include <comdef.h>

#if defined(length)
#undef length
#endif

// FIXME: this is defined twice, FIXME
#define ENV_NAME "BERT"

#include "RCOM.h"

#ifdef _DEBUG

static int objectRefCtr = 0;

#endif

void STRSXP2BSTR(CComBSTR &bstr, SEXP s) {

	const char *sz;
	int type = TYPEOF(s);

	if (type == CHARSXP) {
		sz = CHAR(s);
	}
	else sz = CHAR(STRING_ELT(s, 0));

	int strlen = MultiByteToWideChar(CP_UTF8, 0, sz, -1, 0, 0);
	if (strlen > 0) {
		WCHAR *wsz = new WCHAR[strlen];
		MultiByteToWideChar(CP_UTF8, 0, sz, -1, wsz, strlen);
		bstr = wsz;
		delete[] wsz;
	}
	else bstr = "";
}

void BSTR2String(CComBSTR &bstr, std::string &str)
{
	int len = bstr.Length();
	int slen = WideCharToMultiByte(CP_UTF8, 0, &(bstr.m_str[0]), len, 0, 0, 0, 0);
	char *s = new char[slen + 1];
	WideCharToMultiByte(CP_UTF8, 0, &(bstr.m_str[0]), len, s, slen, 0, 0);
	s[slen] = 0;
	str = s;
	delete[] s;
}

__inline LPDISPATCH SEXP2Ptr(SEXP p) {

	ULONG_PTR up = 0;
	int type = TYPEOF(p);
	if (type == EXTPTRSXP) up = (ULONG_PTR)(R_ExternalPtrAddr(p));
	return (LPDISPATCH)up;
}

void releaseMappedPtr(SEXP ptr) {
	void* p = R_ExternalPtrAddr(ptr);
	if (p) {

#ifdef _DEBUG
		objectRefCtr--;
		DebugOut("[-] Object ref counter: %d\n", objectRefCtr);
#endif

		((LPDISPATCH)p)->Release();
	}
}


bool getCoClassForDispatch(ITypeInfo **ppCoClass, IDispatch *pdisp)
{
	CComPtr<ITypeInfo> spTypeInfo;
	CComPtr<ITypeLib> spTypeLib;

	bool matchIface = false;
	HRESULT hr = pdisp->GetTypeInfo(0, 0, &spTypeInfo);

	if (SUCCEEDED(hr) && spTypeInfo)
	{
		UINT tlidx;
		hr = spTypeInfo->GetContainingTypeLib(&spTypeLib, &tlidx);
	}

	if (SUCCEEDED(hr))
	{
		UINT tlcount = spTypeLib->GetTypeInfoCount();

		for (UINT u = 0; !matchIface && u < tlcount; u++)
		{
			TYPEATTR *pTatt = nullptr;
			CComPtr<ITypeInfo> spTypeInfo2;
			TYPEKIND ptk;

			if (SUCCEEDED(spTypeLib->GetTypeInfoType(u, &ptk)) && ptk == TKIND_COCLASS)
			{
				hr = spTypeLib->GetTypeInfo(u, &spTypeInfo2);
				if (SUCCEEDED(hr))
				{
					hr = spTypeInfo2->GetTypeAttr(&pTatt);
				}
				if (SUCCEEDED(hr))
				{
					for (UINT j = 0; !matchIface && j < pTatt->cImplTypes; j++)
					{
						INT lImplTypeFlags;
						hr = spTypeInfo2->GetImplTypeFlags(j, &lImplTypeFlags);
						if (SUCCEEDED(hr) && lImplTypeFlags == 1) // default interface, disp or dual
						{
							HREFTYPE handle;
							if (SUCCEEDED(spTypeInfo2->GetRefTypeOfImplType(j, &handle)))
							{
								CComPtr< ITypeInfo > spTypeInfo3;
								if (SUCCEEDED(spTypeInfo2->GetRefTypeInfo(handle, &spTypeInfo3)))
								{
									CComBSTR bstr;
									TYPEATTR *pTatt2 = nullptr;
									CComPtr<IUnknown> punk = 0;

									hr = spTypeInfo3->GetTypeAttr(&pTatt2);
									if (SUCCEEDED(hr)) hr = pdisp->QueryInterface(pTatt2->guid, (void**)&punk);
									if (SUCCEEDED(hr))
									{
										*ppCoClass = spTypeInfo2;
										// ... (*ppCoClass)->AddRef();
										matchIface = true;
									}

									if (pTatt2) spTypeInfo3->ReleaseTypeAttr(pTatt2);
								}
							}
						}
					}
				}
				if (pTatt) spTypeInfo2->ReleaseTypeAttr(pTatt);
			}
		}
	}

	return matchIface; 
}

void mapInterface( std::string &name, std::vector< MemberRep > &mrlist, CComPtr<ITypeInfo> typeinfo, CComPtr<ITypeLib> typelib, TYPEATTR *pTatt, TYPEKIND tk)
{
	std::string elementname;
	CComBSTR bstrName;
	FUNCDESC *fd;

	for (UINT u = 0; u < pTatt->cFuncs; u++)
	{
		fd = 0;
		HRESULT hr = typeinfo->GetFuncDesc(u, &fd);
		if (SUCCEEDED(hr))
		{
			elementname.clear();
			CComBSTR bstrDoc;

			hr = typeinfo->GetDocumentation(fd->memid, &bstrName, 0, 0, 0);
			if (SUCCEEDED(hr))
			{
				// mask hidden, restricted interfaces.  FIXME: flag
				if ((fd->wFuncFlags & 0x41) == 0) {

					UINT namecount = 0;
					CComBSTR bstrNameList[32];
					typeinfo->GetNames(fd->memid, (BSTR*)&bstrNameList, 32, &namecount);

					BSTR2String(bstrName, elementname);
					MemberRep mr;
					mr.name = elementname.c_str();
					if (fd->invkind == INVOKE_FUNC) mr.mrflags = MRFLAG_METHOD;
					else if (fd->invkind == INVOKE_PROPERTYGET) mr.mrflags = MRFLAG_PROPGET;
					else mr.mrflags = MRFLAG_PROPPUT;

					for (int i = 1; i < namecount; i++) {
						std::string arg;
						BSTR2String(bstrNameList[i], arg);
						mr.args.push_back(arg);
					}

					mrlist.push_back(mr);

				}
			}
			typeinfo->ReleaseFuncDesc(fd);
		}
	}

}

void mapEnum(std::string &name, CComPtr<ITypeInfo> typeinfo, TYPEATTR *pTatt, SEXP targetEnv)
{
	VARDESC *vd = 0;
	CComBSTR bstrName;
	std::string elementname;

	std::hash_map< std::string, int > members;

	for (UINT u = 0; u < pTatt->cVars; u++)
	{
		vd = 0;
		HRESULT hr = typeinfo->GetVarDesc(u, &vd);
		if (SUCCEEDED(hr))
		{
			if (vd->varkind != VAR_CONST
				|| vd->lpvarValue->vt != VT_I4)
			{
				DebugOut("enum type not const/I4\n");
			}

			elementname.clear();

			hr = typeinfo->GetDocumentation(vd->memid, &bstrName, 0, 0, 0);
			if (SUCCEEDED(hr))
			{
				BSTR2String(bstrName, elementname);
				members.insert(std::pair< std::string, int >(elementname, vd->lpvarValue->intVal));
			}
			typeinfo->ReleaseVarDesc(vd);
		}
	}

	if (members.size() > 0) {

		int err;

		// create a new env for this enum
		SEXP e = PROTECT(R_tryEval(Rf_lang1(Rf_install("new.env")), R_GlobalEnv, &err));
		if (e)
		{
			// install this in the target (parent)
			Rf_defineVar(Rf_install(name.c_str()), e, targetEnv);

			// add members
			for (std::hash_map<std::string, int> ::iterator iter = members.begin(); iter != members.end(); iter++) {
				Rf_defineVar(Rf_install(iter->first.c_str()), Rf_ScalarInteger(iter->second), e);
			}

			UNPROTECT(1);
		}

	}

}

void mapEnums(ULONG_PTR p, SEXP targetEnv) {

	LPDISPATCH pdisp = (LPDISPATCH)p;

	CComPtr<ITypeInfo> spTypeInfo;
	CComPtr<ITypeLib> spTypeLib;

	HRESULT hr = pdisp->GetTypeInfo(0, 0, &spTypeInfo);

	std::string col;

	if (SUCCEEDED(hr) && spTypeInfo)
	{
		UINT tlidx;
		hr = spTypeInfo->GetContainingTypeLib(&spTypeLib, &tlidx);
	}

	if (SUCCEEDED(hr))
	{
		UINT tlcount = spTypeLib->GetTypeInfoCount();
		TYPEKIND tk;

		for (UINT u = 0; u < tlcount; u++)
		{
			if (SUCCEEDED(spTypeLib->GetTypeInfoType(u, &tk)))
			{
				if (tk == TKIND_ENUM)
				{
					CComBSTR bstrName;
					TYPEATTR *pTatt = nullptr;
					CComPtr<ITypeInfo> spTypeInfo2;
					hr = spTypeLib->GetTypeInfo(u, &spTypeInfo2);
					if (SUCCEEDED(hr)) hr = spTypeInfo2->GetTypeAttr(&pTatt);
					if (SUCCEEDED(hr)) hr = spTypeInfo2->GetDocumentation(-1, &bstrName, 0, 0, 0);
					if (SUCCEEDED(hr))
					{
						std::string tname;
						BSTR2String(bstrName, tname);
						mapEnum(tname, spTypeInfo2, pTatt, targetEnv);
					}
					if (pTatt) spTypeInfo2->ReleaseTypeAttr(pTatt);
				}
			}
		}
	}

}

void mapObject(IDispatch *Dispatch, std::vector< MemberRep > &mrlist, CComBSTR &bstrMatch)
{
	CComPtr<ITypeInfo> spTypeInfo;
	CComPtr<ITypeLib> spTypeLib;

	HRESULT hr = Dispatch->GetTypeInfo(0, 0, &spTypeInfo);

	// get type lib

	if (SUCCEEDED(hr) && spTypeInfo)
	{
		UINT tlidx;
		hr = spTypeInfo->GetContainingTypeLib(&spTypeLib, &tlidx);
	}

	if (SUCCEEDED(hr))
	{
		UINT tlcount = spTypeLib->GetTypeInfoCount();
		TYPEKIND tk;

		for (UINT u = 0; u < tlcount; u++)
		{
			if (SUCCEEDED(spTypeLib->GetTypeInfoType(u, &tk)))
			{
				CComBSTR bstrName;
				TYPEATTR *pTatt = nullptr;
				CComPtr<ITypeInfo> spTypeInfo2;
				hr = spTypeLib->GetTypeInfo(u, &spTypeInfo2);
				if (SUCCEEDED(hr)) hr = spTypeInfo2->GetTypeAttr(&pTatt);
				if (SUCCEEDED(hr)) hr = spTypeInfo2->GetDocumentation(-1, &bstrName, 0, 0, 0);

				if (SUCCEEDED(hr) && (bstrName == bstrMatch)) // CComBSTRs are magic
				{
					std::string tname;
					BSTR2String(bstrName, tname);


					std::string tmpstr;

					switch (tk)
					{
					case TKIND_ENUM:
					case TKIND_COCLASS:
						break;

					case TKIND_INTERFACE:
					case TKIND_DISPATCH:
						mapInterface(tname, mrlist, spTypeInfo2, spTypeLib, pTatt, tk);
						break;

						//break;

					default:
						DebugOut("Unexpected tkind: %d\n", tk);
						break;
					}

				}
				if (pTatt) spTypeInfo2->ReleaseTypeAttr(pTatt);
			}
		}
	}

}

bool getObjectInterface(CComBSTR &bstr, IDispatch *pdisp )
{
	UINT ui;
	bool rslt = false;
	CComPtr< ITypeInfo > typeinfo;

	HRESULT hr = pdisp->GetTypeInfoCount(&ui);
	if (SUCCEEDED(hr) && ui > 0)
	{
		hr = pdisp->GetTypeInfo(0, 1033, &typeinfo);
	}
	if (SUCCEEDED(hr))
	{
		hr = typeinfo->GetDocumentation(-1, &bstr, 0, 0, 0);
	}
	if (SUCCEEDED(hr))
	{
		rslt = true;
	}

	return rslt;
}

SEXP wrapDispatch(ULONG_PTR pdisp, bool enums) {

	SEXP result = 0;
	int err;

	std::vector< MemberRep > mrlist;
	CComBSTR bstrDescription, bstrIface;

	if (getObjectInterface(bstrIface, (LPDISPATCH)pdisp)) {
		mapObject((LPDISPATCH)pdisp, mrlist, bstrIface);

		CComPtr<ITypeInfo> pCoClass;
		if (getCoClassForDispatch(&pCoClass, (LPDISPATCH)pdisp)) {

			CComBSTR bstrName;
			CComBSTR bstrDoc;
			HRESULT hr = pCoClass->GetDocumentation(-1, &bstrName, &bstrDoc, 0, 0);
			if (SUCCEEDED(hr)) {

				std::string strcname;
				BSTR2String(bstrName, strcname);
				DebugOut(" * Coclass %s\n", strcname.c_str());

			}
			else {
				DebugOut(" * Coclass failed (2)\n");
			}
		}
		else {
			DebugOut(" * Coclass failed (1)\n");
		}

	}

	SEXP env = R_tryEvalSilent(Rf_lang2(Rf_install("get"), Rf_mkString(ENV_NAME)), R_GlobalEnv, &err);
	if (env)
	{
		SEXP wdargs;
		std::string strName;

		BSTR2String(bstrIface, strName);
		if (!strName.length()) strName = "IDispatch";

		PROTECT(wdargs = Rf_allocVector(VECSXP, 1));
		SET_VECTOR_ELT(wdargs, 0, Rf_mkString(strName.c_str()));
		PROTECT(result = R_tryEval(Rf_lang5(Rf_install("do.call"), Rf_mkString(".WrapDispatch"), wdargs, R_MissingArg, env), env, &err));
		if (result) {

#ifdef _DEBUG
			objectRefCtr++;
			DebugOut("[+] Object ref counter: %d\n", objectRefCtr);
#endif

			// install pointer
			((LPDISPATCH)pdisp)->AddRef();
			SEXP rptr = R_MakeExternalPtr((void*)pdisp, install("COM dispatch pointer"), R_NilValue);
			R_RegisterCFinalizerEx(rptr, (R_CFinalizer_t)releaseMappedPtr, TRUE);
			Rf_defineVar(Rf_install(".p"), rptr, result);

			// enums?
			if (enums) {
				mapEnums(pdisp, result);
			}

			// install function calls
			for (std::vector< MemberRep > ::iterator iter = mrlist.begin(); iter != mrlist.end(); iter++)
			{
				SEXP sargs, lns, ans = 0;

				std::string methodname = "";
				if (iter->mrflags == MRFLAG_PROPGET) methodname = "get_";
				else if (iter->mrflags == MRFLAG_PROPPUT) methodname = "put_";
				methodname += iter->name.c_str();

				SEXP argslist = 0;
				if (iter->args.size() > 0 ) {
					int alen = iter->args.size();
					PROTECT(argslist = Rf_allocVector(VECSXP, alen));
					for (int i = 0; i < alen; i++) {
						SET_VECTOR_ELT(argslist, i, Rf_mkString(iter->args[i].c_str()));
					}
				}

				// func.name, func.type, target.env

				PROTECT(sargs = Rf_allocVector(VECSXP, 4));
				SET_VECTOR_ELT(sargs, 0, Rf_mkString(methodname.c_str()));
				SET_VECTOR_ELT(sargs, 1, Rf_ScalarInteger(iter->mrflags));
				SET_VECTOR_ELT(sargs, 2, argslist ? argslist : R_MissingArg);
				SET_VECTOR_ELT(sargs, 3, result);
				lns = PROTECT(Rf_lang5(Rf_install("do.call"), Rf_mkString(".DefineCOMFunc"), sargs, R_MissingArg, env));
				PROTECT(ans = R_tryEval(lns, R_GlobalEnv, &err));
				UNPROTECT(3);

				if (argslist) { UNPROTECT(1); }


			}
			UNPROTECT(1);

		}
		UNPROTECT(1);

	}
	
	if (!result) result = Rf_ScalarLogical(0);
	return result;

}

__inline bool VT_NUMERIC(VARTYPE vt){

	vt &= (~VT_ARRAY);
	vt &= (~VT_BYREF);
	switch (vt) {
		case VT_I2:
		case VT_I4:
		case VT_R8:
		case VT_UI2:
		case VT_UI4:
		case VT_I8:
		case VT_UI8:
		case VT_INT:
		case VT_UINT:
			return true;
	}
	return false;
}

bool arrayIsNumeric(CComVariant &cv) {

	VARTYPE vt = cv.vt & (~VT_ARRAY);
	if (vt == VT_VARIANT) {

		bool numeric = true;
		UINT dim = SafeArrayGetDim(cv.parray);
		int count = 0;

		if (dim == 1) {
			long lbound, ubound;
			SafeArrayGetLBound(cv.parray, 1, &lbound);
			SafeArrayGetUBound(cv.parray, 1, &ubound);
			count = ubound - lbound + 1;
		}
		else if (dim == 2) {
			long lbound, ubound;
			SafeArrayGetLBound(cv.parray, 1, &lbound);
			SafeArrayGetUBound(cv.parray, 1, &ubound);
			count = ubound - lbound + 1;
			SafeArrayGetLBound(cv.parray, 2, &lbound);
			SafeArrayGetUBound(cv.parray, 2, &ubound);
			count *= (ubound - lbound + 1);
		}
		else return false; // not handled anyway

		if (count > 0) {

			VARIANT *pv;
			SafeArrayAccessData(cv.parray, (void**)&pv);
			for (int i = 0; i < count; i++) {
				if (!VT_NUMERIC(pv[i].vt) && pv[i].vt != VT_EMPTY && pv[i].vt != VT_NULL) {
					numeric = false;
					break;
				}
			}
			SafeArrayUnaccessData(cv.parray);

		}

		return numeric;

	}
	else return VT_NUMERIC(vt);
}

SEXP Variant2SEXP(CComVariant &cv) {

	VARTYPE vt = cv.vt & (~VT_BYREF);

	if (cv.vt & VT_ARRAY) {

		vt = cv.vt & (~VT_ARRAY);

		bool numeric = arrayIsNumeric(cv);
		int rows = 1, cols = 1;

		UINT dim = SafeArrayGetDim(cv.parray);
		if (dim == 1) {

			long lbound, ubound;
			SafeArrayGetLBound(cv.parray, 1, &lbound);
			SafeArrayGetUBound(cv.parray, 1, &ubound);
			rows = ubound - lbound + 1;

		}
		else if (dim == 2) {

			long lbound, ubound;
			SafeArrayGetLBound(cv.parray, 1, &lbound);
			SafeArrayGetUBound(cv.parray, 1, &ubound);
			rows = ubound - lbound + 1;

			SafeArrayGetLBound(cv.parray, 2, &lbound);
			SafeArrayGetUBound(cv.parray, 2, &ubound);
			cols = ubound - lbound + 1;

		}
		else { // >2 not handled

			return R_NilValue;

		}

		SEXP s = Rf_allocMatrix(VECSXP, rows, cols);
		int index = 0;

		switch (vt) {
		case VT_BSTR:
		{
			BSTR *pv;
			SafeArrayAccessData(cv.parray, (void**)&pv);
			for (int i = 0; i < cols; i++)
			{
				for (int j = 0; j < rows; j++)
				{
					CComBSTR bstr(pv[index]);
					std::string str;
					BSTR2String(bstr, str);
					SET_VECTOR_ELT(s, index, Rf_mkString(str.c_str()));
					index++; // watch out for optimizer
				}
			}
			SafeArrayUnaccessData(cv.parray);
		}
		break;
		case VT_INT:
		case VT_I4:
		case VT_I8:
		{
			int *pv;
			SafeArrayAccessData(cv.parray, (void**)&pv);
			for (int i = 0; i < cols; i++)
			{
				for (int j = 0; j < rows; j++)
				{
					SET_VECTOR_ELT(s, index, Rf_ScalarLogical(pv[index]));
					index++; // watch out for optimizer
				}
			}
			SafeArrayUnaccessData(cv.parray);
		}
		break;
		case VT_R4:
		case VT_R8:
		{
			double *pv;
			SafeArrayAccessData(cv.parray, (void**)&pv);
			for (int i = 0; i < cols; i++)
			{
				for (int j = 0; j < rows; j++)
				{
					SET_VECTOR_ELT(s, index, Rf_ScalarReal(pv[index]));
					index++; // watch out for optimizer
				}
			}
			SafeArrayUnaccessData(cv.parray);
		}
		break;
		case VT_VARIANT:
		{
			VARIANT *pv;
			SafeArrayAccessData(cv.parray, (void**)&pv);
			for (int i = 0; i < cols; i++)
			{
				for (int j = 0; j < rows; j++)
				{
					CComVariant cvinner(pv[index]);
					SEXP sinner = Variant2SEXP(cvinner);

					// this sets values to 0 when empty for a numeric-only matrix.  is that legit?
					// if (!sinner || sinner == R_NilValue) sinner = Rf_ScalarInteger(0);

					SET_VECTOR_ELT(s, index, sinner ? sinner : R_NilValue);
					index++; // watch out for optimizer
				}
			}
			SafeArrayUnaccessData(cv.parray);
		}
		break;
		}

		return s;
	}

	switch (vt) {
	case VT_EMPTY:
		return R_NilValue;
	case VT_NULL:
		return R_NilValue;
	case VT_R4:
	case VT_R8:
		return ScalarReal(cv.dblVal);
	case 8:
		{
			std::string str;
			CComBSTR bstr = cv.bstrVal;
			BSTR2String(bstr, str);
			return Rf_mkString(str.c_str());
		}
	case VT_DISPATCH:
		return wrapDispatch((ULONG_PTR)(cv.pdispVal));
	case VT_BOOL:
		return ScalarLogical(cv.boolVal);
	case VT_INT:
	case VT_I4:
	case VT_I8:
		return ScalarInteger(cv.intVal);
	case VT_UINT:
	case VT_UI4:
	case VT_UI8:
		return ScalarInteger(cv.uintVal);
	}

	return 0;

}

void formatCOMError(std::string &target, HRESULT hr, const char *msg, const char *symbol = 0)
{
	char szException[128];

	_com_error err(hr);
	LPCTSTR errmsg = err.ErrorMessage();

	std::string str;

	int len = _tcslen(errmsg);
	int slen = WideCharToMultiByte(CP_UTF8, 0, errmsg, len, 0, 0, 0, 0);
	char *s = new char[slen + 1];
	WideCharToMultiByte(CP_UTF8, 0, errmsg, len, s, slen, 0, 0);
	s[slen] = 0;
	str = s;
	delete[] s;

	target = msg ? msg : "COM Error";
	if (symbol)
	{
		target += " (";
		target += symbol;
		target += ")";
	}
	target += " ";

	sprintf_s(szException, "0x%x: ", hr);

	target += szException;
	target += str.c_str();

}

SEXP invokePropertyPut(std::string name, LPDISPATCH pdisp, SEXP value)
{
	if (!pdisp) {
		Rf_error("Invalid pointer value" );
	}

	CComBSTR wide;
	wide.Append(name.c_str());
	std::string errmsg;

	WCHAR *member = (LPWSTR)wide;
	DISPID dispid;

	HRESULT hr = pdisp->GetIDsOfNames(IID_NULL, &member, 1, 1033, &dispid);
	
	if (FAILED(hr)) {
		Rf_error("Name not found");
	}

	DISPPARAMS dispparams;
	DISPID dispidNamed = DISPID_PROPERTYPUT;

	dispparams.cArgs = 1;
	dispparams.cNamedArgs = 1;
	dispparams.rgdispidNamedArgs = &dispidNamed;

	CComVariant cv;
	CComBSTR bstr;

	SEXP arg = value;
	int t;

	// wrapped in an outside list to pass it in
	if ((Rf_length(arg) == 1) && (TYPEOF(arg) == VECSXP)) {
		arg = VECTOR_ELT(value, 0);
	}

	// now check this element -- could be a list
	if (Rf_length(arg) <= 1) {

		t = TYPEOF(value);
		if (t == VECSXP) {
			arg = VECTOR_ELT(value, 0);
			t = TYPEOF(arg);

		}

		if (Rf_isLogical(arg)) cv = (bool)(INTEGER(arg)[0] != 0);
		else if (Rf_isInteger(arg)) cv = INTEGER(arg)[0];
		else if (Rf_isNumeric(arg)) cv = REAL(arg)[0];
		else if (Rf_isString(arg)) {

			STRSXP2BSTR(bstr, arg);
			//cv.vt = VT_BSTR | VT_BYREF;
			//cv.pbstrVal = &(bstr);
			cv.vt = VT_BSTR;
			cv.bstrVal = bstr;

		}

		dispparams.rgvarg = &cv;
		hr = pdisp->Invoke(dispid, IID_NULL, 1033, DISPATCH_PROPERTYPUT, &dispparams, NULL, NULL, NULL);

	}
	else {

		int nr = Rf_nrows(arg);
		int nc = Rf_ncols(arg);

		SAFEARRAYBOUND sab[2];
		sab[0].cElements = nr;
		sab[0].lLbound = 0;
		sab[1].cElements = nc;
		sab[1].lLbound = 0;

		CComSafeArray<VARIANT> cc;
		cc.Create(sab, 2);
		std::vector<CComBSTR*> bv;

		VARIANT *pv;
		SafeArrayAccessData(cc, (void**)&pv);

		int index = 0;
		int len = nr * nc;

		t = TYPEOF(arg);

		if (t == INTSXP) {
			int *src = INTEGER(arg);
			for (int i = 0; i < len; i++) { pv[i].vt = VT_INT; pv[i].intVal = src[i]; }
		}
		else if (t == REALSXP) {
			double *src = REAL(arg);
			for (int i = 0; i < len; i++) { 
				pv[i].vt = VT_R8; 
				pv[i].dblVal = src[i]; 
			}
		}
		else if (t == LGLSXP) {
			int *src = INTEGER(arg);
			for (int i = 0; i < len; i++) { pv[i].vt = VT_BOOL; pv[i].boolVal = (bool)(src[i] != 0); }
		}
		else if (t == STRSXP) {
			for (int i = 0; i < len; i++) {
				//CComBSTR *bstr = new CComBSTR(CHAR(STRING_ELT(arg, i)));
				CComBSTR *bstr = new CComBSTR();
				STRSXP2BSTR(*bstr, STRING_ELT(arg, i));
				pv[i].vt = VT_BSTR | VT_BYREF;
				pv[i].pbstrVal = &(bstr->m_str);
				bv.push_back(bstr);
			}
		}
		else if (t == VECSXP) {
			for (int i = 0; i < len; i++){
				SEXP x = VECTOR_ELT(arg, i);
				if (Rf_isLogical(x)) { pv[i].vt = VT_BOOL; pv[i].boolVal = (bool)(INTEGER(x)[0] != 0); }
				else if (Rf_isInteger(x)) { pv[i].vt = VT_INT; pv[i].intVal = INTEGER(x)[0]; }
				else if (Rf_isNumeric(x)) { pv[i].vt = VT_R8; pv[i].dblVal = REAL(x)[0]; }
				else if (Rf_isString(x)) {

					CComBSTR *bstr = new CComBSTR();
					int tlen = Rf_length(x);
					STRSXP2BSTR(*bstr, tlen == 1 ? x : STRING_ELT(x, 0));
					pv[i].vt = VT_BSTR | VT_BYREF;
					pv[i].pbstrVal = &(bstr->m_str);
					bv.push_back(bstr);
				}
			}
		}

		SafeArrayUnaccessData(cc);

		cv = cc;
		dispparams.rgvarg = &cv;
		hr = pdisp->Invoke(dispid, IID_NULL, 1033, DISPATCH_PROPERTYPUT, &dispparams, NULL, NULL, NULL);

		// clean up strings
		for (std::vector< CComBSTR* > ::iterator iter = bv.begin(); iter != bv.end(); iter++) delete(*iter);

		// and array
		cc.Destroy();

	}


	if (FAILED(hr))
	{
		formatCOMError(errmsg, hr, 0, name.c_str());
	}

	if (errmsg.length()) Rf_error(errmsg.c_str());
	return Rf_ScalarLogical(1);
}

SEXP invokeFunc(std::string name, LPDISPATCH pdisp, SEXP args)
{
	if (!pdisp) {
		Rf_error("Invalid pointer value");
	}

	SEXP result = 0;
	std::string errmsg;

	CComBSTR wide;
	wide.Append(name.c_str());

	WCHAR *member = (LPWSTR)wide;
	DISPID dispid;

	HRESULT hr = pdisp->GetIDsOfNames(IID_NULL, &member, 1, 1033, &dispid);

	if (FAILED(hr)) {
		Rf_error("Name not found");
	}

	CComPtr<ITypeInfo> spTypeInfo;
	hr = pdisp->GetTypeInfo(0, 0, &spTypeInfo);
	bool found = false;

	// FIXME: no need to iterate, you can use GetIDsOfNames from ITypeInfo 
	// to get the memid, then use that to get the funcdesc.  much more efficient.
	// ... how does that deal with propget/propput?

	if (SUCCEEDED(hr) && spTypeInfo)
	{
		TYPEATTR *pTatt = nullptr;
		hr = spTypeInfo->GetTypeAttr(&pTatt);
		if (SUCCEEDED(hr) && pTatt)
		{
			FUNCDESC * fd = nullptr;
			for (int i = 0; !found && i < pTatt->cFuncs; ++i)
			{
				hr = spTypeInfo->GetFuncDesc(i, &fd);
				if (SUCCEEDED(hr) && fd)
				{
					if (dispid == fd->memid
						&& (fd->invkind == INVOKEKIND::INVOKE_FUNC
							|| fd->invkind == INVOKEKIND::INVOKE_PROPERTYGET))
					{
						UINT namecount;
						CComBSTR bstrNameList[32];
						spTypeInfo->GetNames(fd->memid, (BSTR*)&bstrNameList, 32, &namecount);

						found = true;

						// this makes no sense.  it's based on what the call wants, not what the caller does.

						if (fd->invkind == INVOKE_FUNC || ((fd->invkind == INVOKE_PROPERTYGET) && (fd->cParams - fd->cParamsOpt > 0)))
						{

							DISPPARAMS dispparams;
							dispparams.cArgs = 0;
							dispparams.cNamedArgs = 0;

							CComVariant cvResult;

							HRESULT hr;
							int arglen = Rf_length(args);

							dispparams.cArgs = 0; // arglen;
							int type = TYPEOF(args);

							CComVariant *pcv = 0;
							CComBSTR *pbstr = 0;

							if (arglen > 0)
							{
								pcv = new CComVariant[arglen];
								pbstr = new CComBSTR[arglen];

								dispparams.rgvarg = pcv;

								for (int i = 0; i < arglen; i++)
								{
									int k = arglen - 1 - i;
									// int k = i;
									SEXP arg = VECTOR_ELT(args, i);

									if (Rf_isLogical(arg)) pcv[k] = (bool)(INTEGER(arg)[0]);
									else if (Rf_isInteger(arg)) pcv[k] = INTEGER(arg)[0];
									else if (Rf_isNumeric(arg)) pcv[k] = REAL(arg)[0];
									else if (Rf_isString(arg)) {

										int tlen = Rf_length(arg);
										STRSXP2BSTR(pbstr[k], tlen == 1 ? arg : STRING_ELT(arg, 0));

											//const char *sz = CHAR(STRING_ELT(arg, 0));
									//pbstr[k] = sz;
										// STRSXP2BSTR(pbstr[k], STRING_ELT(arg, 0));
										pcv[k].vt = VT_BSTR | VT_BYREF;
										pcv[k].pbstrVal = &(pbstr[k]);
									}
									else if (Rf_isEnvironment(arg) && Rf_inherits( arg, "IDispatch" )){
										SEXP disp = Rf_findVar(Rf_install(".p"), arg);
										if (disp) {
											pcv[k].vt = VT_DISPATCH;
											pcv[k].pdispVal = SEXP2Ptr(disp);
											if (pcv[k].pdispVal) pcv[k].pdispVal->AddRef(); // !
										}
									}
									else {

										int tx = TYPEOF(arg);
										DebugOut("Unhandled argument type; tx = %d\n", tx);

										// equivalent to "missing"
										pcv[k] = CComVariant(DISP_E_PARAMNOTFOUND, VT_ERROR);
									}
									dispparams.cArgs++;
											
								}
							}

							hr = pdisp->Invoke(dispid, IID_NULL, 1033, (fd->invkind == INVOKE_PROPERTYGET) ? DISPATCH_PROPERTYGET : DISPATCH_METHOD, &dispparams, &cvResult, NULL, NULL);

							if (pcv) delete[] pcv;
							if (pbstr) delete[] pbstr;
							
							if (FAILED(hr))
							{
								formatCOMError(errmsg, hr, "COM Exception in Invoke", name.c_str());
							}
							else
							{
								result = Variant2SEXP(cvResult);
							}

						}
						else if (fd->invkind == INVOKE_PROPERTYGET)
						{

							DISPPARAMS dispparams;
							dispparams.cArgs = 0;
							dispparams.cNamedArgs = 0;

							CComVariant cvResult;

							HRESULT hr = pdisp->Invoke(dispid, IID_NULL, 1033, DISPATCH_PROPERTYGET, &dispparams, &cvResult, NULL, NULL);

							if (SUCCEEDED(hr))
							{
								result = Variant2SEXP(cvResult);
							}
							else
							{
								formatCOMError(errmsg, hr, "COM Exception in Invoke", name.c_str());
							}

						}
					}

					spTypeInfo->ReleaseFuncDesc(fd);
				}
			}

			spTypeInfo->ReleaseTypeAttr(pTatt);
		}
	}

	if (errmsg.length()) Rf_error(errmsg.c_str());
	return result ? result : R_NilValue;

}


SEXP COMPropPut(SEXP name, SEXP p, SEXP args) {

	const char *funcname = CHAR(STRING_ELT(name, 0));
	return invokePropertyPut(funcname + 4, SEXP2Ptr(p), args);

}

SEXP COMPropGet(SEXP name, SEXP p, SEXP args) {

	const char *funcname = CHAR(STRING_ELT(name, 0));
	return invokeFunc(funcname + 4, SEXP2Ptr(p), args);

}

SEXP COMFunc(SEXP name, SEXP p, SEXP args) {

	const char *funcname = CHAR(STRING_ELT(name, 0));
	return invokeFunc(funcname, SEXP2Ptr(p), args);

}
