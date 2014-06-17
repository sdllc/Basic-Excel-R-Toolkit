
#ifndef __DIALOGS_H
#define __DIALOGS_H

#define DIALOG_RESULT_TYPE BOOL

DIALOG_RESULT_TYPE CALLBACK ConsoleDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
DIALOG_RESULT_TYPE CALLBACK OptionsDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

void ConsoleDlg(HINSTANCE hInst);
void AppendLog(const char *buffer);



#endif // #ifndef __DIALOGS_H


