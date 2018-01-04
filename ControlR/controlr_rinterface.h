/*
 * controlR
 * Copyright (C) 2016 Structured Data, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included 
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * this header is the interface between the R and libuv sides, each of which
 * keeps its API to itself
 */
 
#ifndef __CONTROLR_RINTERFACE_H
#define __CONTROLR_RINTERFACE_H

#include <iostream>
#include <vector>
#include <string>
#include <list>

/**
 * this is a reimplementation of R's parse status eunm; that one is 
 * in a file including R API types which we don't want to pass across
 * to the libuv side, so we can't include the real enum.
 */
typedef enum {
    PARSE2_NULL,
    PARSE2_OK,
    PARSE2_INCOMPLETE,
    PARSE2_ERROR,
    PARSE2_EOF
} 
PARSE_STATUS_2;

/**
 * called by the R read console method (see comments for rationale)
 */
int InputStreamRead(const char *prompt, char *buf, int len, int addtohistory, bool is_continuation);

/** 
 * handler for messages coming from the R side (js client package)
 */
void DirectCallback( const char *channel, const char *data, bool buffer );

/**
 * console output handler
 */
void ConsoleMessage( const char *buf, int len = -1, int flag = 0 );

/**
 * run R via its internal repl
 */	
int RLoop( const char *rhome, const char *ruser, int argc, char **argv );

/**
 * break uses signals on linux, flag on windows
 */
void RSetUserBreak( const char *msg = 0 );

/**
 * idle or periodic event handler -- implementation is platform-specific
 */
void RTick();

/**
 * periodically test the connection
 */
//void heartbeat();

#endif // #ifndef __CONTROLR_RINTERFACE_H
