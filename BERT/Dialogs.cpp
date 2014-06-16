
#include "stdafx.h"
#include "BERT.h"
#include "ExcelFunctions.h"
#include "ThreadLocalStorage.h"
#include "resource.h"
#include "Dialogs.h"

#define DIALOG_RESULT_TYPE BOOL

DIALOG_RESULT_TYPE CALLBACK ConsoleDlgProc( HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	HWND hWnd = 0;
	char *szBuffer = 0;

	static RECT offsets;

	switch (message)
	{
	case WM_INITDIALOG:
		// CenterWindow(hwndDlg, ::GetParent(hwndDlg));

		hWnd = ::GetDlgItem(hwndDlg, IDC_EDIT1);
		if (hWnd)
		{
		}

		break;

	case WM_SIZING:

		hWnd = ::GetDlgItem(hwndDlg, IDC_EDIT1);
		if (hWnd)
		{
			RECT rctl;
			RECT *prect = (RECT*)lParam;
			::GetWindowRect(hWnd, &rctl);
			::MoveWindow(hWnd, 10, 10, prect->right - prect->left - 50, prect->bottom - prect->top - 100, TRUE);
		}
		break;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code)
		{
			break;
		}
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

void ConsoleDlg(HINSTANCE hInst)
{
	XLOPER12 xWnd;
	Excel12(xlGetHwnd, &xWnd, 0);
	
	// InitRichEdit();

	::DialogBox(hInst,
		MAKEINTRESOURCE(IDD_DIALOG1),
		(HWND)xWnd.val.w,
		(DLGPROC)ConsoleDlgProc);

	Excel12(xlFree, 0, 1, (LPXLOPER12)&xWnd);

}
