/*
 * Basic Excel R Toolkit (BERT)
 * Copyright (C) 2014-2016 Structured Data, LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "stdafx.h"
#include "BERT.h"

#include "resource.h"
#include "Dialogs.h"
#include "StringConstants.h"
#include "RegistryUtils.h"
#include "LinkLabel.h"
#include "Updates.h"

extern HMODULE ghModule;

// windows imaging
#include <wincodec.h>
#include <atlbase.h>

HBITMAP loadPNG( HMODULE mod, int rsrcID ){

	HBITMAP hbmp = 0;
	HRESULT hr;
	static CComPtr<IWICImagingFactory> pFactory = 0;

	if (!pFactory){
		hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (void**)&pFactory);
		if (FAILED(hr) || !pFactory) return 0;
	}

	CComPtr<IStream> pStream = 0;
	CComPtr<IWICBitmapSource> pBitmap = 0;

	HRSRC hrsrc = FindResource(mod, MAKEINTRESOURCE(rsrcID), L"PNG");
	DWORD dwSize = SizeofResource(mod, hrsrc);
	HGLOBAL hImage = LoadResource(mod, hrsrc);
	void * pSource = LockResource(hImage);
	HGLOBAL hData = GlobalAlloc(GMEM_MOVEABLE, dwSize);

	if (hData){
		void * pData = GlobalLock(hData);
		CopyMemory(pData, pSource, dwSize);
		GlobalUnlock(hData);
		CreateStreamOnHGlobal(hData, TRUE, &pStream);
	}

	if (pStream){
		CComPtr<IWICBitmapDecoder> pDecoder = 0;
		CoCreateInstance(CLSID_WICPngDecoder, NULL, CLSCTX_INPROC_SERVER, __uuidof(pDecoder), reinterpret_cast<void**>(&pDecoder));
		pDecoder->Initialize(pStream, WICDecodeMetadataCacheOnLoad);

		CComPtr<IWICBitmapFrameDecode> pFrame = 0;
		pDecoder->GetFrame(0, &pFrame);

		CComQIPtr<IWICBitmapSource> pSource(pFrame);
		hr = WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, pSource, &pBitmap);
	}

	if (pBitmap){

		UINT w, h;
		hr = pBitmap->GetSize(&w, &h);

		if (SUCCEEDED(hr)){

			BITMAPINFO bmi;
			ZeroMemory(&bmi, sizeof(bmi));
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = w;
			bmi.bmiHeader.biHeight = -((LONG)h);
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;
			bmi.bmiHeader.biCompression = BI_RGB;

			void * pImage = NULL;
			HDC hdcScreen = GetDC(NULL);
			hbmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pImage, NULL, 0);
			ReleaseDC(NULL, hdcScreen);

			hr = pBitmap->CopyPixels(NULL, w * 4, w * h * 4, static_cast<BYTE *>(pImage));

		}
	}

	return hbmp;
}

void CenterWindow(HWND hWnd, HWND hParent, int offsetX, int offsetY)
{
	RECT rectWnd;
	RECT rectDlg;

	int iWidth, iHeight;
	int iPWidth, iPHeight;

	::GetWindowRect(hParent, &rectWnd);
	::GetWindowRect(hWnd, &rectDlg);

	iWidth = rectDlg.right - rectDlg.left;
	iHeight = rectDlg.bottom - rectDlg.top;

	iPWidth = rectWnd.right - rectWnd.left;
	iPHeight = rectWnd.bottom - rectWnd.top;

	::SetWindowPos(hWnd, HWND_TOP,
		rectWnd.left + offsetX + (iPWidth - iWidth) / 2,
		rectWnd.top + offsetY + (iPHeight - iHeight) / 2,
		0, 0, SWP_NOSIZE);
}

DIALOG_RESULT_TYPE CALLBACK AboutDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static HBITMAP hbmp = 0;

	switch (message)
	{
	case WM_INITDIALOG:

		::SetWindowText(::GetDlgItem(hwndDlg, IDC_STATIC_ABOUT_BERT), ABOUT_BERT_TEXT);
		::SetWindowText(::GetDlgItem(hwndDlg, IDC_STATIC_ABOUT_R), ABOUT_R_TEXT);
		::SetWindowText(::GetDlgItem(hwndDlg, IDC_STATIC_ABOUT_SCINTILLA), ABOUT_SCINTILLA_TEXT);

		SubclassLinkLabel(::GetDlgItem(hwndDlg, IDC_STATIC_BERT_LINK), BERT_LINK, BERT_LINK_TEXT);
		SubclassLinkLabel(::GetDlgItem(hwndDlg, IDC_STATIC_R_LINK), R_LINK, R_LINK_TEXT);
		SubclassLinkLabel(::GetDlgItem(hwndDlg, IDC_STATIC_SCINTILLA_LINK), SCINTILLA_LINK, SCINTILLA_LINK_TEXT);

		hbmp = loadPNG(ghModule, IDB_PNG1);

		CenterWindow(hwndDlg, ::GetParent(hwndDlg));
		break;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			BITMAP bitmap;
			HGDIOBJ oldBitmap;
			RECT rc;
			HDC hdc = BeginPaint(hwndDlg, &ps);

			::GetClientRect(hwndDlg, &rc);

			BLENDFUNCTION bStruct;
			bStruct.BlendOp = AC_SRC_OVER;
			bStruct.BlendFlags = 0;
			bStruct.SourceConstantAlpha = 255;
			bStruct.AlphaFormat = AC_SRC_ALPHA;

			HDC memdc = CreateCompatibleDC(hdc);
			oldBitmap = SelectObject(memdc, hbmp);

			GetObject(hbmp, sizeof(bitmap), &bitmap);
			AlphaBlend(hdc, rc.right - bitmap.bmWidth - 8, 0, bitmap.bmWidth, bitmap.bmHeight, memdc, 0, 0, bitmap.bmWidth, bitmap.bmHeight, bStruct);

			SelectObject(memdc, oldBitmap);
			DeleteDC(memdc);

			EndPaint(hwndDlg, &ps);
		}
		break;

	case WM_TIMER:

		::KillTimer(hwndDlg, TIMER_ID_UPDATE);
		{
			std::string tag;
			std::string msg;
			bool appendTag = false;
			bool link = false;

			int rslt = checkForUpdates(tag); // FIXME: async!
			switch (rslt) {
			case UC_UP_TO_DATE:
				msg = MSG_UP_TO_DATE;
				appendTag = true;
				break;
			case UC_NEW_VERSION_AVAILABLE:
				msg = MSG_UPDATE_AVAILABLE;
				appendTag = true;
				link = true;
				break;
			case UC_UPDATE_CHECK_FAILED:
				msg = MSG_UPDATE_CHECK_FAILED;
				break;
			case UC_YOURS_IS_NEWER:
				msg = MSG_YOURS_IS_NEWER;
				break;
			}

			if (appendTag) {
				msg += " (";
				msg += tag;
				msg += ").";
			}

			if (link) {
				HWND hwndLink = ::GetDlgItem(hwndDlg, IDC_STATIC_UPDATE_LINK);
				::ShowWindow(hwndLink, SW_SHOW);
				SubclassLinkLabel(hwndLink, LATEST_RELEASE_URL, MSG_CLICK_TO_DOWNLOAD);
			}
			::SetWindowTextA(::GetDlgItem(hwndDlg, IDC_STATIC_UPDATE_MESSAGE), msg.c_str());

			::EnableWindow(::GetDlgItem(hwndDlg, IDOK), true);
		}
		break;

	case WM_COMMAND:

		switch (LOWORD(wParam))
		{
		case IDC_STATIC_UPDATE_LINK:

			// do this via timer? TODO
			//EndDialog(hwndDlg, wParam);
			//return TRUE;

			break;

		case IDC_BCHECK:
			
			::ShowWindow(::GetDlgItem(hwndDlg, IDC_STATIC_ABOUT_BERT), SW_HIDE);
			::ShowWindow(::GetDlgItem(hwndDlg, IDC_STATIC_ABOUT_R), SW_HIDE);
			::ShowWindow(::GetDlgItem(hwndDlg, IDC_STATIC_ABOUT_SCINTILLA), SW_HIDE);

			::ShowWindow(::GetDlgItem(hwndDlg, IDC_STATIC_BERT_LINK), SW_HIDE);
			::ShowWindow(::GetDlgItem(hwndDlg, IDC_STATIC_R_LINK), SW_HIDE);
			::ShowWindow(::GetDlgItem(hwndDlg, IDC_STATIC_SCINTILLA_LINK), SW_HIDE);

			::ShowWindow(::GetDlgItem(hwndDlg, IDC_STATIC_UPDATE_MESSAGE), SW_SHOW);
			::SetTimer(hwndDlg, TIMER_ID_UPDATE, UPDATE_CHECK_DELAY, 0);

			::EnableWindow(::GetDlgItem(hwndDlg, IDOK), false);
			::EnableWindow(::GetDlgItem(hwndDlg, IDC_BCHECK), false);

			break;

		case IDOK:
		case IDCANCEL:
			
			::KillTimer(hwndDlg, TIMER_ID_UPDATE);
			if (hbmp) ::DeleteObject(hbmp);
			hbmp = 0;

			EndDialog(hwndDlg, wParam);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

DIALOG_RESULT_TYPE CALLBACK OptionsDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	char buffer[MAX_PATH];
	DWORD dw;
	HWND hWnd;

	switch (message)
	{
	case WM_INITDIALOG:
		CenterWindow(hwndDlg, ::GetParent(hwndDlg));
		{
			hWnd = ::GetDlgItem(hwndDlg, IDC_RHOME);
			buffer[0] = 0;
			CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, buffer, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_HOME, false);
			SetWindowTextA(hWnd, buffer);

			hWnd = ::GetDlgItem(hwndDlg, IDC_RUSER);
			buffer[0] = 0;
			CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, buffer, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_R_USER, false);
			SetWindowTextA(hWnd, buffer);

			hWnd = ::GetDlgItem(hwndDlg, IDC_ENVIRONMENT);
			buffer[0] = 0;
			CRegistryUtils::GetRegString(HKEY_CURRENT_USER, buffer, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_ENVIRONMENT);
			SetWindowTextA(hWnd, buffer);

			hWnd = ::GetDlgItem(hwndDlg, IDC_STARTUPFILE);
			buffer[0] = 0;
			CRegistryUtils::GetRegString(HKEY_CURRENT_USER, buffer, MAX_PATH - 1, REGISTRY_KEY, REGISTRY_VALUE_STARTUP);
			SetWindowTextA(hWnd, buffer);

			if (!CRegistryUtils::GetRegDWORD(HKEY_CURRENT_USER, &dw, REGISTRY_KEY, REGISTRY_VALUE_PRESERVE_ENV)) dw = DEFAULT_R_PRESERVE_ENV;
			::SendMessage(::GetDlgItem(hwndDlg, IDC_SAVE_ENVIRONMENT), BM_SETCHECK, dw ? BST_CHECKED : BST_UNCHECKED, 0);

		}
		break;

	case WM_COMMAND:

		switch (LOWORD(wParam))
		{
		case IDOK:
			::GetWindowTextA(::GetDlgItem(hwndDlg, IDC_RHOME), buffer, MAX_PATH - 1);
			CRegistryUtils::SetRegExpandString(HKEY_CURRENT_USER, buffer, REGISTRY_KEY, REGISTRY_VALUE_R_HOME );
			::GetWindowTextA(::GetDlgItem(hwndDlg, IDC_RUSER), buffer, MAX_PATH - 1);
			CRegistryUtils::SetRegExpandString(HKEY_CURRENT_USER, buffer, REGISTRY_KEY, REGISTRY_VALUE_R_USER);
			::GetWindowTextA(::GetDlgItem(hwndDlg, IDC_ENVIRONMENT), buffer, MAX_PATH - 1);
			CRegistryUtils::SetRegString(HKEY_CURRENT_USER, buffer, REGISTRY_KEY, REGISTRY_VALUE_ENVIRONMENT);
			::GetWindowTextA(::GetDlgItem(hwndDlg, IDC_STARTUPFILE), buffer, MAX_PATH - 1);
			CRegistryUtils::SetRegString(HKEY_CURRENT_USER, buffer, REGISTRY_KEY, REGISTRY_VALUE_STARTUP);

			dw = ( BST_CHECKED == ::SendMessage(::GetDlgItem(hwndDlg, IDC_SAVE_ENVIRONMENT), BM_GETCHECK, 0, 0)) ? 1 : 0;
			CRegistryUtils::SetRegDWORD(HKEY_CURRENT_USER, dw, REGISTRY_KEY, REGISTRY_VALUE_PRESERVE_ENV);

		case IDCANCEL:
			EndDialog(hwndDlg, wParam);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

