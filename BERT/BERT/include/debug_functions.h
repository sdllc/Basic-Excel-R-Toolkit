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

#include <iostream>
#include <sstream>

#ifdef _DEBUG

int DebugOut(const char *fmt, ...);

// thanks to 
// https://stackoverflow.com/questions/16703835/visual-studio-how-can-i-see-cout-output-in-a-non-console-application
//
// although using streams may be preferable from a development standpoint, there's a drawback
// in that if we start using cout/cerr for development logging, these won't go away in release
// builds. the benefit of using the old DebugOut function is that it can get completely removed 
// by the preprocessor. 
//
// TODO: stream wrappers that can get scrubbed?
//
class dbg_stream_for_stdio : public std::stringbuf
{
public:
  dbg_stream_for_stdio(const std::string &prefix = "") {}
  ~dbg_stream_for_stdio() { sync(); }
  int sync()
  {
    ::OutputDebugStringA(str().c_str());
    str(std::string()); 
    return 0;
  }
public:
  static void InitStreams();
};

#else 
#define DebugOut(fmt, ...){}
#endif

