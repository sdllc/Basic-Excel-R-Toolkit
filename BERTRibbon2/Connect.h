
#pragma once

#include "resource.h"       // main symbols

#include <vector>
#include <atlsafe.h>

typedef IDispatchImpl<IRibbonExtensibility, &__uuidof(IRibbonExtensibility), &LIBID_Office, /* wMajor = */ 2, /* wMinor = */ 4> RIBBON_INTERFACE;

typedef int (*SPPROC)(LPVOID, LPVOID);
typedef int (*UBCPROC)();
typedef int (*UBPROC)(char*, int, char*, int, char*, int, int);

void cstr2BSTR(CComBSTR &bstr, const char *sz) {

	int strlen = MultiByteToWideChar(CP_UTF8, 0, sz, -1, 0, 0);
	if (strlen > 0) {
		WCHAR *wsz = new WCHAR[strlen];
		MultiByteToWideChar(CP_UTF8, 0, sz, -1, wsz, strlen);
		bstr = wsz;
		delete[] wsz;
	}
	else bstr= "";

}

class UserButtonInfo {
public:
	CComBSTR bstrLabel;
	CComBSTR bstrFunction;
	CComBSTR bstrImageMSO;

	UserButtonInfo() : bstrImageMSO("R") {};

	UserButtonInfo(const UserButtonInfo &rhs) {
		bstrLabel = rhs.bstrLabel;
		bstrFunction = rhs.bstrFunction;
		bstrImageMSO = rhs.bstrImageMSO;
	}

	void FromCString(const char *label, const char *func, const char *imgmso = 0) {
		if(label) cstr2BSTR(bstrLabel, label);
		if(func) cstr2BSTR(bstrFunction, func);
		if(imgmso) cstr2BSTR(bstrImageMSO, imgmso);
	}

};

typedef std::vector < UserButtonInfo > UBVECTOR;

#define DISPID_USER_BUTTONS_VISIBLE		3000
#define DISPID_USER_BUTTON_VISIBLE		3001
#define DISPID_USER_BUTTON_LABEL		3002
#define DISPID_USER_BUTTON_ACTION		3004
#define DISPID_USER_BUTTON_IMAGE		3005

#define DISPID_ADD_USER_BUTTON			4000
#define DISPID_REMOVE_USER_BUTTON		4001
#define DISPID_CLEAR_USER_BUTTONS		4002
#define DISPID_REFRESH_RIBBON			4003

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
			else if (!wcscmp(rgszNames[0], L"getUserButtonImage"))				dispID = DISPID_USER_BUTTON_IMAGE;
			
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

				if (pdispparams->cArgs>2 && pdispparams->rgvarg[2].vt == VT_BSTR) {
					ubi.bstrImageMSO = pdispparams->rgvarg[2].bstrVal;
					//SysFreeString(pdispparams->rgvarg[1].bstrVal);
				}
				else if (pdispparams->cArgs>2 && pdispparams->rgvarg[1].vt == (VT_BSTR | VT_BYREF)) {
					ubi.bstrImageMSO = pdispparams->rgvarg[2].bstrVal;
				}

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
						CComSafeArray<VARIANT> cc;
						cc.Create(1); 
						CComBSTR bstrCode = userbuttons[id].bstrFunction;
						bstrCode.Append("(");
						WCHAR w = bstrTag.m_str[0] - 1;
						bstrCode.Append(w);
						bstrCode.Append( "); ");
						CComVariant v(bstrCode);
						cc.SetAt(0, v);
						CComVariant cvRslt;
						CComVariant m(DISP_E_PARAMNOTFOUND, VT_ERROR);
						HRESULT hr = pApp->_Run2( CComVariant(CComBSTR("BERT.SafeCall")), CComVariant(0L), CComVariant((LPSAFEARRAY)cc),
							m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, m, 1033,
							&cvRslt);
							return hr;
						cc.Destroy();

					}

				}
			}
			return E_FAIL;
		case DISPID_USER_BUTTON_IMAGE:
			{
				if (pvarResult && pdispparams->cArgs && pdispparams->rgvarg[0].vt == VT_DISPATCH) {
					CComQIPtr<IRibbonControl> ctrl(pdispparams->rgvarg[0].pdispVal);
					CComBSTR bstrTag;
					ctrl->get_Tag(&bstrTag);
					int id = bstrTag.m_str[0] - '1';
					if (userbuttons.size() > id) {
						CComQIPtr< Excel::_Application > pApp(m_pApplication);
						if (pApp) {
							bstr = userbuttons[id].bstrImageMSO; // "GoLeftToRight";
							CComPtr<Office::_CommandBars> cb;
							IPictureDisp *pic = 0;
							pApp->get_CommandBars(&cb);
							if (cb) cb->GetImageMso(bstr, 16, 16, &pic);
							if (pic) {
								pic->AddRef();
								pvarResult->vt = VT_PICTURE;
								pvarResult->pdispVal = pic;
								return S_OK;
							}
						}
					}
				}
				return E_FAIL;
			}
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
								char img[256];
								for (int j = 0; j < ubcount && j < 12; j++) {
									if (ub(label, 255, func, 255, img, 255, j) == 0) {
										UserButtonInfo ubi;
										ubi.FromCString(label, func, img[0] ? img : 0);
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
