 
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


