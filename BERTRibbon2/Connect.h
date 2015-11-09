
#pragma once

#include "resource.h"       // main symbols

#include <vector>

typedef IDispatchImpl<IRibbonExtensibility, &__uuidof(IRibbonExtensibility), &LIBID_Office, /* wMajor = */ 2, /* wMinor = */ 4> RIBBON_INTERFACE;

typedef int (*SPPROC)(LPVOID, LPVOID);
typedef int (*UBCPROC)();
typedef int (*UBPROC)(char*, int, char*, int, int);

class UserButtonInfo {
public:
	CComBSTR bstrLabel;
	CComBSTR bstrFunction;
	int iImageIndex;

	UserButtonInfo() {};
	UserButtonInfo(const UserButtonInfo &rhs) {
		bstrLabel = rhs.bstrLabel;
		bstrFunction = rhs.bstrFunction;
		iImageIndex = rhs.iImageIndex;
	}
};

typedef std::vector < UserButtonInfo > UBVECTOR;

#define DISPID_USER_BUTTONS_VISIBLE		3000
#define DISPID_USER_BUTTON_VISIBLE		3001
#define DISPID_USER_BUTTON_LABEL		3002
#define DISPID_USER_BUTTON_ACTION		3004

#define DISPID_ADD_USER_BUTTON			3005
#define DISPID_REMOVE_USER_BUTTON		3006
#define DISPID_CLEAR_USER_BUTTONS		3007
#define DISPID_REFRESH_RIBBON			3008

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
			
			else if (!wcscmp(rgszNames[0], L"RefreshRibbonUI"))					dispID = DISPID_REFRESH_RIBBON;

			else if (!wcscmp(rgszNames[0], L"AddUserButton"))					dispID = DISPID_ADD_USER_BUTTON;
			else if (!wcscmp(rgszNames[0], L"RemoveUserButton"))				dispID = DISPID_REMOVE_USER_BUTTON;
			else if (!wcscmp(rgszNames[0], L"ClearUserButtons"))				dispID = DISPID_CLEAR_USER_BUTTONS;

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

		case DISPID_ADD_USER_BUTTON:
			if (pdispparams->cArgs >= 2) {

				UserButtonInfo ubi;

				if (pdispparams->rgvarg[0].vt == VT_BSTR) {
					ubi.bstrLabel = pdispparams->rgvarg[0].bstrVal;
					//SysFreeString(pdispparams->rgvarg[0].bstrVal);
				}
				else if (pdispparams->rgvarg[0].vt == (VT_BSTR | VT_BYREF)) {
					ubi.bstrLabel = pdispparams->rgvarg[0].bstrVal;
				}
				else return E_INVALIDARG;

				if (pdispparams->rgvarg[1].vt == VT_BSTR) {
					ubi.bstrFunction = pdispparams->rgvarg[1].bstrVal;
					//SysFreeString(pdispparams->rgvarg[1].bstrVal);
				}
				else if (pdispparams->rgvarg[1].vt == (VT_BSTR | VT_BYREF)) {
					ubi.bstrFunction = pdispparams->rgvarg[1].bstrVal;
				}
				else return E_INVALIDARG;

				userbuttons.push_back(ubi);
				if (m_pRibbonUI) m_pRibbonUI->Invalidate();

				return S_OK;
			}
			return E_INVALIDARG;

		case DISPID_REMOVE_USER_BUTTON:
			return E_NOTIMPL;

		case DISPID_CLEAR_USER_BUTTONS:
			userbuttons.clear();
			if (m_pRibbonUI) m_pRibbonUI->Invalidate();
			return S_OK;

		case DISPID_REFRESH_RIBBON:
			if (m_pRibbonUI) {
				m_pRibbonUI->Invalidate();
			}
			return S_OK;

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
					bstr = userbuttons[id].bstrLabel;
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
						CComBSTR bstrMethod(userbuttons[id].bstrFunction );

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
							sp((LPVOID)pexcel, (LPVOID)pribbon);
							rslt = 0;
						}

						int ubcount = 0;
						UBCPROC ubc = (UBCPROC)::GetProcAddress( hMods[i], "GetUBCount" );
						if (ubc) {
							ubcount = ubc();
						}
						if (ubcount) {
							UBPROC ub = (UBPROC)::GetProcAddress(hMods[i], "GetUB");
							if (ub) {
								char label[256];
								char func[256];
								for (int j = 0; j < ubcount && j < 12; j++) {
									if (ub(label, 255, func, 255, j) == 0) {
										UserButtonInfo ubi;
										ubi.bstrLabel = label;
										ubi.bstrFunction = func;
										userbuttons.push_back(ubi);
									}
								}

							}
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
