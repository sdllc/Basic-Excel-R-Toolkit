// Connect.cpp : Implementation of CConnect
#include "stdafx.h"
#include "BERTRibbon2_i.h"
#include "Connect.h"
#include "../BERT/RegistryUtils.h"
#include "../BERT/RegistryConstants.h"

extern CBERTRibbon2Module _AtlModule;

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
