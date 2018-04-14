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

#include <Windows.h>
#include <process.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <stack>

#include "variable.pb.h"
#include "string_utilities.h"
#include "pipe.h"
#include "message_utilities.h"
#include "process_exit_codes.h"

// pipe index of callback
#define CALLBACK_INDEX          0

// pipe index of primary client
#define PRIMARY_CLIENT_INDEX    1

// results of system call
#define SYSTEMCALL_OK           0
#define SYSTEMCALL_SHUTDOWN    -1

/**
 * calls an R function, by name, possibly with arguments
 */
BERTBuffers::CallResponse& RCall(BERTBuffers::CallResponse &rsp, const BERTBuffers::CallResponse &call);

/**
 * callback from user control ("button")
 */
BERTBuffers::CallResponse& ExecUserCommand(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call);

/** 
 * runs arbitrary R code
 */
BERTBuffers::CallResponse& RExec(BERTBuffers::CallResponse &rsp, const BERTBuffers::CallResponse &call);

/** 
 * returns a list of functions exported to Excel
 */
BERTBuffers::CallResponse& ListScriptFunctions(BERTBuffers::CallResponse &message);

/**
 * send a message to the console. this includes stdio as well as graphics and notifications
 */
void PushConsoleMessage(google::protobuf::Message &message);

/**
 * callback _from_ the console
 */
bool ConsoleCallback(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response);

/** 
 * reads source file. in R, this uses `source`.
 */
bool ReadSourceFile(const std::string &file, bool notify = false);

/**
 *
 */
bool Callback(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response);

/**
* this is a reimplementation of R's parse status eunm; that one is
* in a file including R API types which we don't want to pass across
* to the libuv side, so we can't include the real enum.
*/
typedef enum {
  PARSE2_NULL,
  PARSE2_OK,
  PARSE2_INCOMPLETE,
  PARSE2_ERROR,
  PARSE2_EOF
}
PARSE_STATUS_2;

/**
 * called by the R read console method (see comments for rationale)
 */
int InputStreamRead(const char *prompt, char *buf, int len, int addtohistory, bool is_continuation);

/**
 * handler for messages coming from the R side (js client package)
 */
void DirectCallback(const char *channel, const char *data, bool buffer);

/**
 * console output handler
 */
void ConsoleMessage(const char *buf, int len = -1, int flag = 0);

/**
 * run R via its internal repl
 */
int RLoop(const char *rhome, const char *ruser, int argc, char **argv);

/**
 * break uses signals on linux, flag on windows
 */
void RSetUserBreak(const char *msg = 0);

/**
 * idle or periodic event handler -- implementation is platform-specific
 */
void RTick();

/**
 * do any pending graphics updates. these are scheduled rather than real-time 
 * because they are expensive. it might even be worth getting them off the 
 * main thread (meaning out of this function) because we don't want to wait.
 */
void UpdateSpreadsheetGraphics();


/**
 * returns version as reported by the loaded R library
 */
void RGetVersion(int32_t *major, int32_t *minor, int32_t *patch);

