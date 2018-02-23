#pragma once

#include <fstream>
#include <iostream>

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include <math.h>
#include <ctype.h>
#include <inttypes.h>

#include "variable.pb.h"
#include "string_utilities.h"
#include "message_utilities.h"

typedef enum {
  Error = 0,
  Success,
  Incomplete
}
ExecResult;

/** startup */
void JuliaInit();

/** shutdown */
void JuliaShutdown();

/** get version so we can gate/limit */
void JuliaGetVersion(int32_t *major, int32_t *minor, int32_t *patch);

/** exec in a shell context; as if we're typing at the repl */
ExecResult JuliaShellExec(const std::string &command, const std::string &shell_buffer);

/** returns a list of functions exported to excel */
void ListScriptFunctions(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call);

/** runs arbitrary julia code */
void JuliaExec(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call);

/** runs julia function by name, optionally with arguments */
void JuliaCall(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call);

/** second-level init */
bool JuliaPostInit();

/** reads source file; for julia this uses `import` */
bool ReadSourceFile(const std::string &file, bool notify = false);

bool Callback(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response);

/** 
 * send message to the console. in julia, stdio is handled separately. console
 * messages are used for notifications and non-text output (e.g. images)
 */
void PushConsoleMessage(google::protobuf::Message &message);
