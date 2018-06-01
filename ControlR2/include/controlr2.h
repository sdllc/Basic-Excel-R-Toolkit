
#ifndef __CONTROLR2_H
#define __CONTROLR2_H

#include "uv.h"
#include "variable.pb.h"
#include "string_utilities.h"
#include "message_utilities.h"

#ifdef _WIN32
#include <Windows.h>
#else // #ifdef _WIN32
#include <unistd.h>
#endif // #ifdef _WIN32

// results of system call
#define SYSTEMCALL_OK           0
#define SYSTEMCALL_SHUTDOWN    -1

/**
 * calls an R function, by name, possibly with arguments
 */
BERTBuffers::CallResponse& RCall(BERTBuffers::CallResponse &rsp, const BERTBuffers::CallResponse &call);

/**
 * runs arbitrary R code 
 */
BERTBuffers::CallResponse& RExec(BERTBuffers::CallResponse &rsp, const BERTBuffers::CallResponse &call);

/**
 * returns version as reported by the loaded R library
 * FIXME: move declaration
 */
void RGetVersion(int32_t *major, int32_t *minor, int32_t *patch);

/**
 *
 */
bool Callback(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response);

/**
 * callback _from_ the console
 */
bool ConsoleCallback(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response);

bool SpreadsheetCallback(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response);

/**
 * send a message to the console. this includes stdio as well as graphics and notifications
 */
void PushConsoleMessage(google::protobuf::Message &message);

/** similar, push message to the spreadsheet. graphics. */
void PushSpreadsheetMessage(google::protobuf::Message &message);

/**
* idle or periodic event handler -- implementation is platform-specific
*/
void RTick();

/**
 * break uses signals on linux, flag on windows
 */
void RSetUserBreak(const char *msg = 0);

/**
* do any pending graphics updates. these are scheduled rather than real-time
* because they are expensive. it might even be worth getting them off the
* main thread (meaning out of this function) because we don't want to wait.
*/
void UpdateSpreadsheetGraphics();

#endif // #ifndef __CONTROLR2_H
