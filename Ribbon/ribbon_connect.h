/**
 * Copyright (c) 2017-2018 Structured Data, LLC
 * 
 * This file is part of BERT.
 *
 * BERT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BERT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with BERT.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "resource.h"       // main symbols

#include <atlsafe.h>
#include <Gdiplus.h>

#include "../Common/user_button.h"
#include <vector>

typedef IDispatchImpl<IRibbonExtensibility, &__uuidof(IRibbonExtensibility), &LIBID_Office, /* wMajor = */ 2, /* wMinor = */ 4> RIBBON_INTERFACE;
typedef int(*SetPointersProcedure)(ULONG_PTR, ULONG_PTR);

typedef enum {

  ShowConsole = 1001,
  GetImage,
  GetLabel,

  UpdateRibbon,
  AddUserButton,
  ClearUserButtons,

  GetUserButtonImage,
  GetUserButtonVisible,
  GetUserButtonsVisible,
  GetUserButtonLabel,
  GetUserButtonTip,
  UserButtonAction,

  RibbonLoaded

} DispIds;

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

  STDMETHOD(GetImage)(int32_t image_id, VARIANT *result);

  CComPtr<IDispatch>      m_pApplication;
  CComPtr<IDispatch>      m_pAddInInstance;
  CComQIPtr<IRibbonUI>    m_pRibbonUI;

  std::vector<UserButton> user_buttons_;

  // IRibbonExtensibility methods
  STDMETHOD(GetCustomUI)(BSTR RibbonID, BSTR *pbstrRibbonXML);

  // IDispatch methods
  STDMETHOD(GetIDsOfNames)(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid) {

    int disp_id = 0;

    if (cNames > 0) {
      if (!wcscmp(rgszNames[0], L"ShowConsole")) disp_id = DispIds::ShowConsole;
      else if (!wcscmp(rgszNames[0], L"GetImage")) disp_id = DispIds::GetImage;
      else if (!wcscmp(rgszNames[0], L"GetLabel")) disp_id = DispIds::GetLabel;
      else if (!wcscmp(rgszNames[0], L"GetUserButtonImage")) disp_id = DispIds::GetUserButtonImage;
      else if (!wcscmp(rgszNames[0], L"GetUserButtonVisible")) disp_id = DispIds::GetUserButtonVisible;
      else if (!wcscmp(rgszNames[0], L"GetUserButtonsVisible")) disp_id = DispIds::GetUserButtonsVisible;
      else if (!wcscmp(rgszNames[0], L"GetUserButtonLabel")) disp_id = DispIds::GetUserButtonLabel;
      else if (!wcscmp(rgszNames[0], L"GetUserButtonTip")) disp_id = DispIds::GetUserButtonTip;
      else if (!wcscmp(rgszNames[0], L"UserButtonAction")) disp_id = DispIds::UserButtonAction;
      else if (!wcscmp(rgszNames[0], L"UpdateRibbon")) disp_id = DispIds::UpdateRibbon;
      else if (!wcscmp(rgszNames[0], L"AddUserButton")) disp_id = DispIds::AddUserButton;
      else if (!wcscmp(rgszNames[0], L"ClearUserButtons")) disp_id = DispIds::ClearUserButtons;
      else if (!wcscmp(rgszNames[0], L"RibbonLoaded")) disp_id = DispIds::RibbonLoaded;
    }

    if (disp_id > 0)
    {
      rgdispid[0] = disp_id;
      return S_OK;
    }

    return RIBBON_INTERFACE::GetIDsOfNames(riid, rgszNames, cNames, lcid, rgdispid);
  }

  int ButtonTag(DISPPARAMS* pdispparams) {

    int tag = -1; // invalid
    if (pdispparams->cArgs && pdispparams->rgvarg[0].vt == VT_DISPATCH) {
      CComQIPtr<IRibbonControl> control(pdispparams->rgvarg[0].pdispVal);
      if (control) {
        CComBSTR tag_string;
        control->get_Tag(&tag_string);
        tag = _wtoi(tag_string);
      }
    }
    return tag;

  }

  STDMETHOD(Invoke)(DISPID dispidMember, REFIID riid,
    LCID lcid, WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult,
    EXCEPINFO* pexcepinfo, UINT* puArgErr)
  {
    static CComBSTR bstr;
    CComVariant result;
    int tag;

    switch (dispidMember)
    {
    case DispIds::RibbonLoaded:
      return HandleCacheRibbon(pdispparams);

    case DispIds::AddUserButton:
    {
      if (pdispparams->cArgs >= 4) {

        uint32_t id = pdispparams->rgvarg[0].intVal;
        CComBSTR image(pdispparams->rgvarg[1].bstrVal);
        CComBSTR language_tag(pdispparams->rgvarg[2].bstrVal);
        CComBSTR label(pdispparams->rgvarg[3].bstrVal);
        
        CComBSTR tip = L"";
        if (pdispparams->cArgs == 5) tip = pdispparams->rgvarg[4].bstrVal;

        UserButton button(label.m_str, language_tag.m_str, image.m_str, tip.m_str, id);
        user_buttons_.push_back(button);
        
        if (m_pRibbonUI) m_pRibbonUI->Invalidate();
        return S_OK;
      }
      return E_FAIL;
    }

    case DispIds::GetLabel:
      result = "BERT\u00a0Console";
      result.Detach(pvarResult);
      return S_OK;

    case DispIds::UserButtonAction:
    {
      tag = ButtonTag(pdispparams);
      CComQIPtr< Excel::_Application > pApp(m_pApplication);
      if (tag > 0 && user_buttons_.size() > tag - 1 && pApp) {
        const UserButton &button = user_buttons_[tag - 1];

        CComVariant result;
        CComVariant command = "BERT.ButtonCallback";
        CComVariant id = button.id_;
        CComVariant language = button.language_tag_.c_str();

        CComVariant missing(DISP_E_PARAMNOTFOUND, VT_ERROR);
        return pApp->_Run2(command, id, language, missing, missing, missing, missing, missing, missing, missing,
          missing, missing, missing, missing, missing, missing, missing, missing, missing, missing, missing, missing,
          missing, missing, missing, missing, missing, missing, missing, missing, missing, 1033, &result);
      }
      return E_FAIL;
    }

    case DispIds::GetUserButtonImage:
    {
      tag = ButtonTag(pdispparams);
      if (tag > 0 && user_buttons_.size() > tag - 1) {
        result = user_buttons_[tag - 1].image_mso_.c_str();
        result.Detach(pvarResult);
      }
      return S_OK;
    }
    case DispIds::GetUserButtonLabel:
      tag = ButtonTag(pdispparams);
      result = "";
      if (tag > 0 && user_buttons_.size() > tag - 1) {
        result = user_buttons_[tag - 1].label_.c_str();
      }
      result.Detach(pvarResult);
      return S_OK;

    case DispIds::GetUserButtonTip:
      tag = ButtonTag(pdispparams);
      result = "";
      if (tag > 0 && user_buttons_.size() > tag - 1) {
        result = user_buttons_[tag - 1].tip_.c_str();
      }
      result.Detach(pvarResult);
      return S_OK;

    case DispIds::GetUserButtonsVisible:
      result = (user_buttons_.size() > 0) ? VARIANT_TRUE : VARIANT_FALSE;
      result.Detach(pvarResult);
      return S_OK;

    case DispIds::GetUserButtonVisible:
      tag = ButtonTag(pdispparams);
      result = (tag > 0 && user_buttons_.size() > tag-1) ? VARIANT_TRUE : VARIANT_FALSE;
      result.Detach(pvarResult);
      return S_OK;

    case DispIds::GetImage:
    {
      HDC screen = GetDC(NULL);
      double scale = GetDeviceCaps(screen, LOGPIXELSX) / 96.0;
      ReleaseDC(NULL, screen);
      if (scale > 1) {
        return GetImage(IDB_CONSOLE_32, pvarResult);
      }
      return GetImage(IDB_CONSOLE_16, pvarResult);
    }

    case DispIds::ShowConsole:
      return HandleControlInvocation("BERT.Console()");

    case DispIds::ClearUserButtons:
      user_buttons_.clear();
      if (m_pRibbonUI) m_pRibbonUI->Invalidate();
      break;

    case DispIds::UpdateRibbon:
      if (m_pRibbonUI) m_pRibbonUI->Invalidate();
      break;

    }

    return RIBBON_INTERFACE::Invoke(dispidMember, riid, lcid, wFlags, pdispparams, pvarResult, pexcepinfo, puArgErr);
  }

  // Local Methods

  int SetPointers(ULONG_PTR excel_pointer, ULONG_PTR ribbon_pointer)
  {
    HANDLE handle = GetCurrentProcess();
    HMODULE module_handles[1024];
    DWORD bytes;
    char buffer[MAX_PATH];

    if (EnumProcessModules(handle, module_handles, sizeof(module_handles), &bytes))
    {
      int count = bytes / sizeof(HANDLE);
      for (int i = 0; i < count; i++)
      {
        if (GetModuleFileNameExA(handle, module_handles[i], buffer, MAX_PATH))
        {
          std::string module_name(buffer);
          if (std::string::npos != module_name.find("BERT") && std::string::npos != module_name.find(".xll"))
          {
            auto procedure = GetProcAddress(module_handles[i], "BERT_SetPointers");
            if (procedure) {
              SetPointersProcedure set_pointers = reinterpret_cast<SetPointersProcedure>(procedure);
              set_pointers(excel_pointer, ribbon_pointer);
              return 0;
            }
          }
        }
      }
    }

    return -1;
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
