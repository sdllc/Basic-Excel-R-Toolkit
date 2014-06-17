
#ifndef __RINTERFACE_H
#define __RINTERFACE_H

#include "XLCALL.H"

#define MAX_LOGLIST_SIZE	80

typedef std::vector< std::string > SVECTOR;
typedef std::vector< SVECTOR > FDVECTOR;

// this is a map of an R enum.  there's no reason 
// to do this other than to enfore separation between
// R and non-R parts of the code, which is useful.

typedef enum
{
	PARSE2_NULL,
	PARSE2_OK,
	PARSE2_INCOMPLETE,
	PARSE2_ERROR,
	PARSE2_EOF
} 
PARSE_STATUS_2;

extern FDVECTOR RFunctions;

void RInit();
void RShutdown();

LPXLOPER12 RExec(LPXLOPER12 code);
bool RExec2(LPXLOPER12 rslt, std::string &funcname, std::vector< LPXLOPER12 > &args);
void RExecString( const char *buffer, int *err = 0, PARSE_STATUS_2 *parseErr = 0 );
int UpdateR(std::string &str);
void MapFunctions();
void LoadStartupFile();

short InstallPackages();

#endif // #ifndef __RINTERFACE_H
