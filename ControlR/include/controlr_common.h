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

SEXP RCallback(SEXP, SEXP);
SEXP COMCallback(SEXP, SEXP, SEXP, SEXP, SEXP);

extern "C" {

  // loop functions
  extern void setup_Rmainloop();
  extern void run_Rmainloop();

  // in case we want to call these programatically (TODO)
  extern void R_RestoreGlobalEnvFromFile(const char *, Rboolean);
  extern void R_SaveGlobalEnvToFile(const char *);

  // for win32
  extern void R_ProcessEvents(void);

  extern void Rf_PrintWarnings();
  extern Rboolean R_Visible;

};

#endif // #ifndef __CONTROLR_COMMON_H


