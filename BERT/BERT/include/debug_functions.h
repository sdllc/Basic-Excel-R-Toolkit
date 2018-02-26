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

