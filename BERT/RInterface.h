
#ifndef __RINTERFACE_H
#define __RINTERFACE_H

#include "XLCALL.H"

typedef std::vector< std::string > SVECTOR;
typedef std::vector< SVECTOR > FDVECTOR;

extern FDVECTOR RFunctions;

void RInit();
void RShutdown();

LPXLOPER12 RExec(LPXLOPER12 code);
bool RExec2(LPXLOPER12 rslt, std::string &funcname, std::vector< LPXLOPER12 > &args);
int UpdateR(std::string &str);
void MapFunctions();
void LoadStartupFile();


#endif // #ifndef __RINTERFACE_H
