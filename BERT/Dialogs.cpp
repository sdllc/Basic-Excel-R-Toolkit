/*
 * Basic Excel R Toolkit (BERT)
 * Copyright (C) 2014 Structured Data, LLC
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

extern HMODULE ghModule;

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
	
	switch (message)
	{
	case WM_INITDIALOG:

		::SetWindowTextA(::GetDlgItem(hwndDlg, IDC_STATIC_ABOUT_BERT), ABOUT_BERT_TEXT);
		::SetWindowTextA(::GetDlgItem(hwndDlg, IDC_STATIC_ABOUT_R), ABOUT_R_TEXT);

		CenterWindow(hwndDlg, ::GetParent(hwndDlg));
		break;

	case WM_COMMAND:

		switch (LOWORD(wParam))
		{
		case IDOK:
		case IDCANCEL:
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

		case IDCANCEL:
			EndDialog(hwndDlg, wParam);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

