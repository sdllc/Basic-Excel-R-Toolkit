
#ifndef __BERT_H
#define __BERT_H

#include <string>
#include <vector>

#ifdef EXCEL13
#include "XLCALL-13.H"
#else
#include "XLCALL-12.H"
#endif // #ifdef EXCEL13

#define DLLEX extern "C" __declspec(dllexport)

static LPSTR funcTemplates[][16] = {
	{ "UpdateScript", "UU", "UpdateScript", "R Code", "1", "BERT", "", "99", "Update Script", "", "", "", "", "", "", "" },
	{ 0 }
};

#define MAX_FUNCTION_COUNT 100

LPXLOPER12 UpdateScript(LPXLOPER12 script);

/**
 * de-register any functions we have previously registered, via the local vector
 */
void UnregisterFunctions();

bool RegisterBasicFunctions();

/**
* register functions (dynamic)
*/
bool RegisterAddinFunctions();

/**
 * generic call dispatcher function, exported from dll
 */
__inline LPXLOPER12 BERTFunctionCall(
	int findex,
	LPXLOPER12 input_0 = 0
	, LPXLOPER12 input_1 = 0
	, LPXLOPER12 input_2 = 0
	, LPXLOPER12 input_3 = 0
	, LPXLOPER12 input_4 = 0
	, LPXLOPER12 input_5 = 0
	, LPXLOPER12 input_6 = 0
	, LPXLOPER12 input_7 = 0
	, LPXLOPER12 input_8 = 0
	, LPXLOPER12 input_9 = 0
	, LPXLOPER12 input_10 = 0
	, LPXLOPER12 input_11 = 0
	, LPXLOPER12 input_12 = 0
	, LPXLOPER12 input_13 = 0
	, LPXLOPER12 input_14 = 0
	, LPXLOPER12 input_15 = 0
	);

#define BFC(num) \
LPXLOPER12 BERTFunctionCall ## num ( \
	LPXLOPER12 input_0 = 0 \
	, LPXLOPER12 input_1 = 0 \
	, LPXLOPER12 input_2 = 0 \
	, LPXLOPER12 input_3 = 0 \
	, LPXLOPER12 input_4 = 0 \
	, LPXLOPER12 input_5 = 0 \
	, LPXLOPER12 input_6 = 0 \
	, LPXLOPER12 input_7 = 0 \
	, LPXLOPER12 input_8 = 0 \
	, LPXLOPER12 input_9 = 0 \
	, LPXLOPER12 input_10 = 0 \
	, LPXLOPER12 input_11 = 0 \
	, LPXLOPER12 input_12 = 0 \
	, LPXLOPER12 input_13 = 0 \
	, LPXLOPER12 input_14 = 0 \
	, LPXLOPER12 input_15 = 0 \
	){ return BERTFunctionCall( num-1000, input_0, input_1, input_2, input_3, input_4, input_5, input_6, input_7, input_8, input_9, input_10, input_11, input_12, input_13, input_14, input_15 ); }

#endif // #ifndef __BERT_H
