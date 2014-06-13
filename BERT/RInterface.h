
#ifndef __RINTERFACE_H
#define __RINTERFACE_H

#ifdef EXCEL13
#include "XLCALL-13.H"
#else
#include "XLCALL-12.H"
#endif // #ifdef EXCEL13

typedef std::vector< std::string > SVECTOR;
typedef std::vector< SVECTOR > FDVECTOR;

extern FDVECTOR RFunctions;

void RInit();
void RShutdown();

LPXLOPER12 RExec(LPXLOPER12 code);
bool RExec2(LPXLOPER12 rslt, std::string &funcname, std::vector< LPXLOPER12 > &args);
int UpdateR(std::string &str);
void MapFunctions();

#endif // #ifndef __RINTERFACE_H
