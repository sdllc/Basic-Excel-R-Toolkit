/*
* Basic Excel R Toolkit (BERT)
* Copyright (C) 2014 Structured Data, LLC
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

// these are generated type library headers.  they were originally created
// using #import statements; then the files were added to the project and 
// the #import statements removed.

// this should be less fragile than using #import as a preprocessor step.

#include "MSO.tlh"
#include "Excel.tlh"

IDispatch *pdispThread = 0;
IDispatch *pdispApp = 0;

IStream *pstream = 0;

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
	CComQIPtr< Excel::_Application > application = pdispThread;
	if (application)
	{
		CComVariant cvFunc, cvRslt, cvArg;
		CComVariant cvCmdID = 0L;
		CComBSTR bstrCmd;
		HRESULT hr = S_OK;

		switch (cmd)
		{
		case SCC_EXEC:
			{
				cvFunc = "BERT.SafeCall";

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

				cvArg = (LPSAFEARRAY)cc;

				hr = application->_Run2(cvFunc, cvCmdID, cvArg,
					vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing,
					vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing,
					vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing,
					1033, &cvRslt);

				cc.Destroy();
			}
			break;

		case SCC_CALLTIP:
			cvFunc = "BERT.SafeCall";
			cvArg = vec->begin()->c_str();
			hr = application->_Run2(cvFunc, cvCmdID, cvArg,
				vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing,
				vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing,
				vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing,
				1033, &cvRslt);
			break;

		case SCC_NAMES:
			cvFunc = "BERT.SafeCall";
			cvCmdID = 2; // FIXME: ENUM
			cvArg = vec->begin()->c_str();
			hr = application->_Run2(cvFunc, cvCmdID, cvArg,
				vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing,
				vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing,
				vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing,
				1033, &cvRslt);
			break;

		case SCC_INSTALLPACKAGES:
			bstrCmd = "BERT.InstallPackages";
			cvFunc = bstrCmd;
			hr = application->_Run2(cvFunc, cvCmdID,
				vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing,
				vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing,
				vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing,
				1033, &cvRslt);
			break;

		case SCC_RELOAD_STARTUP:
			bstrCmd = "BERT.Reload";
			cvFunc = bstrCmd;
			hr = application->_Run2(cvFunc, cvCmdID,
				vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing,
				vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing,
				vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing, vtMissing,
				1033, &cvRslt);
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

		return hr;
	}
	return E_FAIL;
}

/**
 *
 */
HRESULT ReleaseThreadPtr()
{
	HRESULT hr = S_OK;
	if (pdispThread)
	{
		hr = pdispThread->Release();
		pdispThread = 0;
	}
	return hr;
}

/**
 *
 */
void SetExcelPtr(LPVOID p)
{
	pdispApp = (IDispatch*)p;
}

/**
 *
 */
void FreeStream()
{
	if (pstream)
	{
		AtlFreeMarshalStream(pstream);
	}
	pstream = 0;
}

/**
 *
 */
HRESULT Unmarshal()
{
	HRESULT hr = AtlUnmarshalPtr(pstream, IID_IDispatch, (IUnknown**)&pdispThread);
	return hr;
}

/**
 *
 */
HRESULT Marshal()
{
	if (!pdispApp) return E_FAIL;
	if (pstream) return S_OK; // only once
	return AtlMarshalPtrInProc(pdispApp, IID_IDispatch, &pstream);
}


