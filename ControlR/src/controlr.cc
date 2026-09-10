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

#include "controlr.h"
#include "convert.h"
#include "windows_api_functions.h"
#include "json11\json11.hpp"

std::string pipename;
std::string rhome;

int console_client = -1;

std::vector<Pipe*> pipes;
std::vector<HANDLE> handles; 
std::vector<std::string> console_buffer;

// flag indicates we are operating after a break; changes prompt semantics
bool user_break_flag = false;

std::stack<int> active_pipe;

std::string language_tag;

// set from the R version at startup; see R_WriteConsoleEx
bool r_console_utf8 = false;

/** debug/util function */
std::string GetLastErrorAsString(DWORD err = -1)
{
  //Get the error message, if any.
  DWORD errorMessageID = err;
  if (-1 == err) errorMessageID = ::GetLastError();
  if (errorMessageID == 0)
    return std::string(); //No error message has been recorded

  LPSTR messageBuffer = nullptr;
  size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

  std::string message(messageBuffer, size);

  //Free the buffer.
  LocalFree(messageBuffer);

  return message;
}

void DirectCallback(const char *channel, const char *data, bool buffered) {

}

/**
 * frame message and push to console client, or to queue if
 * no console client is connected
 */
void PushConsoleMessage(google::protobuf::Message &message) {

//  static uint32_t cmid = 100000;
//  message.set_id(cmid++);

  std::string framed = MessageUtilities::Frame(message);
  if (console_client >= 0) {
    pipes[console_client]->PushWrite(framed);
  }
  else {
    console_buffer.push_back(framed);
  }
}

void ConsoleResetPrompt(uint32_t id) {
  BERTBuffers::CallResponse message;
  message.set_id(id);
  message.mutable_function_call()->set_target(BERTBuffers::CallTarget::system);
  message.mutable_function_call()->set_function("reset-prompt");
  PushConsoleMessage(message);
}

void ConsoleControlMessage(const std::string &control_message) {
  BERTBuffers::CallResponse message;
  message.mutable_function_call()->set_target(BERTBuffers::CallTarget::system);
  message.mutable_function_call()->set_function(control_message);
  PushConsoleMessage(message);
}

void ConsolePrompt(const char *prompt, uint32_t id) {
  BERTBuffers::CallResponse message;
  message.set_id(id);
  message.mutable_console()->set_prompt(prompt);
  PushConsoleMessage(message);
}

void ConsoleMessage(const char *buf, int len, int flag) {
  BERTBuffers::CallResponse message;

  // R passes a length, and the buffer is not necessarily terminated there.
  // reading past it sent whatever followed, which the console's protobuf
  // runtime rejects as invalid UTF-8, losing the message.

  if (len < 0) len = (int)strlen(buf);
  if (flag) message.mutable_console()->set_err(buf, len);
  else message.mutable_console()->set_text(buf, len);
  PushConsoleMessage(message);
}

bool ConsoleCallback(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response) {

  if (console_client < 0) return false; // fail
  Pipe *pipe = pipes[console_client];

  if (!pipe->connected()) return false;

  pipe->PushWrite(MessageUtilities::Frame(call));
  pipe->StartRead(); // probably not necessary

  // we need a blocking write...

  // temp (ugh)
  while (pipe->writing()) {
    pipe->NextWrite();
    Sleep(1);
  }

  std::string data;
  DWORD result;
  do {
    result = pipe->Read(data, true);
  } 
  while (result == ERROR_MORE_DATA);

  if (!result) MessageUtilities::Unframe(response, data);

  pipe->StartRead(); // probably not necessary either
  return (result == 0);

  return false;

}

bool Callback(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response) {

  Pipe *pipe = 0;

  if (active_pipe.size()) {
    int index = active_pipe.top();
    pipe = pipes[index];

    // dev
    if (index != 1) std::cout << "WARN: callback, top of pipe is " << index << std::endl;

    // don't allow that?
    // std::cout << "(switching to callback pipe)" << std::endl;
    pipe = pipes[CALLBACK_INDEX];

  }
  else {
    // std::cout << "using callback pipe" << std::endl;
    pipe = pipes[CALLBACK_INDEX];
  }

  if (!pipe->connected()) return false;

  std::cout << "Calback " << std::endl;

  pipe->PushWrite(MessageUtilities::Frame(call));
  pipe->StartRead(); // probably not necessary

  std::string data;
  DWORD result;
  do {
    result = pipe->Read(data, true);
  } while (result == ERROR_MORE_DATA);

  std::cout << "Callback end" << std::endl;

  if (!result) MessageUtilities::Unframe(response, data);
  
  pipe->StartRead(); // probably not necessary either
  return (result == 0);
}

void Shutdown(int exit_code) {
  ExitProcess(0);
}

void NextPipeInstance(bool block, std::string &name) {
  Pipe *pipe = new Pipe;
  int rslt = pipe->Start(name, block);
  handles.push_back(pipe->wait_handle_read());
  handles.push_back(pipe->wait_handle_write());
  pipes.push_back(pipe);
}

void CloseClient(int index) {

  // shutdown if primary client breaks connection
  if (index == PRIMARY_CLIENT_INDEX) Shutdown(-1);

  // callback shouldn't close either
  else if (index == CALLBACK_INDEX) {
    std::cerr << "callback pipe closed" << std::endl;
    // Shutdown(-1);
  }

  // otherwise clean up, and watch out for console
  else {
    pipes[index]->Reset();
    if (index == console_client) {
      console_client = -1;
    }
  }

}

void QueueConsoleWrites() {
  pipes[console_client]->QueueWrites(console_buffer);
  console_buffer.clear();
}

/**
 * in an effort to make the core language agnostic, all actual functions are moved
 * here. this should cover things like initialization and setting the COM pointers.
 *
 * the caller uses symbolic constants that call these functions in the appropriate
 * language.
 */
int SystemCall(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call, int pipe_index) {
  std::string function = call.function_call().function();

  BERTBuffers::CallResponse translated_call;
  translated_call.CopyFrom(call);

  if (!function.compare("install-application-pointer")) {
    translated_call.mutable_function_call()->set_target(BERTBuffers::CallTarget::language);
    translated_call.mutable_function_call()->set_function("BERT$install.application.pointer");
    RCall(response, translated_call);
  }
  else if (!function.compare("list-functions")) {
    response.set_id(call.id());
    ListScriptFunctions(response);
  }
  else if (!function.compare("get-language")) {
    response.mutable_result()->set_str(language_tag); //  "R");
  }
  else if (!function.compare("read-source-file")) {
    std::string file = call.function_call().arguments(0).str();
    bool notify = false;
    if (call.function_call().arguments_size() > 1) notify = call.function_call().arguments(1).boolean();
    bool success = false;
    if( file.length()){
      success = ReadSourceFile(file, notify);
    }
    response.mutable_result()->set_boolean(success);
  }
  else if (!function.compare("shutdown")) {
    ConsoleControlMessage("shutdown");
    // Shutdown(0);
    return SYSTEMCALL_SHUTDOWN;
  }
  else if (!function.compare("console")) {
    if (console_client < 0) {
      console_client = pipe_index;
      std::cout << "set console client -> " << pipe_index << std::endl;
      QueueConsoleWrites();
    }
    else std::cerr << "console client already set" << std::endl;
  }
  else if (!function.compare("close")) {
    CloseClient(pipe_index);
    return SYSTEMCALL_OK; //  break; // no response?
  }
  else {
    std::cout << "ENOTIMPL (system): " << function << std::endl;
    response.mutable_result()->set_boolean(false);
  }

  return SYSTEMCALL_OK;

}

int InputStreamRead(const char *prompt, char *buf, int len, int addtohistory, bool is_continuation) {

  // it turns out this function can get called recursively. we
  // hijack this function to run non-interactive R calls, but if
  // one of those calls wants a shell interface (such as a debug
  // browser, it will call into this function again). this gets
  // a little hard to track on the UI side, as we have extra prompts
  // from the internal calls, but we don't know when those are 
  // finished.

  // however we should be able to figure this out just by tracking
  // recursion. note that this is never threaded.

  static uint32_t call_depth = 0;
  static bool recursive_calls = false;

  static uint32_t prompt_transaction_id = 0;

  std::string buffer;
  std::string message;

  DWORD result;

  if (call_depth > 0) {
    // set flag to indicate we'll need to "unwind" the console
    std::cout << "console prompt at depth " << call_depth << std::endl;
    recursive_calls = true;
  }

  ConsolePrompt(prompt, prompt_transaction_id);

  while (true) {

    result = WaitForMultipleObjects((DWORD)handles.size(), &(handles[0]), FALSE, 100);

    if (result >= WAIT_OBJECT_0 && result - WAIT_OBJECT_0 < 16) {

      int offset = (result - WAIT_OBJECT_0);
      int index = offset / 2;
      bool write = offset % 2;
      auto pipe = pipes[index];

      if (!index) std::cout << "pipe event on index 0 (" << (write ? "write" : "read") << ")" << std::endl;

      ResetEvent(handles[result - WAIT_OBJECT_0]);

      if (!pipe->connected()) {
        std::cout << "connect (" << index << ")" << std::endl;
        pipe->Connect(); // this will start reading
        if (pipes.size() < MAX_PIPE_COUNT) NextPipeInstance(false, pipename);
      }
      else if (write) {
        pipe->NextWrite();
      }
      else {
        result = pipe->Read(message);

        if (!result) {

          BERTBuffers::CallResponse call, response;
          bool success = MessageUtilities::Unframe(call, message);

          if (success) {

            response.set_id(call.id());
            switch (call.operation_case()) {

            case BERTBuffers::CallResponse::kFunctionCall:
              call_depth++;
              active_pipe.push(index);
              switch (call.function_call().target()) {
              case BERTBuffers::CallTarget::system:
                if (SYSTEMCALL_SHUTDOWN == SystemCall(response, call, index)) {
                  Shutdown(0); // we're not handling this well
                  return 0; // will terminate R loop
                }
                break;
              default:
                RCall(response, call);
                break;
              }
              active_pipe.pop();
              call_depth--;
              if (call.wait()) pipe->PushWrite(MessageUtilities::Frame(response));
              break;

            case BERTBuffers::CallResponse::kUserCommand:
              ExecUserCommand(response, call);
              if (call.wait()) pipe->PushWrite(MessageUtilities::Frame(response));
              break;

            case BERTBuffers::CallResponse::kCode:
              call_depth++;
              active_pipe.push(index);
              RExec(response, call);
              active_pipe.pop();
              call_depth--;
              if (call.wait()) pipe->PushWrite(MessageUtilities::Frame(response));
              break;

            case BERTBuffers::CallResponse::kShellCommand:
              {
                // the console sends UTF-8; before 4.2.0 R expects the
                // windows code page here, the mirror of R_WriteConsoleEx
                std::string command = r_console_utf8 ? call.shell_command() : UTF8ToWindowsCP(call.shell_command());
                len = min(len - 2, (int)command.length());
                strcpy_s(buf, len + 1, command.c_str());
              }
              buf[len++] = '\n';
              buf[len++] = 0;
              prompt_transaction_id = call.id();
              pipe->StartRead();

              // start read and then exit this function; that will cycle the R REPL loop.
              // the (implicit/explicit) response from this command is going to be the next 
              // prompt.

              return len;

            default:
              // ...
              0;
            }

            if (call_depth == 0 && recursive_calls) {
              std::cout << "unwind recursive prompt stack" << std::endl;
              recursive_calls = false;
              ConsoleResetPrompt(prompt_transaction_id);
            }

          }
          else {
            if (pipe->error()) {
              std::cout << "ERR in system pipe: " << result << std::endl;
            }
            else std::cerr << "error parsing packet: " << result << std::endl;
          }
          if (pipe->connected() && !pipe->reading()) {
            pipe->StartRead();
          }
        }
        else {
          if (result == ERROR_BROKEN_PIPE) {
            std::cerr << "broken pipe (" << index << ")" << std::endl;
            CloseClient(index);
          }
        }
      }
    }
    else if (result == WAIT_TIMEOUT) {
      RTick();
      UpdateSpreadsheetGraphics();
    }
    else {
      std::cerr << "ERR " << result << ": " << GetLastErrorAsString(result) << std::endl;
      break;
    }
  }

  return 0;
}

unsigned __stdcall ManagementThreadFunction(void *data) {

  DWORD result;
  Pipe pipe;
  char *name = reinterpret_cast<char*>(data);

  std::cout << "start management pipe on " << name << std::endl;

  int rslt = pipe.Start(name, false);
  std::string message;

  while (true) {
    result = WaitForSingleObject(pipe.wait_handle_read(), 1000);
    if (result == WAIT_OBJECT_0) {
      ResetEvent(pipe.wait_handle_read());
      if (!pipe.connected()) {
        std::cout << "connect management pipe" << std::endl;
        pipe.Connect(); // this will start reading
      }
      else {
        result = pipe.Read(message);
        if (!result) {
          BERTBuffers::CallResponse call;
          bool success = MessageUtilities::Unframe(call, message);
          if (success) {
            //std::string command = call.control_message();
            std::string command = call.function_call().function();
            if (command.length()) {
              if (!command.compare("break")) {
                user_break_flag = true;
                RSetUserBreak();
              }
              else {
                std::cerr << "unexpected system command (management pipe): " << command << std::endl;
              }
            }
          }
          else {
            std::cerr << "error parsing management message" << std::endl;
          }
          pipe.StartRead();
        }
        else {
          if (result == ERROR_BROKEN_PIPE) {
            std::cerr << "broken pipe in management thread" << std::endl;
            pipe.Reset();
          }
        }
      }
    }
    else if (result != WAIT_TIMEOUT) {
      std::cerr << "error in management thread: " << GetLastError() << std::endl;
      pipe.Reset();
      break;
    }
  }
  return 0;
}

/**
 * the R versions this controller accepts.
 *
 * the headers this compiles against and the import libraries in lib/ come
 * from one specific R release (see docs/BUILDING.md), but the embedding
 * interface they describe has been stable across the 3.x and 4.x series,
 * so a single binary can host any of them. the floor is the oldest series
 * this code has been built against. there is deliberately no upper bound:
 * the loader resolves the import libraries before main() runs, so a runtime
 * that lacks a symbol we need has already failed to start by the time we
 * get here. a newer major series than the one this build was tested with
 * gets a warning rather than a refusal, so this does not turn back into
 * the hard-coded barrier that the original major/minor equality test was.
 */
#define BERT_R_MINIMUM_MAJOR  3
#define BERT_R_MINIMUM_MINOR  5
#define BERT_R_TESTED_MAJOR   4
#define BERT_R_TESTED_MINOR   6

void ScrubPath(char *string) {

  int len = strlen(string);
  for (int i = 0; i < len; ) {
    if (i > 1 && string[i] == '\\' && string[i - 1] == '\\') {
      std::cout << "mark @ " << i << std::endl;
      for (int j = i; j < len; j++) {
        string[j] = string[j+1]; // also moves up trailing null
      }
      len--;
    }
    else i++;
  }

}

int main(int argc, char** argv) {

  char buffer[MAX_PATH];
  int major, minor, patch;
  RGetVersion(&major, &minor, &patch);

  std::cout << "R version: " << major << ", " << minor << ", " << patch << std::endl;

  /*
  char rvx[64];
  sprintf_s(rvx, "RV %d.%d.%d", major, minor, patch);
  MessageBoxA(0, rvx, "R version", MB_OK);
  */

  if (major < BERT_R_MINIMUM_MAJOR
      || (major == BERT_R_MINIMUM_MAJOR && minor < BERT_R_MINIMUM_MINOR)) {
    std::cerr << "unsupported R version " << major << "." << minor << "." << patch
      << "; this build requires R " << BERT_R_MINIMUM_MAJOR << "."
      << BERT_R_MINIMUM_MINOR << " or later" << std::endl;
    return PROCESS_ERROR_UNSUPPORTED_VERSION;
  }

  // this build is compiled against R 4.5 headers and runs against 4.6 as
  // well. anything newer has not been tried: say so rather than failing in
  // some less obvious way later.

  if (major > BERT_R_TESTED_MAJOR
      || (major == BERT_R_TESTED_MAJOR && minor > BERT_R_TESTED_MINOR)) {
    std::cerr << "warning: R " << major << "." << minor << "." << patch
      << " is newer than the R " << BERT_R_TESTED_MAJOR << "."
      << BERT_R_TESTED_MINOR
      << " this build was tested against; it may not work. graphics in "
      << "particular needs a BERTModule built for your R series; continuing"
      << std::endl;
  }

  // R for Windows uses UTF-8 as its native encoding from 4.2.0, but only
  // when the process has the UTF-8 code page, which controlr.manifest asks
  // for. otherwise R uses the system code page and console text has to be
  // converted. see R_WriteConsoleEx in rinterface_win.cc.
  r_console_utf8 = (major > 4 || (major == 4 && minor >= 2)) && (GetACP() == CP_UTF8);
  std::cout << "process code page " << GetACP() << "; console text "
    << (r_console_utf8 ? "passes through as UTF-8" : "is converted from the code page") << std::endl;

  std::stringstream ss;
  ss << "R::" << major << "." << minor << "." << patch;
  language_tag = ss.str();

  for (int i = 0; i < argc; i++) {
    if (!strncmp(argv[i], "-p", 2) && i < argc - 1) {
      pipename = argv[++i];
    }
    else if (!strncmp(argv[i], "-r", 2) && i < argc - 1) {
      rhome = argv[++i];
    }
  }

  if (!pipename.length()) {
    std::cerr << "call with -p pipename" << std::endl;
    //MessageBoxA(0, "missing pipe name", "R ERR", MB_OK);
    return PROCESS_ERROR_CONFIGURATION_ERROR;
  }
  if (!rhome.length()) {
    std::cerr << "call with -r rhome" << std::endl;
    //MessageBoxA(0, "missing r name", "R ERR", MB_OK);
    return PROCESS_ERROR_CONFIGURATION_ERROR;
  }
  
  std::cout << "pipe: " << pipename << std::endl;
  std::cout << "pid: " << _getpid() << std::endl;

  // set R_LIBS from config file, if there's a lib field

  GetEnvironmentVariableA("BERT_HOME", buffer, MAX_PATH);
  std::string config_data;
  std::string config_path(buffer);
  config_path.append("bert-config.json");
  APIFunctions::FileError config_result = APIFunctions::FileContents(config_data, config_path);
  if (config_result == APIFunctions::FileError::Success) {

    std::string err;
    json11::Json config = json11::Json::parse(config_data, err, json11::COMMENTS);
    json11::Json lib_dir = config["BERT"]["R"]["lib"];

    if (lib_dir.is_string()) {
      ExpandEnvironmentStringsA(lib_dir.string_value().c_str(), buffer, MAX_PATH);
      std::cout << "set R_LIBS from config file: " << buffer << std::endl;
      SetEnvironmentVariableA("R_LIBS", buffer);
    }
  }

  // we need a non-const block for the thread function. 
  // it just gets used once, and immediately

  sprintf_s(buffer, "%s-M", pipename.c_str());
  uintptr_t thread_handle = _beginthreadex(0, 0, ManagementThreadFunction, buffer, 0, 0);

  // start the callback pipe first. doesn't block.

  std::string callback_pipe_name = pipename;
  callback_pipe_name += "-CB";
  NextPipeInstance(false, callback_pipe_name);

  // the first connection blocks. we don't start R until there's a client.

  NextPipeInstance(true, pipename);

  // start next pipe listener, don't block

  NextPipeInstance(false, pipename);

  // now start R 

//  char no_save[] = "--no-save";
//  char no_restore[] = "--no-restore";
//  char* args[] = { argv[0], no_save, no_restore };

  char* args[] = { argv[0], "--no-save", "--no-restore", "--encoding=UTF-8" };

  int result = RLoop(rhome.c_str(), "", 4, args);
  if (result) std::cerr << "R loop failed: " << result << std::endl;

  handles.clear();
  for (auto pipe : pipes) delete pipe;

  pipes.clear();



  return 0;
}
