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
	PARSE2_EXTERNAL_ERROR,
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
	SCC_NAMES = 5,
	SCC_WATCH_NOTIFY = 6,
	SCC_BREAK = 7,
	SCC_CONSOLE_WIDTH = 8,
	SCC_AUTOCOMPLETE = 9,

	SCC_LAST
}
SAFECALL_CMD;

class Util
{
public:

	static int icasecompare(const std::string& a, const std::string& b)
	{
		unsigned int len = a.length();
		if (b.length() != len) return 1;
		for (int i = 0; i < len; ++i) if (tolower(a[i]) != tolower(b[i])) return 1;
		return 0;
	}

	static std::string trim(const std::string& str, const std::string& whitespace = " \r\n\t")
	{
		const auto strBegin = str.find_first_not_of(whitespace);
		if (strBegin == std::string::npos)
			return ""; // no content

		const auto strEnd = str.find_last_not_of(whitespace);
		const auto strRange = strEnd - strBegin + 1;

		return str.substr(strBegin, strRange);
	}

	//
	// see
	// http://stackoverflow.com/questions/3418231/replace-part-of-a-string-with-another-string
	//
	static void replaceall(std::string& str, const std::string& from, const std::string& to) 
	{
		if (from.empty()) return;
		size_t start_pos = 0;
		while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
			str.replace(start_pos, from.length(), to);
			start_pos += to.length(); 
		}
	}

	static void replacechar(std::string& str, const std::string& from, const std::string& to)
	{
		if (from.empty()) return;
		size_t start_pos = 0;
		while ((start_pos = str.find_first_of(from, start_pos)) != std::string::npos) {
			str.replace(start_pos, 1, to);
			start_pos += to.length();
		}
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

	//
	// thanks to
	// http://stackoverflow.com/questions/874134/find-if-string-ends-with-another-string-in-c
	//
	static bool endsWith(std::string const &fullString, std::string const &ending) {
		if (fullString.length() >= ending.length()) {
			return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
		}
		else {
			return false;
		}
	}

};

#endif // #ifndef __UTIL_H


