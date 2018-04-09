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

#include "control_julia.h"
#include "julia_interface.h"
#include "io_redirector.h"

// handle for signaling break (ctrl+c); set as first
// handle in pipe loop set
HANDLE break_event_handle = CreateEvent(0, TRUE, FALSE, 0);
std::vector<HANDLE> handles = { break_event_handle };

std::vector<Pipe*> pipes;
std::vector<std::string> console_buffer;

std::string pipename;
int console_client = -1;

HANDLE prompt_event_handle;

Pipe stdout_pipe, stderr_pipe;

extern void JuliaRunUVLoop(bool until_done);

std::string language_tag;

void NextPipeInstance(bool block, std::string &name) {
  Pipe *pipe = new Pipe;
  int rslt = pipe->Start(name, block);
  handles.push_back(pipe->wait_handle_read());
  handles.push_back(pipe->wait_handle_write());
  pipes.push_back(pipe);
}

void CloseClient(int index) {

  /*
  // shutdown if primary client breaks connection
  if (index == PRIMARY_CLIENT_INDEX) Shutdown(-1);

  // callback shouldn't close either
  else if (index == CALLBACK_INDEX) {
    cerr << "callback pipe closed" << endl;
    // Shutdown(-1);
  }

  // otherwise clean up, and watch out for console
  else
  */
  {
    pipes[index]->Reset();
    if (index == console_client) {
      console_client = -1;
    }
  }

}

// FIXME: utility library
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


/**
* frame message and push to console client, or to queue if
* no console client is connected
*/
void PushConsoleMessage(google::protobuf::Message &message) {
  std::string framed = MessageUtilities::Frame(message);
  if (console_client >= 0) {
    pipes[console_client]->PushWrite(framed);
  }
  else {
    console_buffer.push_back(framed);
  }
}

void PushConsoleString(const std::string &str) {
  if (console_client >= 0) {
    pipes[console_client]->PushWrite(str);
  }
  else {
    console_buffer.push_back(str);
  }
}

/*
void ConsoleMessage(const char *buf, int len, int flag) {
  BERTBuffers::CallResponse message;
  if (flag) message.mutable_console()->set_err(buf, len);
  else message.mutable_console()->set_text(buf, len);
  PushConsoleMessage(message);
}
*/

// std::string prompt_string;

void ConsolePrompt(const char *prompt, uint32_t id) {
  BERTBuffers::CallResponse message;
  message.set_id(id);
  message.mutable_console()->set_prompt(prompt);
  // prompt_string = MessageUtilities::Frame(message);
  // SetEvent(prompt_event_handle);
  PushConsoleMessage(message);
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
bool SystemCall(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call, int pipe_index) {
  std::string function = call.function_call().function();

  if (!function.compare("get-language")) {
    response.mutable_result()->set_str(language_tag); //  "Julia");
  }
  else if (!function.compare("read-source-file")) {
    std::string file = call.function_call().arguments(0).str();
    bool notify = false;
    if (call.function_call().arguments_size() > 1) notify = call.function_call().arguments(1).boolean();
    bool success = false;
    if (file.length()) {
      std::cout << "read source: " << file << std::endl;
      success = ReadSourceFile(file, notify);
    }
    response.mutable_result()->set_boolean(success);
  }
  /*
  else if (!function.compare("post-init")) {
    //JuliaPostInit(response, translated_call);
    response.mutable_result()->set_boolean(JuliaPostInit());
  }
  */
  else if (!function.compare("install-application-pointer")) {
    BERTBuffers::CallResponse translated_call;
    translated_call.CopyFrom(call);
    auto mutable_function_call = translated_call.mutable_function_call();
    mutable_function_call->set_target(BERTBuffers::CallTarget::language);
    mutable_function_call->set_function("BERT.InstallApplicationPointer");
    JuliaCall(response, translated_call);
  }
  else if (!function.compare("list-functions")) {

    // we're using exec for the moment because we don't support scoping (i.e. Bert.X) in call

    //translated_call.mutable_code()->add_line("BERT.ListFunctions()");
    //JuliaExec(response, translated_call);
    ListScriptFunctions(response, call);
    
    //translated_call.mutable_function_call()->set_target(BERTBuffers::CallTarget::language);
    //translated_call.mutable_function_call()->set_function("BERT.ListFunctions");
    //JuliaCall(response, translated_call);
  }
  else if (!function.compare("shutdown")) {

  }
  else if (!function.compare("console")) {
    if (console_client < 0) {
      console_client = pipe_index;
      std::cout << "set console client -> " << pipe_index << std::endl;
      QueueConsoleWrites(); // just prompts
    }
  }
  else if (!function.compare("close")) {
    CloseClient(pipe_index);
    return false;
  }
  else {
    std::cout << "ENOTIMPL (system): " << function << std::endl;
    response.mutable_result()->set_boolean(false);
  }

  return true;
}

//std::vector< std::string > shell_buffer;
std::string shell_buffer;

void pipe_loop() {

  char default_prompt[] = "> ";
  char continuation_prompt[] = "+ ";

  DWORD result;
  uint32_t console_prompt_id = 1;
  std::string message;

  bool executing_command = false;

  ConsolePrompt(default_prompt, console_prompt_id++);

  while (true) {

    result = WaitForMultipleObjects((DWORD)handles.size(), &(handles[0]), FALSE, 100);

    if (result == WAIT_OBJECT_0) {

      // this is the break handle
      std::cout << "break handle set" << std::endl;
      ::ResetEvent(break_event_handle);

      shell_buffer.clear();
      JuliaShellExec("\n", shell_buffer);
      ConsolePrompt(default_prompt, console_prompt_id++);

    }
    else if (result >= WAIT_OBJECT_0 && result - WAIT_OBJECT_0 < 16) {

      int offset = (result - WAIT_OBJECT_0 - 1); // -1 for break handle at top
      int index = offset / 2;
      bool write = offset % 2;
      auto pipe = pipes[index];

      //if (!index) std::cout << "pipe event on index 0 (" << (write ? "write" : "read") << ")" << std::endl;

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

              //std::cout << "function call" << std::endl;
              switch (call.function_call().target()) {
              case BERTBuffers::CallTarget::system:
                SystemCall(response, call, index);
                break;
              default:
                JuliaCall(response, call);
                break;
              }
              if (call.wait()) pipe->PushWrite(MessageUtilities::Frame(response));
              break;

            case BERTBuffers::CallResponse::kCode:
              // std::cout << "code" << std::endl;
              JuliaExec(response, call);
              if (call.wait()) pipe->PushWrite(MessageUtilities::Frame(response));
              break;

            case BERTBuffers::CallResponse::kShellCommand:
            {
              ExecResult exec_result = JuliaShellExec(call.shell_command(), shell_buffer);
              console_prompt_id = call.id();
              if (exec_result == ExecResult::Incomplete) {
                shell_buffer += call.shell_command();
                shell_buffer += "\n";
                ConsolePrompt(continuation_prompt, console_prompt_id);
              }
              else {
                shell_buffer = "";
                ConsolePrompt(default_prompt, console_prompt_id);
              }
              break;
            }
            
            default:
              0;
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
      // ...

      // maybe // JuliaRunUVLoop(false); // why not? 

    }
    else {
      std::cerr << "ERR " << result << ": " << GetLastErrorAsString(result) << std::endl;
      break;
    }
  }

}

void TruncateBuffer(std::string &buffer, uint32_t target_length = 1024 * 8) {

  // let's not chop byte by byte here, be coarse

  if (buffer.length() > (target_length + 1024)) {
    buffer.erase(0, buffer.length() - target_length);
  }

}

/*
unsigned __stdcall StdioThreadFunction(void *data) {

  //Pipe *pipe = (Pipe*)data;
  Pipe **pipes_array = (Pipe**)data;
  std::string str;

  // FIXME: we should cap these buffers, as they could potentially 
  // grow very large.

  std::string stdout_buffer;
  std::string stderr_buffer;

  HANDLE handles[] = { pipes_array[0]->wait_handle_read(), pipes_array[1]->wait_handle_read(), stdout_pipe.wait_handle_read(), stderr_pipe.wait_handle_read() };
  while (true) {
    DWORD wait_result = WaitForMultipleObjects(4, handles, FALSE, 1000);
    int index = wait_result - WAIT_OBJECT_0;

    if (index == 2) {
      ResetEvent(stdout_pipe.wait_handle_read());
      if (!stdout_pipe.connected()) {
        stdout_pipe.Connect();
        //std::cout << "Connect stdout pipe, buffer: ``" << stdout_buffer << "''" << std::endl;
        if(stdout_buffer.length()) stdout_pipe.PushWrite(stdout_buffer);
        stdout_buffer.clear();
      }
      else {
        int result = stdout_pipe.Read(str, false);
        if (stdout_pipe.error()) stdout_pipe.Reset();
      }
    }
    else if (index == 3) {
      ResetEvent(stderr_pipe.wait_handle_read());
      if (!stderr_pipe.connected()) {
        stderr_pipe.Connect();
        //std::cout << "Connect stderr pipe, buffer: ``" << stderr_buffer << "''" << std::endl;
        if(stderr_buffer.length()) stderr_pipe.PushWrite(stderr_buffer);
        stderr_buffer.clear();
      }
      else {
        int result = stderr_pipe.Read(str, false);
        if (stderr_pipe.error()) stderr_pipe.Reset();
      }
    }

    else  if (index >= 0 && index < 2) {
      ResetEvent(pipes_array[index]->wait_handle_read());
      if (!pipes_array[index]->connected()) {
        pipes_array[index]->Connect(true);
        std::cout << "connect stdio pipe " << index << std::endl;
      }
      else {

        // FIXME: we're getting a lot of one-byte strings here.
        // we should implement some minimal amount of buffering
        // to try to clean this up... could be as small as 10ms
        
        DWORD read_result = pipes_array[index]->Read(str, false);
        pipes_array[index]->StartRead();

        if (index) {
          if (stderr_pipe.connected()) {
            stderr_pipe.PushWrite(str);
          }
          else {
            TruncateBuffer(stderr_buffer.append(str));
          }
        }
        else {
          if (stdout_pipe.connected()) {
            stdout_pipe.PushWrite(str);
            //std::cout << "OUT [2]: " << str << std::endl;
          }
          else {
            TruncateBuffer(stdout_buffer.append(str));
            //std::cout << "OUT [1]: " << str << std::endl;
          }
        }
      }
      
    }
    else if (wait_result == WAIT_TIMEOUT) {
      // std::cerr << "timeout" << std::endl;
    }
    else {
      std::cerr << "ERR in wait: " << GetLastError() << std::endl;
    }
  }

  return 0;
}
*/

unsigned __stdcall StdioThreadFunction(void *data) {

  IORedirector **io_redirectors = (IORedirector**)data;
  std::string str;

  // FIXME: we should cap these buffers, as they could potentially 
  // grow very large.

  std::string stdout_buffer;
  std::string stderr_buffer;

  HANDLE handles[] = { io_redirectors[0]->data_available_, io_redirectors[1]->data_available_, stdout_pipe.wait_handle_read(), stderr_pipe.wait_handle_read() };
  while (true) {
    DWORD wait_result = WaitForMultipleObjects(4, handles, FALSE, 1000);
    int index = wait_result - WAIT_OBJECT_0;

    if (index == 2) {
      ResetEvent(stdout_pipe.wait_handle_read());
      if (!stdout_pipe.connected()) {
        stdout_pipe.Connect();
        //std::cout << "Connect stdout pipe, buffer: ``" << stdout_buffer << "''" << std::endl;
        if (stdout_buffer.length()) stdout_pipe.PushWrite(stdout_buffer);
        stdout_buffer.clear();
      }
      else {
        int result = stdout_pipe.Read(str, false);
        if (stdout_pipe.error()) stdout_pipe.Reset();
      }
    }
    else if (index == 3) {
      ResetEvent(stderr_pipe.wait_handle_read());
      if (!stderr_pipe.connected()) {
        stderr_pipe.Connect();
        //std::cout << "Connect stderr pipe, buffer: ``" << stderr_buffer << "''" << std::endl;
        if (stderr_buffer.length()) stderr_pipe.PushWrite(stderr_buffer);
        stderr_buffer.clear();
      }
      else {
        int result = stderr_pipe.Read(str, false);
        if (stderr_pipe.error()) stderr_pipe.Reset();
      }
    }

    else  if (index >= 0 && index < 2) {
      ResetEvent(io_redirectors[index]->data_available_);
      //if (!pipes_array[index]->connected()) {
      //  pipes_array[index]->Connect(true);
      //  std::cout << "connect stdio pipe " << index << std::endl;
      //}
      //else 
      {

        // FIXME: we're getting a lot of one-byte strings here.
        // we should implement some minimal amount of buffering
        // to try to clean this up... could be as small as 10ms

        io_redirectors[index]->GetData(str);

        if (index) {
          if (stderr_pipe.connected()) {
            stderr_pipe.PushWrite(str);
          }
          else {
            TruncateBuffer(stderr_buffer.append(str));
          }
        }
        else {
          if (stdout_pipe.connected()) {
            stdout_pipe.PushWrite(str);
            //std::cout << "OUT [2]: " << str << std::endl;
          }
          else {
            TruncateBuffer(stdout_buffer.append(str));
            //std::cout << "OUT [1]: " << str << std::endl;
          }
        }

      }

    }
    else if (wait_result == WAIT_TIMEOUT) {
      // std::cerr << "timeout" << std::endl;
    }
    else {
      std::cerr << "ERR in wait: " << GetLastError() << std::endl;
    }
  }

  return 0;
}

void SetBreak() {
  std::cout << "SET BREAK" << std::endl;
  
  // this is moved to the event handler
  // shell_buffer.clear();

  GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);

  // we want to spin the julia uv loop. there may be 
  // no better way to do this than to wait... actually 
  // this is the wrong thread. (...)

  // we want julia to actually do something. so we'll
  // signal the main thread to run a shell command, 
  // which should fail.

  // NOTE: only do that if we're not currently executing
  // a command. need some state.

  ::SetEvent(break_event_handle);

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

                SetBreak();

                //user_break_flag = true;
                // RSetUserBreak();
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


bool Callback(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response) {

  Pipe *pipe = 0;

  /* FIXME
  if (active_pipe.size()) {
    int index = active_pipe.top();
    pipe = pipes[index];

    // dev
    if (index != 1) cout << "WARN: callback, top of pipe is " << index << endl;
  }
  else {
    cout << "using callback pipe" << endl;
    pipe = pipes[CALLBACK_INDEX];
  }
  */
  pipe = pipes[0];

  if (!pipe->connected()) return false;

  pipe->PushWrite(MessageUtilities::Frame(call));
  pipe->StartRead(); // probably not necessary

  std::string data;
  DWORD result;
  do {
    result = pipe->Read(data, true);
  } while (result == ERROR_MORE_DATA);

  if (!result) MessageUtilities::Unframe(response, data);

  pipe->StartRead(); // probably not necessary either
  return (result == 0);
}

int main(int argc, char **argv) {

  int32_t major = 0;
  int32_t minor = 0;
  int32_t patch = 0;

  JuliaGetVersion(&major, &minor, &patch);
  std::cout << "Julia version reports " << major << "." << minor << "." << patch << std::endl;

  // FIXME: constants for version checks, return values
  // we can probably use ranges as well, at least for minor.
  // major versions may require specific versions of this controller

  if (major != 0) {
    std::cerr << "invalid major version" << std::endl;
    return PROCESS_ERROR_UNSUPPORTED_VERSION;
  }
  if (minor != 7) {
    std::cerr << "invalid minor version" << std::endl;
    return PROCESS_ERROR_UNSUPPORTED_VERSION;
  }

  std::stringstream ss;
  ss << "Julia::" << major << "." << minor << "." << patch;
  language_tag = ss.str();

  for (int i = 0; i < argc; i++) {
    if (!strncmp(argv[i], "-p", 2) && i < argc - 1) {
      pipename = argv[++i];
    }
  }

  if (!pipename.length()) {
    std::cerr << "call with -p pipename" << std::endl;
    return PROCESS_ERROR_CONFIGURATION_ERROR;
  }

  std::cout << "pipe: " << pipename << std::endl;

  char buffer[MAX_PATH];
  sprintf_s(buffer, "%s-M", pipename.c_str());
  uintptr_t management_thread_handle = _beginthreadex(0, 0, ManagementThreadFunction, buffer, 0, 0);

  // for julia, contra R, we are capturing stdio (out and err) 
  // to send to a console client. because writes can happen while
  // we're blocked, we'll need a separate thread to handle stdio 
  // writes.

  // attach cout back to the console for debug/tracing

  // FIXME: is it necessary to duplicate this handle? 

  fflush(stdout);
  int console_stdout_fd = _dup(1);
  std::ofstream console_out(_fdopen(console_stdout_fd, "w")); // NOTE this is nonstandard
  std::cout.rdbuf(console_out.rdbuf());

  fflush(stderr);
  int console_stderr_fd = _dup(2);
  std::ofstream console_err(_fdopen(console_stderr_fd, "w"));
  std::cerr.rdbuf(console_err.rdbuf());

  // these are 

  {
    char buf2[MAX_PATH];
    sprintf_s(buf2, "%s-STDOUT", pipename.c_str());
    stdout_pipe.Start(buf2, false);

    sprintf_s(buf2, "%s-STDERR", pipename.c_str());
    stderr_pipe.Start(buf2, false);
  }

  prompt_event_handle = CreateEvent(0, TRUE, FALSE, 0);

  /*
  Pipe *stdio_pipes[] = { new Pipe, new Pipe };

  stdio_pipes[0]->Start("stdout", false);
  HANDLE stdout_write_handle = CreateFile(stdio_pipes[0]->full_name().c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  
  stdio_pipes[1]->Start("stderr", false);
  HANDLE stderr_write_handle = CreateFile(stdio_pipes[1]->full_name().c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

  //int file_stdout = _open_osfhandle((intptr_t)stdout_write_handle, _O_TEXT);
  //_dup2(file_stdout, 1);
//  _dup2(_open_osfhandle((intptr_t)stdout_write_handle, _O_TEXT), 1);

  _dup2(_open_osfhandle((intptr_t)stdout_write_handle, _O_TEXT), 1); // _O_TEXT), 1); // stdout
  _dup2(_open_osfhandle((intptr_t)stderr_write_handle, _O_TEXT), 2); // _O_TEXT), 1); // stderr

  uintptr_t io_thread_handle = _beginthreadex(0, 0, StdioThreadFunction, stdio_pipes, 0, 0);
  */

  std::cout << "starting IO redirectors" << std::endl;

  IORedirector *io_redirectors[] = { new IORedirector, new IORedirector };
  io_redirectors[0]->Start(stdout);
  io_redirectors[1]->Start(stderr);
  uintptr_t io_thread_handle = _beginthreadex(0, 0, StdioThreadFunction, io_redirectors, 0, 0);

  // start the callback pipe first. doesn't block.

  std::string callback_pipe_name = pipename;
  callback_pipe_name += "-CB";
  NextPipeInstance(false, callback_pipe_name);

  // ...

  NextPipeInstance(true, pipename);

  std::cout << "first pipe connected" << std::endl;

  JuliaInit();

  pipe_loop();
  // julia_exec();

  JuliaShutdown();

  handles.clear();

  for (auto pipe : pipes) delete pipe;

  pipes.clear();

  return 0;

}

