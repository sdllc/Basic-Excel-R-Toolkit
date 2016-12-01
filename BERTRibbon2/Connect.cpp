// Connect.cpp : Implementation of CConnect
#include "stdafx.h"
#include "BERTRibbon2_i.h"
#include "Connect.h"
#include "../BERT/RegistryUtils.h"
#include "../BERT/RegistryConstants.h"

extern CBERTRibbon2Module _AtlModule;

STDMETHODIMP CConnect::GetCustomUI(BSTR RibbonID, BSTR *pbstrRibbonXML)
{
	// prior to valid locales, we will support a locale/dev folder
	// FIXME: require a registry entry to support this? 
	
	// or is that too far, considering you have to create this directory in the first place, 
	// and if you have directory access you essentially have registry access as well?

	char buffer[MAX_PATH];
	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, buffer, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_INSTALL_DIR))
		sprintf_s(buffer, MAX_PATH, "");

	int len = strlen(buffer);
	if (len > 0) if (buffer[len - 1] != '\\') strcat_s(buffer, MAX_PATH - 1, "\\");

	LCID lcid = ::GetUserDefaultLCID();
	std::string localeStr;
	WCHAR wstrNameBuffer[LOCALE_NAME_MAX_LENGTH];
	DWORD error = ERROR_SUCCESS;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	char strLocale[LOCALE_NAME_MAX_LENGTH] = { 0 };

	// FIXME: allow user override -- registry?

	int lclen = ::LCIDToLocaleName(lcid, wstrNameBuffer, LOCALE_NAME_MAX_LENGTH, 0);
	if (lclen > 1) // returned length includes trailing null
	{
		for (int i = 0; i < lclen; i++) {
			strLocale[i] = wstrNameBuffer[i] & 0xff;
		}

		// set env here, so we can use it in shell, don't need to check in app
		::SetEnvironmentVariableA("BERT_LOCALE_ID", strLocale);
	}

	// check for dev template first.  this always overrides.

	{
		std::string sdir = buffer;
		sdir += "locale\\dev\\ribbon-menu-template.xml";
		hFile = ::CreateFileA(sdir.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	}

	// if no dev, and we have locale, check that

	if( hFile == INVALID_HANDLE_VALUE && strLocale[0] ){
		std::string sdir = buffer;
		sdir += "locale\\";
		sdir += strLocale;
		sdir += "\\ribbon-menu-template.xml";
		hFile = ::CreateFileA(sdir.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	}

	// if either locale or dev are available, read and exit
	
	if (hFile != INVALID_HANDLE_VALUE) {

		std::string strContents;
		DWORD dwRead = 0;
		DWORD dwBuffer;
		DWORD dwPtr = 0;
		while (::ReadFile(hFile, buffer, MAX_PATH-1, &dwRead, 0)){
			if (dwRead > 0) {
				buffer[dwRead] = 0;
				strContents += buffer;
			}
			else break;
		}
		::CloseHandle(hFile);
		if (strContents.length() > 0) {
			int req = ::MultiByteToWideChar(CP_UTF8, 0, strContents.c_str(), strContents.length(), 0, 0);
			if (req > 0) {
				WCHAR *wsz = new WCHAR[req + 1];
				::MultiByteToWideChar(CP_UTF8, 0, strContents.c_str(), strContents.length(), wsz, req);
				wsz[req] = 0;
				CComBSTR bstrXML = wsz;
				delete[] wsz;
				*pbstrRibbonXML = bstrXML.Detach();
				return S_OK;
			}
		}
	}
	else {
		DWORD dwErr = ::GetLastError();
		ATLTRACE(L"Read err: %d\n", dwErr);
	}

	// at this point nothing worked. default to resource.  

	HRSRC hRsrc = ::FindResource(_AtlBaseModule.m_hInstResource, MAKEINTRESOURCE(IDR_XML1), L"XML");
	if (hRsrc)
	{
		DWORD dwLen = SizeofResource(_AtlBaseModule.m_hInstResource, hRsrc);
		if (dwLen > 0)
		{
			HGLOBAL hGbl = LoadResource(_AtlBaseModule.m_hInstResource, hRsrc);
			if (hGbl)
			{
				LPVOID pData = LockResource(hGbl);
				if (pData)
				{
					CComBSTR bstrData(dwLen, (char*)pData);
					UnlockResource(hGbl);
					*pbstrRibbonXML = bstrData.Detach();
				}
			}
		}
	}
	return S_OK;
}

// CConnect
STDMETHODIMP CConnect::OnConnection(IDispatch *pApplication, AddInDesignerObjects::ext_ConnectMode /*ConnectMode*/, IDispatch *pAddInInst, SAFEARRAY ** /*custom*/)
{
	char BERTXLL[32] = "";
	
	char RBin[MAX_PATH];
	char Home[MAX_PATH];

	char Install[MAX_PATH];
	char XLLPath[MAX_PATH];

	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, RBin, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_HOME, true))
		ExpandEnvironmentStringsA(DEFAULT_R_HOME, RBin, MAX_PATH - 1);

	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, Home, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_USER, true))
		ExpandEnvironmentStringsA(DEFAULT_R_USER, Home, MAX_PATH - 1);

	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, Install, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_INSTALL_DIR))
		sprintf_s(Install, MAX_PATH, "");

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

	char *buffer = new char[blen + elen + 1];
	strcpy_s(buffer, blen + elen + 1, RBin);
	if (elen > 0)
	{
		::GetEnvironmentVariableA("PATH", &(buffer[blen]), elen);
	}

	::SetEnvironmentVariableA("PATH", buffer);

	// load xll

#ifdef _DEBUG 
#ifdef _WIN64
	sprintf_s(BERTXLL, 32, "BERT64D.xll");
#else
	sprintf_s(BERTXLL, 32, "BERT32D.xll");
#endif
#else
#ifdef _WIN64
	sprintf_s(BERTXLL, 32, "BERT64.xll");
#else
	sprintf_s(BERTXLL, 32, "BERT32.xll");
#endif
#endif

	pApplication->QueryInterface(__uuidof(IDispatch), (LPVOID*)&m_pApplication);
	pAddInInst->QueryInterface(__uuidof(IDispatch), (LPVOID*)&m_pAddInInstance);

	::PathAddBackslashA(Install);
	::PathCombineA(XLLPath, Install, BERTXLL);

	CComQIPtr<Excel::_Application> app(m_pApplication);
	if (app){

		int rslt = SetCOMPtrs((void*)m_pApplication.p, (void*)this);
		if (!rslt){
			ATLTRACE("Already registered\n");
		}
		else if (rslt && strlen(XLLPath))
		{
			_bstr_t bstrPath(XLLPath);
			VARIANT_BOOL vb = VARIANT_FALSE;
			HRESULT hr = app->RegisterXLL(bstrPath, 1033, &vb);

			if (SUCCEEDED(hr)){

				if (vb) {
					rslt = SetCOMPtrs((void*)m_pApplication.p, (void*)this);
					ATLTRACE("Loaded xll OK");
				}
				else {
					ATLTRACE("Succeeded but load returned false\n");
				}
			}
			else {
				ATLTRACE("Failed with 0x%x\n", hr);
			}
		}
	}

	// restore original path.  note that R apparently caches environment 
	// variables so (at least at first) the PATH variable will include 
	// the BERT path.  since this is local to BERT, it shouldn't be a problem.
	// however if necessary we can explicitly fix the R value.

	::SetEnvironmentVariableA("PATH", buffer + blen); 

	delete[] buffer;

	return S_OK;
}

//////

STDMETHODIMP CConnect::OnDisconnection(AddInDesignerObjects::ext_DisconnectMode /*RemoveMode*/, SAFEARRAY ** /*custom*/)
{
	m_pApplication = NULL;
	m_pAddInInstance = NULL;
	if (m_pRibbonUI) m_pRibbonUI.Release();
	return S_OK;
}

STDMETHODIMP CConnect::OnAddInsUpdate(SAFEARRAY ** /*custom*/)
{
	return S_OK;
}

STDMETHODIMP CConnect::OnStartupComplete(SAFEARRAY ** /*custom*/)
{
	return S_OK;
}

STDMETHODIMP CConnect::OnBeginShutdown(SAFEARRAY ** /*custom*/)
{
	return S_OK;
}
