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

typedef enum {
  Error = 0,
  Success,
  Incomplete
}
ExecResult;

void JuliaInit();
void JuliaShutdown();
void JuliaGetVersion(int32_t *major, int32_t *minor, int32_t *patch);

// void julia_exec();

ExecResult JuliaShellExec(const std::string &command, const std::string &shell_buffer);

void ListScriptFunctions(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call);

void JuliaExec(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call);
void JuliaCall(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call);

bool JuliaPostInit();
bool ReadSourceFile(const std::string &file);
