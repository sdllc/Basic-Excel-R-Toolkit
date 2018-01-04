#pragma once

#include <string>
#include <sstream>
#include <vector>

typedef std::vector<std::string> STRINGVECTOR;

class Util {
public:

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

	static std::string trim(const std::string& str, const std::string& whitespace = " \r\n\t")
	{
		const auto strBegin = str.find_first_not_of(whitespace);
		if (strBegin == std::string::npos)
			return ""; // no content

		const auto strEnd = str.find_last_not_of(whitespace);
		const auto strRange = strEnd - strBegin + 1;

		return str.substr(strBegin, strRange);
	}

	static STRINGVECTOR & split(const std::string &s, char delim, int minLength, STRINGVECTOR &elems, bool ftrim = false)
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
