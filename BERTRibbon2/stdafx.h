// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

#ifndef STRICT
#define STRICT
#endif

// Modify the following defines if you have to target a platform prior to the ones specified below.
// Refer to MSDN for the latest info on corresponding values for different platforms.
#ifndef WINVER				// Allow use of features specific to Windows 95 and Windows NT 4 or later.
#define WINVER 0x0600		// Change this to the appropriate value to target Windows 98 and Windows 2000 or later.
#endif

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows NT 4 or later.
//#define _WIN32_WINNT 0x0400	// Change this to the appropriate value to target Windows 2000 or later.
#define _WIN32_WINNT 0x0600 // Change this to the appropriate value to target Windows 2000 or later.
#endif						

#ifndef _WIN32_WINDOWS		// Allow use of features specific to Windows 98 or later.
//#define _WIN32_WINDOWS 0x0410 // Change this to the appropriate value to target Windows Me or later.
#define _WIN32_WINDOWS 0x0600// Change this to the appropriate value to target Windows Me or later.
#endif

#ifndef _WIN32_IE			// Allow use of features specific to IE 4.0 or later.
#define _WIN32_IE 0x0400	// Change this to the appropriate value to target IE 5.0 or later.
#endif

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit

// turns off ATL's hiding of some common and often safely ignored warning messages
#define _ATL_ALL_WARNINGS


#include "resource.h"
#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>

#include <Psapi.h>
#include <string>

#pragma warning( disable : 4278 )
#pragma warning( disable : 4146 )
//The following #import imports the IDTExtensibility2 interface based on it's LIBID
#import "libid:AC0714F2-3D04-11D1-AE7D-00A0C90F26F4" version("1.0") lcid("0")  raw_interfaces_only named_guids

#pragma warning( default : 4146 )
#pragma warning( default : 4278 )

#define IfFailGo(x) { hr=(x); if (FAILED(hr)) goto Error; }
#define IfFailGoCheck(x, p) { hr=(x); if (FAILED(hr)) goto Error; if(!p) {hr = E_FAIL; goto Error; } }

class DECLSPEC_UUID("BB49688B-03E5-4703-8175-99CF97125C80") BERTRibbon2Lib;

using namespace ATL;

class CBERTRibbon2Module : public CAtlDllModuleT< CBERTRibbon2Module >
{
public:
	CBERTRibbon2Module()
	{
		m_hInstance = NULL;
	}

	DECLARE_LIBID(__uuidof(BERTRibbon2Lib))

	inline HINSTANCE GetResourceInstance()
	{
		return m_hInstance;
	}

	inline void SetResourceInstance(HINSTANCE hInstance)
	{
		m_hInstance = hInstance;
	}

private:
	HINSTANCE m_hInstance;
};

extern CBERTRibbon2Module _AtlModule;

// @see BERTCOM.cpp about these files

#include "..\BERT\mso-15.tlh"
using namespace Office;

#include "..\BERT\excel-15.tlh"




