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

#ifndef __CONSOLE_H
#define __CONSOLE_H

#define SCINTILLA_FONT_NAME			"Consolas"
#define SCINTILLA_FONT_SIZE			10
#define SCINTILLA_R_TEXT_COLOR		RGB(0, 0, 128)
#define SCINTILLA_USER_TEXT_COLOR	RGB(0, 128, 0)
#define SCINTILLA_BACK_COLOR		RGB(255, 255, 255)

#define DEFAULT_PROMPT		"> "
#define CONTINUATION_PROMPT	"+ "

#define WM_CLOSE_CONSOLE		(WM_USER + 1001)
#define WM_CLOSE_CONSOLE_ASYNC	(WM_USER + 1002)
#define WM_REBUILD_WORDLISTS	(WM_USER + 1003)
#define WM_APPEND_LOG			(WM_USER + 1004)
#define WM_SET_PTR				(WM_USER + 1005)
#define WM_CALL_COMPLETE		(WM_USER + 1006)
#define WM_CLEAR_BUFFER			(WM_USER + 1007)
#define WM_CLEANUP_PASTE		(WM_USER + 1008)
#define WM_PASTE_LINE			(WM_USER + 1009)

#define TIMERID_FLUSHBUFFER		(100)

#define MIN_WORD_LENGTH				3
#define MAX_AUTOCOMPLETE_LIST_LEN	10

void ConsoleDlg(HINSTANCE hInst);
void AppendLog(const char *buffer, int style = 1, int checkoverlap = 1);
void RunThreadedConsole(HWND w);
void SetConsoleDefaults();

void UpdateConsoleWidth(bool force = false);


#endif // #ifndef __CONSOLE_H
