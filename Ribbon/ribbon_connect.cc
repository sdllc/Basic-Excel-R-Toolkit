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
 
void __stdcall BERTRibbon2_LookupFunction(void) {}

// CConnect
STDMETHODIMP CConnect::OnConnection(IDispatch *pApplication, AddInDesignerObjects::ext_ConnectMode /*ConnectMode*/, IDispatch *pAddInInst, SAFEARRAY ** /*custom*/)
{
  char path[MAX_PATH];

  GetModuleFileNameA(_AtlBaseModule.m_hInst, path, sizeof(path));
  PathRemoveFileSpecA(path); // deprecated, but the replacement is windows 8+ only

  std::string xll_path = path;
  xll_path.append("\\"); // is there a win32 constant for path separator?

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
  if (app) {

    int result = SetPointers(reinterpret_cast<ULONG_PTR>(m_pApplication.p), reinterpret_cast<ULONG_PTR>(this));
    if (result) {

      _bstr_t bstrPath(xll_path.c_str());
      VARIANT_BOOL vb = VARIANT_FALSE;
      HRESULT hr = app->RegisterXLL(bstrPath, 1033, &vb);

      if (SUCCEEDED(hr)) {
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

/**
 * thanks to
 * http://microsoft.public.office.developer.com.add-ins.narkive.com/lyxdw2Ns/iribbonextensibility-callback-problem
 */
STDMETHODIMP CConnect::GetImage(int32_t image_id, VARIANT *result)
{
  // FIXME: we need a different image for high-dpi screens

  HRESULT hr = S_OK;
  PICTDESC pd;

  pd.cbSizeofstruct = sizeof(PICTDESC);
  pd.picType = PICTYPE_BITMAP;

  Gdiplus::GdiplusStartupInput gdiplusStartupInput;
  ULONG_PTR gdiplusToken;

  gdiplusStartupInput.DebugEventCallback = NULL;
  gdiplusStartupInput.SuppressBackgroundThread = FALSE;
  gdiplusStartupInput.SuppressExternalCodecs = FALSE;
  gdiplusStartupInput.GdiplusVersion = 1;
  GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

  HRSRC hResource = FindResource(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(image_id), TEXT("PNG"));

  if (!hResource) return S_FALSE;

  DWORD dwImageSize = SizeofResource(_AtlBaseModule.GetResourceInstance(), hResource);
  const void* pResourceData = LockResource(LoadResource(_AtlBaseModule.GetResourceInstance(), hResource));
  if (!pResourceData) return S_FALSE;

  HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, dwImageSize);
  if (hBuffer) {

    void* pBuffer = GlobalLock(hBuffer);
    if (pBuffer) {

      CopyMemory(pBuffer, pResourceData, dwImageSize);

      IStream* pStream = NULL;
      if (SUCCEEDED(::CreateStreamOnHGlobal(hBuffer, FALSE, &pStream))) {

        Gdiplus::Bitmap *pBitmap = Gdiplus::Bitmap::FromStream(pStream);
        pStream->Release();

        if (pBitmap) {
          pBitmap->GetHBITMAP(0, &pd.bmp.hbitmap);
          hr = OleCreatePictureIndirect(&pd, IID_IDispatch, TRUE, (LPVOID*)(&(result->pdispVal)));
          if (SUCCEEDED(hr)) {
            result->pdispVal->AddRef();
            result->vt = VT_DISPATCH;
          }
          delete pBitmap;
        }
      }
      GlobalUnlock(pBuffer);
    }
    GlobalFree(hBuffer);
  }

  Gdiplus::GdiplusShutdown(gdiplusToken);

  return hr;
}

