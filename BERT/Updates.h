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

#ifndef __UPDATES_H
#define __UPDATES_H

#include "BERT_Version.h"

typedef enum 
{
	UC_NULL_RESULT = 0,
	UC_UPDATE_CHECK_FAILED = 1,
	UC_NEW_VERSION_AVAILABLE = 2,
	UC_UP_TO_DATE = 3,
	UC_YOURS_IS_NEWER = 4
} 
UPDATE_CHECK_RESULT;

#define USER_AGENT_STRING L"sdllc/Basic-Excel-R-Toolkit/v" BERT_VERSION

#define GITHUB_HOST L"api.github.com"
#define RELEASE_URL L"/repos/sdllc/Basic-Excel-R-Toolkit/releases/latest"

// this is the URL for downloading (linked in the dialog)
#define LATEST_RELEASE_URL L"https://github.com/StructuredDataLLC/Basic-Excel-R-Toolkit/releases/latest"

int checkForUpdates(std::string &tag);

#endif // #ifndef __UPDATES_H
