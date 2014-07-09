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

#ifndef __UTIL_H
#define __UTIL_H

// who does this?
#ifdef length
#undef length
#endif

#ifdef clear
#undef clear
#endif

typedef std::vector< std::string > SVECTOR;

// this is a map of an R enum.  there's no reason 
// to do this other than to enfore separation between
// R and non-R parts of the code, which is useful.

typedef enum
{
	PARSE2_NULL,
	PARSE2_OK,
	PARSE2_INCOMPLETE,
	PARSE2_ERROR,
	PARSE2_EOF
}
PARSE_STATUS_2;

typedef enum
{
	SCC_NULL = 0,
	SCC_EXEC = 1,
	SCC_RELOAD_STARTUP = 2,
	SCC_INSTALLPACKAGES = 3,
	SCC_CALLTIP = 4,

	SCC_LAST
}
SAFECALL_CMD;

class Util
{
public:

	static std::string trim(const std::string& str, const std::string& whitespace = " \r\n\t")
	{
		const auto strBegin = str.find_first_not_of(whitespace);
		if (strBegin == std::string::npos)
			return ""; // no content

		const auto strEnd = str.find_last_not_of(whitespace);
		const auto strRange = strEnd - strBegin + 1;

		return str.substr(strBegin, strRange);
	}

	static SVECTOR & split(const std::string &s, char delim, int minLength, SVECTOR &elems, bool ftrim = false)
	{
		std::stringstream ss(s);
		std::string item;
		while (std::getline(ss, item, delim))
		{
			if (ftrim) item = trim(item);
			if (!item.empty() && item.length() >= minLength) elems.push_back(item);
		}
		return elems;
	}

};

#endif // #ifndef __UTIL_H


