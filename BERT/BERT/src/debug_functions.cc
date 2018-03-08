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

