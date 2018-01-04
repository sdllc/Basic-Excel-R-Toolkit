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
 * this header is for R code common to linux and win32 implmentations, but 
 * which is shielded from the non-R portion of the code (using R types or APIs).
 */
 
#ifndef __CONTROLR_COMMON_H
#define __CONTROLR_COMMON_H

#ifdef WIN32

	#define Win32
	#include <windows.h>

#else // #ifdef WIN32

	#define R_INTERFACE_PTRS

#endif // #ifdef WIN32

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>
#include <iostream>

#include <Rversion.h>
#include <Rinternals.h>
#include <Rembedded.h>

#ifdef WIN32

	#include <graphapp.h>
	#include <R_ext\RStartup.h>

#else // #ifdef WIN32

	#include <signal.h>
	#include <unistd.h>
	#include <Rinterface.h>

#endif // #ifdef WIN32

#include <R_ext/Parse.h>
#include <R_ext/Rdynload.h>

// try to store fuel now, you jerks
#undef clear
#undef length

SEXP ExternalCallback(int, void*, void *);
SEXP RCallback(SEXP, SEXP);
SEXP COMCallback(SEXP, SEXP, SEXP, SEXP);

#endif // #ifndef __CONTROLR_COMMON_H


