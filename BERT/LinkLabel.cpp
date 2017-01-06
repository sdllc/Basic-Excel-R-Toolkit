/*
* Basic Excel R Toolkit (BERT)
* Copyright (C) 2014-2017 Structured Data, LLC
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
#include "Shellapi.h"
#include "LinkLabel.h"

static HFONT underline = 0;
static HCURSOR hand = 0;

int refcount = 0;

LRESULT CALLBACK LinkLabelParentMsgProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WNDPROC base = (WNDPROC)GetProp(hwnd, TEXT("BaseWndProc"));
	switch (message)
	{
	case WM_CTLCOLORSTATIC:
		if (GetProp((HWND)lParam, TEXT("Link")))
		{
			HDC dc = (HDC)wParam;
			LRESULT rslt = CallWindowProc(base, hwnd, message, wParam, lParam);
			SetTextColor(dc, RGB(0, 72, 244));
			return rslt;
		}
		break;
	}
	return CallWindowProc(base, hwnd, message, wParam, lParam);
}

LRESULT CALLBACK LinkLabelMsgProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_KILLFOCUS:
		::RedrawWindow(hwnd, 0, 0, RDW_INVALIDATE);
		break;

	case WM_SETFOCUS:
		{
			RECT rc;
			HDC dc = ::GetDC(hwnd);
			::GetClientRect(hwnd, &rc);
			::DrawFocusRect(dc, &rc);
			::ReleaseDC(hwnd, dc);
		}
		break;

	case WM_MOUSEMOVE:
		if (GetCapture() != hwnd)
		{
			SendMessage(hwnd, WM_SETFONT, (WPARAM)GetProp(hwnd, TEXT("Underline")), TRUE);
			SetCapture(hwnd);
		}
		else
		{
			RECT rc;
			GetWindowRect(hwnd, &rc);
			POINT p = { LOWORD(lParam), HIWORD(lParam) };
			ClientToScreen(hwnd, &p);
			if (!PtInRect(&rc, p)) ReleaseCapture();
		}
		break;

	case WM_CAPTURECHANGED:
		SendMessage(hwnd, WM_SETFONT, (WPARAM)GetProp(hwnd, TEXT("BaseFont")), TRUE);
		break;

	case WM_SETCURSOR:
		{
			HCURSOR hand = (HCURSOR)LoadImage(0, IDC_HAND, IMAGE_CURSOR, 0, 0, LR_SHARED);
			if (hand) SetCursor(hand);
			return TRUE;
		}
		break;

	case WM_KEYUP:
		if (wParam != VK_SPACE) break;

	case WM_LBUTTONUP:
		{
			WCHAR *wc = (WCHAR*)GetProp(hwnd, TEXT("Link"));
			if ( wc ) ShellExecute(0, 0, wc, 0, 0, SW_SHOWNORMAL);
		}
		break;

	case WM_DESTROY:
		{
			WCHAR *wc = (WCHAR*)GetProp(hwnd, TEXT("Link"));
			if (wc) delete [] wc;
			RemoveProp(hwnd, TEXT("Link"));
			RemoveProp(hwnd, TEXT("BaseFont"));
			HFONT underline = (HFONT)GetProp(hwnd, TEXT("Underline"));
			::DeleteObject(underline);
			RemoveProp(hwnd, TEXT("Underline"));
		}
		break;
	}

	WNDPROC base = (WNDPROC)GetProp( hwnd, TEXT("BaseWndProc"));
	return CallWindowProc(base, hwnd, message, wParam, lParam);
}

void ResizeLabel(HWND hwnd, DWORD style)
{
	HDC dc = ::GetDC(hwnd);
	HFONT font = (HFONT)::SendMessage(hwnd, WM_GETFONT, 0, 0);
	HGDIOBJ old = ::SelectObject(dc, font);
	SIZE size;

	int len = GetWindowTextLength(hwnd);
	WCHAR *buffer = new WCHAR[len + 1];
	::GetWindowText(hwnd, buffer, len);

	if (GetTextExtentPoint32(dc, buffer, len, &size))
	{
		RECT rc;
		::GetWindowRect(hwnd, &rc);
		::ScreenToClient(::GetParent(hwnd), (POINT*)&rc); 

		if (style & SS_CENTER) {

			::ScreenToClient(::GetParent(hwnd), (POINT*)&(rc.right));

			// center horizontally but leave vertical alone? 
			int left = ((rc.right - rc.left) - size.cx) / 2;
			::MoveWindow(hwnd, rc.left + left, rc.top, size.cx, size.cy, false);

		}
		else {
			::MoveWindow(hwnd, rc.left, rc.top, size.cx, size.cy, false);
		}
	}

	::SelectObject(dc, old);
	::ReleaseDC(hwnd, dc);

	delete [] buffer;
}

void SubclassLinkLabel(HWND hwnd, const WCHAR *link, const WCHAR *text)
{
	WNDPROC wndproc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)LinkLabelMsgProc);
	SetProp(hwnd, TEXT("BaseWndProc"), (HANDLE)wndproc);

	wndproc = (WNDPROC)GetWindowLongPtr(::GetParent(hwnd), GWLP_WNDPROC);
	if (wndproc != LinkLabelParentMsgProc)
	{
		wndproc = (WNDPROC)SetWindowLongPtr(::GetParent(hwnd), GWLP_WNDPROC, (LONG_PTR)LinkLabelParentMsgProc);
		SetProp(::GetParent(hwnd), TEXT("BaseWndProc"), (HANDLE)wndproc);
	}

	int len = wcslen(link);
	WCHAR *wc = new WCHAR[len + 1];
	wcscpy_s(wc, len + 1, link);
	SetProp(hwnd, TEXT("Link"), (HANDLE)wc);

	DWORD style = GetWindowLongPtr(hwnd, GWL_STYLE) | SS_NOTIFY | WS_TABSTOP;
	SetWindowLongPtr(hwnd, GWL_STYLE, style);

	if (text) ::SetWindowText(hwnd, text);
	ResizeLabel(hwnd, style);

	HFONT font = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
	LOGFONT logfont;
	GetObject(font, sizeof(logfont), &logfont);
	logfont.lfUnderline = -1;
	HFONT underline = CreateFontIndirect(&logfont);

	SetProp(hwnd, TEXT("BaseFont"), (HANDLE)font);
	SetProp(hwnd, TEXT("Underline"), (HANDLE)underline);

}