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

#ifndef __STRING_CONSTANTS_H
#define __STRING_CONSTANTS_H

#define ABOUT_BERT_TEXT \
	L"Basic Excel R Toolkit (BERT) v " BERT_VERSION L"\r\n" \
	L"Copyright (C) 2014 Structured Data, LLC"

#define BERT_LINK_TEXT		L"bert-toolkit.com"
#define BERT_LINK			L"http://" BERT_LINK_TEXT

#define ABOUT_R_TEXT \
	L"Includes R version 3.1.1 (2014-07-10) -- \"Sock it to Me\"\r\n" \
	L"Copyright (C) 2014 The R Foundation for Statistical Computing"

#define R_LINK_TEXT			L"www.r-project.org"
#define R_LINK				L"http://" R_LINK_TEXT

#define ABOUT_SCINTILLA_TEXT \
	L"Includes Scintilla text component\r\n" \
	L"Copyright 1998 - 2014 by Neil Hodgson <neilh@scintilla.org>"

#define SCINTILLA_LINK_TEXT	L"scintilla.org"
#define SCINTILLA_LINK		L"http://" SCINTILLA_LINK_TEXT

#define PARSE_ERROR_MESSAGE "(parse error)\n"
#define EXTERNAL_ERROR_MESSAGE "(an error occurred processing the command.  is excel busy?)\n"


#endif // #ifndef __STRING_CONSTANTS_H

