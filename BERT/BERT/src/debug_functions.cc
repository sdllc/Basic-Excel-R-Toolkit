
#include "stdafx.h"

#ifdef _DEBUG

int DebugOut(const char *fmt, ...)
{
	static char msg[1024 * 32]; // ??
	int ret;
	va_list args;
	va_start(args, fmt);
	ret = vsprintf_s(msg, fmt, args);
	OutputDebugStringA(msg);
	va_end(args);
	return ret;
}

dbg_stream_for_stdio g_DebugStreamFor_cout;
dbg_stream_for_stdio g_DebugStreamFor_cerr;

void dbg_stream_for_stdio::InitStreams() {
  std::cout.rdbuf(&g_DebugStreamFor_cout);
  std::cerr.rdbuf(&g_DebugStreamFor_cerr);
}

#endif

