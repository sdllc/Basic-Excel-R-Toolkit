
#pragma once

#include "resource.h"       // main symbols

#include <vector>

typedef IDispatchImpl<IRibbonExtensibility, &__uuidof(IRibbonExtensibility), &LIBID_Office, /* wMajor = */ 2, /* wMinor = */ 4> RIBBON_INTERFACE;

typedef int (*SPPROC)(LPVOID);

class UserButtonInfo {
public:
	std::string strLabel;
	std::string strFunction;
	int iImageIndex;

	UserButtonInfo() {};
	UserButtonInfo(const UserButtonInfo &rhs) {
		strLabel = rhs.strLabel;
		strFunction = rhs.strFunction;
		iImageIndex = rhs.iImageIndex;
	}
};

typedef std::vector < UserButtonInfo > UBVECTOR;

#define DISPID_USER_BUTTONS_VISIBLE		3000
#define DISPID_USER_BUTTON_VISIBLE		3001
#define DISPID_USER_BUTTON_LABEL		3002
#define DISPID_USER_BUTTON_ACTION		3004

// CConnect
class ATL_NO_VTABLE CConnect :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CConnect, &CLSID_Connect>,
	public IDispatchImpl<AddInDesignerObjects::_IDTExtensibility2, &AddInDesignerObjects::IID__IDTExtensibility2, &AddInDesignerObjects::LIBID_AddInDesignerObjects, 1, 0>,
	public RIBBON_INTERFACE
{
public:
	CConnect() : m_pApplication(0)
	{
	}

	DECLARE_REGISTRY_RESOURCEID(IDR_BERTRIBBON2)
	DECLARE_NOT_AGGREGATABLE(CConnect)

	BEGIN_COM_MAP(CConnect)
		COM_INTERFACE_ENTRY2(IDispatch, IRibbonExtensibility)
		COM_INTERFACE_ENTRY(AddInDesignerObjects::_IDTExtensibility2)
		COM_INTERFACE_ENTRY(IRibbonExtensibility)
	END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		/*
		UserButtonInfo ubi;
		ubi.strFunction = "exampleFunction";
		ubi.strLabel = "Example Function";
		userbuttons.push_back(ubi);
		*/
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

	UBVECTOR userbuttons;

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

			else if (!wcscmp(rgszNames[0], L"getUserButtonsVisible"))			dispID = DISPID_USER_BUTTONS_VISIBLE;
			else if (!wcscmp(rgszNames[0], L"getUserButtonVisible"))			dispID = DISPID_USER_BUTTON_VISIBLE;
			else if (!wcscmp(rgszNames[0], L"getUserButtonLabel"))				dispID = DISPID_USER_BUTTON_LABEL;
			else if (!wcscmp(rgszNames[0], L"userButtonAction"))				dispID = DISPID_USER_BUTTON_ACTION;

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
		// this is single-threaded, right?
		static CComBSTR bstr;

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

		case DISPID_USER_BUTTONS_VISIBLE:
			if (pvarResult) {
				pvarResult->vt = VT_BOOL;
				pvarResult->boolVal = ( userbuttons.size() > 0 );
			}
			return S_OK;
		case DISPID_USER_BUTTON_VISIBLE:
			if (pvarResult && pdispparams->cArgs && pdispparams->rgvarg[0].vt == VT_DISPATCH ) {
				CComQIPtr<IRibbonControl> ctrl(pdispparams->rgvarg[0].pdispVal);
				CComBSTR bstrTag;
				ctrl->get_Tag(&bstrTag);
				int id = bstrTag.m_str[0] - '1';
				pvarResult->vt = VT_BOOL;
				pvarResult->boolVal = ( userbuttons.size() > id );
			}
			return S_OK;
		case DISPID_USER_BUTTON_LABEL:
			if (pvarResult && pdispparams->cArgs && pdispparams->rgvarg[0].vt == VT_DISPATCH) {
				CComQIPtr<IRibbonControl> ctrl(pdispparams->rgvarg[0].pdispVal);
				CComBSTR bstrTag;
				ctrl->get_Tag(&bstrTag);
				int id = bstrTag.m_str[0] - '1';
				if (userbuttons.size() > id) {
					bstr = userbuttons[id].strLabel.c_str();
					pvarResult->vt = VT_BSTR | VT_BYREF;
					pvarResult->pbstrVal = &bstr;
				}
			}
			return S_OK;
		case DISPID_USER_BUTTON_ACTION:
			if (pdispparams->cArgs && pdispparams->rgvarg[0].vt == VT_DISPATCH) {
				CComQIPtr<IRibbonControl> ctrl(pdispparams->rgvarg[0].pdispVal);
				CComBSTR bstrTag;
				ctrl->get_Tag(&bstrTag);
				int id = bstrTag.m_str[0] - '1';
				if (userbuttons.size() > id) {
					
					CComQIPtr< Excel::_Application > pApp(m_pApplication);
					if (pApp)
					{
						CComBSTR bstrCmd("BERT.Call");
						CComBSTR bstrMethod(userbuttons[id].strFunction.c_str());

						CComVariant cvCmd = bstrCmd;
						CComVariant cvFunc = bstrMethod;
						CComVariant m(DISP_E_PARAMNOTFOUND, VT_ERROR);
						CComVariant cvRslt;

						HRESULT hr = pApp->Run(cvCmd, cvFunc, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, &cvRslt);
						if (FAILED(hr)) {
							ATLTRACE("Ribbon call failed: 0x%x\n", hr);
						}
					}

				}
			}
			return E_FAIL;
		}
		return RIBBON_INTERFACE::Invoke(dispidMember, riid, lcid,
			wFlags, pdispparams, pvarResult, pexcepinfo, puArgErr);
	}

	// Local methods


	int SetCOMPtrs(void *pexcel, void *pribbon)
//	int SetCOMPtrs(Excel::_Application *pexcel, void *pribbon)
	{
		// this is just -1 for the current process
		HANDLE h = GetCurrentProcess();
		HMODULE hMods[1024];
		DWORD cbNeeded;

		int rslt = -1;

		if (EnumProcessModules(h, hMods, sizeof(hMods), &cbNeeded))
		{
			for (int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
			{
				char szModName[MAX_PATH];
				if (GetModuleFileNameExA(h, hMods[i], szModName, sizeof(szModName) / sizeof(char)))
				{
					std::string str(szModName);

					// this is fragile, because it's a filename.  is this actually expensive if 
					// we call on other modules?

					if (std::string::npos != str.find("BERT")
						&& std::string::npos != str.find(".xll"))
					{
						SPPROC sp;
						sp = (SPPROC)::GetProcAddress(hMods[i], "BERT_SetPtr");
						if (sp){
//							sp((LPVOID)m_pApplication.p);
							sp((LPVOID)pexcel);
							rslt = 0;
						}
					}
				}
			}
		}

		return rslt;
	}


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
		CComQIPtr< Excel::_Application > pApp(m_pApplication);
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
