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
  
  // the seventh field is xlfRegister's shortcut key, and it is deliberately
  // empty. a shortcut registered there does fire, but excel then looks the
  // command up in workbook scope and cannot find it -- "cannot run the
  // macro '!BERT.Console'" -- because an xll command lives in the add-in's
  // context. the console shortcut is bound with ON.KEY instead, which
  // resolves the same way ExecuteExcel4Macro does. see BERT::BindConsoleKey.
  { L"BERT_Console", L"J", L"BERT.Console", L"", L"2", L"BERT", L"", L"95", L"", L"", L"", L"", L"", L"", L"", L"" },
  { L"BERT_ContextSwitch", L"JQ", L"BERT.ContextSwitch", L"", L"2", L"BERT", L"", L"94", L"", L"", L"", L"", L"", L"", L"", L"" },
  { L"BERT_UpdateFunctions", L"J", L"BERT.UpdateFunctions", L"", L"2", L"BERT", L"", L"93", L"", L"", L"", L"", L"", L"", L"", L"" },
  { L"BERT_ButtonCallback", L"JQQ", L"BERT.ButtonCallback", L"", L"2", L"BERT", L"", L"92", L"", L"", L"", L"", L"", L"", L"", L"" },

	{ 0 }
};

/**
 * how many arguments a language function can take from a cell.
 *
 * excel fixes a function's arity when it is registered, and it calls a
 * distinct exported entry point per function, so every dispatcher in the
 * pool below is compiled with this many parameters whether the function
 * behind it wants them or not. unused ones arrive as xltypeMissing and are
 * trimmed. that means the cost of raising this is code size in the xll --
 * a couple of hundred bytes per dispatcher -- and nothing else.
 *
 * excel's own ceiling is 254: the type string handed to xlfRegister is
 * capped at 255 characters, one of which is the return type. registration
 * also passes one help string per argument, and xlfRegister takes at most
 * 255 arguments in total, which is the same bound.
 */
#define BERT_MAX_ARGUMENTS 64

/**
 * the argument list after the first, in whichever shape is needed. written
 * once so the four places that repeat it cannot drift apart. these carry a
 * leading comma, so input_0 is spelled out wherever they are used.
 */
#define BERT_ARGUMENTS_AFTER_FIRST(X) \
  X(1)  X(2)  X(3)  X(4)  X(5)  X(6)  X(7)  X(8)  X(9)  X(10) \
  X(11) X(12) X(13) X(14) X(15) X(16) X(17) X(18) X(19) X(20) \
  X(21) X(22) X(23) X(24) X(25) X(26) X(27) X(28) X(29) X(30) \
  X(31) X(32) X(33) X(34) X(35) X(36) X(37) X(38) X(39) X(40) \
  X(41) X(42) X(43) X(44) X(45) X(46) X(47) X(48) X(49) X(50) \
  X(51) X(52) X(53) X(54) X(55) X(56) X(57) X(58) X(59) X(60) \
  X(61) X(62) X(63)

#define BERT_ARGUMENT_DECLARE_DEFAULT(n) , LPXLOPER12 input_ ## n = 0
#define BERT_ARGUMENT_DECLARE(n) , LPXLOPER12 input_ ## n
#define BERT_ARGUMENT_NAME(n) , input_ ## n

/**
 * the type string BERT.Call registers with: "U" for the return, then one
 * "Q" per slot. it takes the function name as its first argument, so it has
 * one more slot than BERT_MAX_ARGUMENTS. the static_assert below keeps this
 * literal honest; per-language functions build theirs at registration.
 */
#define BERT_Q16 L"QQQQQQQQQQQQQQQQ"
#define BERT_CALL_TYPE_TEXT L"UQ" BERT_Q16 BERT_Q16 BERT_Q16 BERT_Q16

static_assert(sizeof(BERT_CALL_TYPE_TEXT) / sizeof(wchar_t) - 1 == BERT_MAX_ARGUMENTS + 2,
  "BERT_CALL_TYPE_TEXT does not have one Q per argument (plus the function name)");

static LPWSTR callTemplates[][16] = {
  { L"BERT_CallLanguage_", BERT_CALL_TYPE_TEXT, L"BERT.Call", L"Function, Argument", L"2", L"BERT", L"", L"99", L"", L"", L"", L"", L"", L"", L"", L"" },
  { L"BERT_ExecLanguage_", L"UQ", L"BERT.Exec", L"Code", L"2", L"BERT", L"", L"97", L"Exec Language Code", L"", L"", L"", L"", L"", L"", L"" }
};

/** exported function */
int BERT_SetPointers(ULONG_PTR excel_pointer, ULONG_PTR ribbon_pointer);

/** exported function */
int BERT_Console();

/** exported function */
int BERT_ContextSwitch(LPXLOPER12 argument);

__inline LPXLOPER12 BERT_Call_Generic(uint32_t language_index, LPXLOPER12 func,
  LPXLOPER12 input_0
  BERT_ARGUMENTS_AFTER_FIRST(BERT_ARGUMENT_DECLARE));

#define BCALL(num) \
LPXLOPER12 BERT_CallLanguage_ ## num ( \
  LPXLOPER12 func = 0 \
  , LPXLOPER12 input_0 = 0 \
  BERT_ARGUMENTS_AFTER_FIRST(BERT_ARGUMENT_DECLARE_DEFAULT) \
){ return BERT_Call_Generic( num - 1000, func, input_0 BERT_ARGUMENTS_AFTER_FIRST(BERT_ARGUMENT_NAME) ); }

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
  BERT_ARGUMENTS_AFTER_FIRST(BERT_ARGUMENT_DECLARE_DEFAULT)
);

#define BFC(num) \
LPXLOPER12 BERTFunctionCall ## num ( \
  LPXLOPER12 input_0 = 0 \
  BERT_ARGUMENTS_AFTER_FIRST(BERT_ARGUMENT_DECLARE_DEFAULT) \
){ return BERTFunctionCall( num-1000, input_0 BERT_ARGUMENTS_AFTER_FIRST(BERT_ARGUMENT_NAME) ); }

