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

#ifndef __STRING_CONSTANTS_H
#define __STRING_CONSTANTS_H

#ifdef _DEBUG
#define ABOUT_BERT_TEXT \
	L"Basic Excel R Toolkit (BERT) v " BERT_VERSION L" (D)\r\n" \
	L"Copyright (C) 2014-2017 Structured Data, LLC"
#else 
#define ABOUT_BERT_TEXT \
	L"Basic Excel R Toolkit (BERT) v " BERT_VERSION L"\r\n" \
	L"Copyright (C) 2014-2017 Structured Data, LLC"
#endif

#define BERT_LINK_TEXT		L"bert-toolkit.com"
#define BERT_LINK			L"http://" BERT_LINK_TEXT

#define ABOUT_R_TEXT \
	L"Includes R version 3.4.2 (2017-09-28) -- \"Short Summer\"\r\n" \
	L"Copyright (C) 2017 The R Foundation for Statistical Computing"

#define R_LINK_TEXT			L"www.r-project.org"
#define R_LINK				L"http://" R_LINK_TEXT

#define PARSE_ERROR_MESSAGE "(parse error)\n"
#define USER_BREAK_MESSAGE "(break)\n"
#define EXTERNAL_ERROR_MESSAGE "(an error occurred processing the command.  is excel busy?)\n"

#define CONSOLE_WINDOW_TITLE L"BERT Console"

// update check messages

#define MSG_CLICK_TO_DOWNLOAD L"Click here to go to the download page."

#define MSG_UP_TO_DATE "You have the latest release"
#define MSG_UPDATE_AVAILABLE "An update is available"
#define MSG_UPDATE_CHECK_FAILED "Checking for updates failed. Please try again later."
#define MSG_YOURS_IS_NEWER "Your software is newer than the current release!"

#define FUNCTIONS_R_DEPRECATED "\n" \
	" * Please note: using the startup file is deprecated (as of v1.20).\n" \
    "   We now use a startup folder.  See the documentation on watching\n" \
	"   files for more information.\n\n"

#endif // #ifndef __STRING_CONSTANTS_H

