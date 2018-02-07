
#include "stdafx.h"
#include "BERTRibbon2_i.h"
#include "ribbon_connect.h"

extern CBERT2RibbonModule _AtlModule;

bool GetRegistryString(std::string &result_value, HKEY base_key, const char *key, const char *name) {
    bool succeeded = false;
    char buffer[MAX_PATH];
    DWORD data_size = MAX_PATH;
    LSTATUS status = RegGetValueA(base_key, key, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, 0, buffer, &data_size);
    if (!status && data_size > 0) result_value.assign(buffer, data_size - 1);
    return (status == 0);
}

STDMETHODIMP CConnect::GetCustomUI(BSTR RibbonID, BSTR *pbstrRibbonXML)
{
	HRSRC hRsrc = ::FindResource(_AtlBaseModule.m_hInstResource, MAKEINTRESOURCE(IDR_XML2), L"XML");
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
    const char path_separator[] = "\\";

    std::string xll_path;
    GetRegistryString(xll_path, HKEY_CURRENT_USER, "Software\\BERT", "BERT2.XLLDir");

    if (xll_path.length() && xll_path.compare(xll_path.length() - 1, 1, path_separator)) {
        xll_path.append(path_separator);
    }

#ifdef _DEBUG 
#ifdef _WIN64
    xll_path.append("BERT64D.xll");
#else
    xll_path.append("BERT32D.xll");
#endif
#else
#ifdef _WIN64
    xll_path.append("BERT64.xll");
#else
    xll_path.append("BERT32.xll");
#endif
#endif
    
	pApplication->QueryInterface(__uuidof(IDispatch), (LPVOID*)&m_pApplication);
	pAddInInst->QueryInterface(__uuidof(IDispatch), (LPVOID*)&m_pAddInInstance);

	CComQIPtr<Excel::_Application> app(m_pApplication);
	if (app){

        int result = SetPointers(reinterpret_cast<ULONG_PTR>(m_pApplication.p), reinterpret_cast<ULONG_PTR>(this));
        if( result ){

			_bstr_t bstrPath(xll_path.c_str());
			VARIANT_BOOL vb = VARIANT_FALSE;
			HRESULT hr = app->RegisterXLL(bstrPath, 1033, &vb);

			if (SUCCEEDED(hr)){
				if (vb) {
                    result = SetPointers(reinterpret_cast<ULONG_PTR>(m_pApplication.p), reinterpret_cast<ULONG_PTR>(this));
                    // ATLTRACE("register loaded xll OK");
				}
				else {
					ATLTRACE("register call succeeded but load returned false\n");
				}
			}
			else {
				ATLTRACE("register failed with 0x%x\n", hr);
			}
		}
	}
    
	return S_OK;
}

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
