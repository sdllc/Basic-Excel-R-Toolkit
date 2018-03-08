/**
 * Copyright (c) 2017-2018 Structured Data, LLC
 * 
 * This file is part of BERT.
 *
 * BERT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BERT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with BERT.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#pragma once

#include <string>
#include <vector>
#include <sstream>

/**
 * some string utilities. these have varying semantics based on when they
 * were written and what they were written for.
 *
 * FIXME: normalize input/output/return semantics
 */
class StringUtilities {

public:

  //
  // thanks to
  // http://stackoverflow.com/questions/874134/find-if-string-ends-with-another-string-in-c
  //
  static bool EndsWith(std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
      return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    }
    else {
      return false;
    }
  }

  /**
   * icase string comparison, ignoring locale issues (this is for windows paths)
   */
  static int ICaseCompare(const std::string &a, const std::string &b) {
    size_t len = a.length();
    if (len != b.length()) return 1;
    for (size_t i = 0; i < len; i++) {
      if (toupper(a[i]) != toupper(b[i])) return 2;
    }
    return 0;
  }
  
  /**
   * escape backslashes. returns a new string.
   */
  static std::string EscapeBackslashes(const std::string &str) {

    std::stringstream new_string;
    std::stringstream old_string(str);
    std::string part;

    while (std::getline(old_string, part, '\\'))
    {
      new_string << part;
      if (!old_string.eof()) new_string << "\\\\";
    }

    return new_string.str();
  }

  static std::string Trim(const std::string& str, const std::string& whitespace = " \r\n\t")
  {
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
      return ""; // no content

    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
  }

  static std::vector<std::string> &Split(const std::string &s, char delim, int minLength, std::vector<std::string> &elems, bool ftrim = false)
  {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
    {
      if (ftrim) item = Trim(item);
      if (!item.empty() && item.length() >= minLength) elems.push_back(item);
    }
    return elems;
  }
};
