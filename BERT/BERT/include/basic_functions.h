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

#pragma once

#define MAX_FUNCTIONS 2048
#define MAX_ARGS 16

// FIXME: add dynamic Exec and Call functions based on loaded languages
//
// BUT NOTE: we probably need to use constant IDs for those functions, in the event
// excel refers to them by ID and not by name.
//
// HOWEVER: I'm fairly certain that Excel never does that, at least not in current
// excel. perhaps in previous versions (going way back). also (if it did cache numbers)
// that would cause all sorts of problems with BERT that we haven't yet noticed, so I 
// think we're safe in the assumption that these are not used anymore.

static LPWSTR funcTemplates[][16] = {

  // these are constructed at runtime
  
  { L"BERT_Console", L"J", L"BERT.Console", L"", L"2", L"BERT", L"", L"95", L"", L"", L"", L"", L"", L"", L"", L"" },
  { L"BERT_ContextSwitch", L"JQ", L"BERT.ContextSwitch", L"", L"2", L"BERT", L"", L"94", L"", L"", L"", L"", L"", L"", L"", L"" },
  { L"BERT_UpdateFunctions", L"J", L"BERT.UpdateFunctions", L"", L"2", L"BERT", L"", L"93", L"", L"", L"", L"", L"", L"", L"", L"" },
  { L"BERT_ButtonCallback", L"JQQ", L"BERT.ButtonCallback", L"", L"2", L"BERT", L"", L"92", L"", L"", L"", L"", L"", L"", L"", L"" },

	{ 0 }
};

static LPWSTR callTemplates[][16] = {
  { L"BERT_CallLanguage_", L"UQQQQQQQQQQQQQQQQQ", L"BERT.Call", L"Function, Argument", L"2", L"BERT", L"", L"99", L"", L"", L"", L"", L"", L"", L"", L"" },
  { L"BERT_ExecLanguage_", L"UQ", L"BERT.Exec", L"Code", L"2", L"BERT", L"", L"97", L"Exec Language Code", L"", L"", L"", L"", L"", L"", L"" }
};

/** exported function */
int BERT_SetPointers(ULONG_PTR excel_pointer, ULONG_PTR ribbon_pointer);

/** exported function */
int BERT_Console();

/** exported function */
int BERT_ContextSwitch(LPXLOPER12 argument);

__inline LPXLOPER12 BERT_Call_Generic(uint32_t language_index, LPXLOPER12 func,
  LPXLOPER12 arg0, LPXLOPER12 arg1, LPXLOPER12 arg2, LPXLOPER12 arg3,
  LPXLOPER12 arg4, LPXLOPER12 arg5, LPXLOPER12 arg6, LPXLOPER12 arg7,
  LPXLOPER12 arg8, LPXLOPER12 arg9, LPXLOPER12 arg10, LPXLOPER12 arg11,
  LPXLOPER12 arg12, LPXLOPER12 arg13, LPXLOPER12 arg14, LPXLOPER12 arg15);

#define BCALL(num) \
LPXLOPER12 BERT_CallLanguage_ ## num ( \
  LPXLOPER12 func = 0 \
  , LPXLOPER12 input_0 = 0 \
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
){ return BERT_Call_Generic( num - 1000, func, input_0, input_1, input_2, input_3, input_4, input_5, input_6, input_7, input_8, input_9, input_10, input_11, input_12, input_13, input_14, input_15 ); }

__inline LPXLOPER12 BERT_Exec_Generic(uint32_t language_index, LPXLOPER12 code);

#define BEXEC(num) \
LPXLOPER12 BERT_ExecLanguage_ ## num ( \
  LPXLOPER12 code = 0 \
){ return BERT_Exec_Generic( num - 1000, code ); }

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

