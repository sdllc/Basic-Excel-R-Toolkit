// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define  WINVER			0x0501

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif // #ifndef _WIN32_WINNT

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// TODO: reference additional headers your program requires here
