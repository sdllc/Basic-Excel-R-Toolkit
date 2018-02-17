
#ifndef __CONSOLE_GRAPHICS_DEVICE_H
#define __CONSOLE_GRAPHICS_DEVICE_H

// FIXME: some of the R internals use functions that windows declares
// deprecated for security. move this into an isolated lib so we don't
// have to deal with that.

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// we may be able to redefine these interfaces, as long as they're the
// same shape. probably fragile but still maybe preferable.

//#include <Rconfig.h>
#include <Rinternals.h>
//#include <R_ext/Rdynload.h>
#include <R_ext/GraphicsEngine.h>

#ifdef length
#undef length
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <iostream>
#include <sstream>
#include <string>

SEXP CreateConsoleDevice(void *p, const std::string &type);

#endif // #ifndef __CONSOLE_GRAPHICS_DEVICE_H

