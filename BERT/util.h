
#ifndef __UTIL_H
#define __UTIL_H

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

#endif // #ifndef __UTIL_H


