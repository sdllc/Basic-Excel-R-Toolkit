#pragma once

#define MAX_FUNCTIONS 2048
#define MAX_ARGS 16

static LPWSTR funcTemplates[][16] = {

	{ L"BERT_Call", L"UQQQQQQQQQQQQQQQQQ", L"BERT.Call", L"R Function, Argument", L"1", L"BERT", L"", L"99", L"", L"", L"", L"", L"", L"", L"", L"" },
    { L"BERT_Exec", L"UQ", L"BERT.Exec", L"R Code", L"1", L"BERT", L"", L"98", L"Exec R Code", L"", L"", L"", L"", L"", L"", L"" },
    { L"BERT_Console", L"J", L"BERT.Console", L"", L"2", L"BERT", L"", L"97", L"", L"", L"", L"", L"", L"", L"", L"" },
    { L"BERT_ContextSwitch", L"J", L"BERT.ContextSwitch", L"", L"2", L"BERT", L"", L"96", L"", L"", L"", L"", L"", L"", L"", L"" },

	/*
	// type 2 functions

	{ L"BERT_UpdateScript", L"UU#", L"BERT.UpdateScript", L"R Code", L"2", L"BERT", L"", L"100", L"Update Script", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_RExec", L"UU", L"BERT.Exec", L"R Code", L"2", L"BERT", L"", L"99", L"Exec R Code", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_RCall", L"UUUUUUUUUU", L"BERT.Call", L"R Function, Argument", L"2", L"BERT", L"", L"98", L"Exec R Code", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_Configure", L"A#", L"BERT.Configure", L"", L"2", L"BERT", L"", L"97", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_Console", L"A#", L"BERT.Console", L"", L"2", L"BERT", L"", L"96", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_HomeDirectory", L"A#", L"BERT.Home", L"", L"2", L"BERT", L"", L"95", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_StartupFolder", L"A#", L"BERT.StartupFolder", L"", L"2", L"BERT", L"", L"95", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_InstallPackages", L"A#", L"BERT.InstallPackages", L"", L"2", L"BERT", L"", L"94", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_Reload", L"A#", L"BERT.Reload", L"", L"2", L"BERT", L"", L"93", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_About", L"A#", L"BERT.About", L"", L"2", L"BERT", L"", L"92", L"", L"", L"", L"", L"", L"", L"", L"" },
	{ L"BERT_SafeCall", L"JJUU#", L"BERT.SafeCall", L"", L"2", L"BERT", L"", L"91", L"", L"", L"", L"", L"", L"", L"", L"" },

	// type 1 functions

	{ L"BERT_Volatile", L"UU!", L"BERT.Volatile", L"", L"1", L"BERT", L"", L"80", \
	L"Forces calculation to be volatile (recalculate on F9).", \
	L"", L"", L"", L"", L"", L"", L"" },
	*/

	{ 0 }
};


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

