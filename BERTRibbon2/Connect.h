
#pragma once

#include "resource.h"       // main symbols

typedef IDispatchImpl<IRibbonExtensibility, &__uuidof(IRibbonExtensibility), &LIBID_Office, /* wMajor = */ 2, /* wMinor = */ 4> RIBBON_INTERFACE;

// CConnect
class ATL_NO_VTABLE CConnect :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CConnect, &CLSID_Connect>,
	public IDispatchImpl<AddInDesignerObjects::_IDTExtensibility2, &AddInDesignerObjects::IID__IDTExtensibility2, &AddInDesignerObjects::LIBID_AddInDesignerObjects, 1, 0>,
	public RIBBON_INTERFACE
{
public:
	CConnect()
	{
	}

	DECLARE_REGISTRY_RESOURCEID(IDR_BERTRIBBON2)
	DECLARE_NOT_AGGREGATABLE(CConnect)

	BEGIN_COM_MAP(CConnect)
		COM_INTERFACE_ENTRY2(IDispatch, IRibbonExtensibility)
		COM_INTERFACE_ENTRY(AddInDesignerObjects::IDTExtensibility2)
		COM_INTERFACE_ENTRY(IRibbonExtensibility)
	END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

public:

public:
	//IDTExtensibility2 implementation:
	STDMETHOD(OnConnection)(IDispatch * Application, AddInDesignerObjects::ext_ConnectMode ConnectMode, IDispatch *AddInInst, SAFEARRAY **custom);
	STDMETHOD(OnDisconnection)(AddInDesignerObjects::ext_DisconnectMode RemoveMode, SAFEARRAY **custom);
	STDMETHOD(OnAddInsUpdate)(SAFEARRAY **custom);
	STDMETHOD(OnStartupComplete)(SAFEARRAY **custom);
	STDMETHOD(OnBeginShutdown)(SAFEARRAY **custom);

	CComPtr<IDispatch> m_pApplication;
	CComPtr<IDispatch> m_pAddInInstance;
	CComQIPtr<IRibbonUI> m_pRibbonUI;

	// IRibbonExtensibility Methods
	STDMETHOD(GetCustomUI)(BSTR RibbonID, BSTR *pbstrRibbonXML)
	{
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

	// IDispatch methods
	STDMETHOD(GetIDsOfNames)(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid)
	{

		int dispID = -1;
		if (cNames > 0)
		{
			if (!wcscmp(rgszNames[0], L"BERTRIBBON_BERT_Console"))				dispID = 1001;
			else if (!wcscmp(rgszNames[0], L"BERTRIBBON_BERT_Home"))			dispID = 1002;
			else if (!wcscmp(rgszNames[0], L"BERTRIBBON_BERT_Reload"))			dispID = 1003;
			else if (!wcscmp(rgszNames[0], L"BERTRIBBON_BERT_InstallPackages")) dispID = 1004;
			else if (!wcscmp(rgszNames[0], L"BERTRIBBON_BERT_Configure"))		dispID = 1005;
			else if (!wcscmp(rgszNames[0], L"BERTRIBBON_BERT_About"))			dispID = 1006;
			else if (!wcscmp(rgszNames[0], L"BERTRIBBON_Ribbon_Loaded"))		dispID = 2000;
		}
		if (dispID > 0)
		{
			rgdispid[0] = dispID;
			return S_OK;
		}

		return RIBBON_INTERFACE::GetIDsOfNames(riid, rgszNames, cNames, lcid, rgdispid);
	}

	STDMETHOD(Invoke)(DISPID dispidMember, REFIID riid,
		LCID lcid, WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult,
		EXCEPINFO* pexcepinfo, UINT* puArgErr)
	{
		switch (dispidMember)
		{
		case 1001:
			HandleControlInvocation("BERT.Console()");
			return S_OK;
		case 1002:
			HandleControlInvocation("BERT.Home()");
			return S_OK;
		case 1003:
			HandleControlInvocation("BERT.Reload()");
			return S_OK;
		case 1004:
			HandleControlInvocation("BERT.InstallPackages()");
			return S_OK;
		case 1005:
			HandleControlInvocation("BERT.Configure()");
			return S_OK;
		case 1006:
			HandleControlInvocation("BERT.About()");
			return S_OK;
		case 2000:
			return HandleCacheRibbon(pdispparams);
		}
		return RIBBON_INTERFACE::Invoke(dispidMember, riid, lcid,
			wFlags, pdispparams, pvarResult, pexcepinfo, puArgErr);
	}

	// Local methods

	HRESULT HandleCacheRibbon(DISPPARAMS* pdispparams)
	{
		if (pdispparams->cArgs > 0
			&& pdispparams->rgvarg[0].vt == VT_DISPATCH)
		{
			m_pRibbonUI = pdispparams->rgvarg[0].pdispVal;
			return S_OK;
		}
		return E_FAIL;
	}

	HRESULT HandleControlInvocation(const char *szCmd)
	{
		CComQIPtr< Excel::_Application > pApp = m_pApplication;
		if (pApp)
		{
			CComBSTR bstrCmd(szCmd);
			CComVariant cvRslt;
			HRESULT hr = pApp->ExecuteExcel4Macro(bstrCmd, 1033, &cvRslt);

		}
		return S_OK;
	}

};

OBJECT_ENTRY_AUTO(__uuidof(Connect), CConnect)
