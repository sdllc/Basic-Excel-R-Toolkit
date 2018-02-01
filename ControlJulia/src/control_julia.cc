
#include "include_common.h"
#include "control_julia.h"
#include "julia_interface.h"
#include "pipe.h"

#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>

#include <fstream>

std::vector<Pipe*> pipes;
std::vector<HANDLE> handles;
std::vector<std::string> console_buffer;

std::string pipename;
int console_client = -1;

HANDLE prompt_event_handle;

Pipe stdout_pipe, stderr_pipe;

//std::ofstream console_out;
//std::ofstream console_err;

/** debug/util function */
void DumpJSON(const google::protobuf::Message &message, const char *path = 0) {
  std::string str;
  google::protobuf::util::JsonOptions opts;
  opts.add_whitespace = true;
  google::protobuf::util::MessageToJsonString(message, &str, opts);
  if (path) {
    FILE *f;
    fopen_s(&f, path, "w");
    if (f) {
      fwrite(str.c_str(), sizeof(char), str.length(), f);
      fflush(f);
    }
    fclose(f);
  }
  else std::cout << str << std::endl;
}

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

std::string prompt_string;

void ConsolePrompt(const char *prompt, uint32_t id) {
  BERTBuffers::CallResponse message;
  message.set_id(id);
  message.mutable_console()->set_prompt(prompt);
  prompt_string = MessageUtilities::Frame(message);
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

  BERTBuffers::CallResponse translated_call;
  translated_call.CopyFrom(call);

  if (!function.compare("get-language")) {
    response.mutable_result()->set_str("Julia");
  }
  else if (!function.compare("read-source-file")) {
    std::string file = call.function_call().arguments(0).str();
    bool success = false;
    if (file.length()) {
      std::cout << "read source: " << file << std::endl;
      success = ReadSourceFile(file);
    }
    response.mutable_result()->set_boolean(success);
  }
  else if (!function.compare("install-application-pointer")) {

    translated_call.mutable_function_call()->set_target(BERTBuffers::CallTarget::language);
    translated_call.mutable_function_call()->set_function("BERT.InstallApplicationPointer");
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

  DWORD result, len;
  uint32_t console_prompt_id = 1;
  std::string message;

  ConsolePrompt(default_prompt, console_prompt_id++);

  while (true) {

    result = WaitForMultipleObjects((DWORD)handles.size(), &(handles[0]), FALSE, 100);

    if (result >= WAIT_OBJECT_0 && result - WAIT_OBJECT_0 < 16) {

      int offset = (result - WAIT_OBJECT_0);
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

            //std::cout << "success" << std::endl;
            response.set_id(call.id());
            //DumpJSON(call);

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
              // std::cout << "shell command" << std::endl;
              ExecResult exec_result = JuliaShellExec(call.shell_command(), shell_buffer);
              // if (call.wait()) pipe->PushWrite(MessageUtilities::Frame(response));
              console_prompt_id = call.id();
              if (exec_result == ExecResult::Incomplete) {
                //shell_buffer.push_back(call.shell_command());
                shell_buffer += call.shell_command();
                shell_buffer += "\n";
                ConsolePrompt(continuation_prompt, console_prompt_id);
              }
              else {
                //shell_buffer.clear();
                shell_buffer = "";
                ConsolePrompt(default_prompt, console_prompt_id);
              }
              break;
            }
            /*
            case BERTBuffers::CallResponse::kControlMessage:
            {
              std::string command = call.control_message();
              std::cout << "system command: " << command << std::endl;
              if (!command.compare("shutdown")) {
                //ConsoleControlMessage("shutdown");
                //Shutdown(0);
                return; // exit
              }
              else if (!command.compare("console")) {
                if (console_client < 0) {
                  console_client = index;
                  std::cout << "set console client -> " << index << std::endl;
                  pipe->QueueWrites(console_buffer);
                  console_buffer.clear();
                }
                else std::cerr << "console client already set" << std::endl;
              }
              else if (!command.compare("close")) {
                CloseClient(index);
                break; // no response 
              }
              else {
                // ...
              }

              if (call.wait()) {
                response.set_id(call.id());
                //pipe->PushWrite(rsp.SerializeAsString());
                pipe->PushWrite(MessageUtilities::Frame(response));
              }
              else pipe->NextWrite();
            }
            break;
            */

            default:
              // ...
              0;
            }

            /*
            if (call_depth == 0 && recursive_calls) {
              cout << "unwind recursive prompt stack" << endl;
              recursive_calls = false;
              ConsoleResetPrompt(prompt_transaction_id);
            }
            */

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
          //else if (result == ERROR_MORE_DATA) {
          //    cout << "(more data...)" << endl;
          //}
        }
      }
    }
    else if (result == WAIT_TIMEOUT) {
      // ...
    }
    else {
      std::cerr << "ERR " << result << ": " << GetLastErrorAsString(result) << std::endl;
      break;
    }
  }

}


unsigned __stdcall StdioThreadFunction(void *data) {

  //Pipe *pipe = (Pipe*)data;
  Pipe **pipes = (Pipe**)data;
  std::string str;

  std::string stdout_buffer;
  std::string stderr_buffer;

  HANDLE handles[] = { pipes[0]->wait_handle_read(), pipes[1]->wait_handle_read(), stdout_pipe.wait_handle_read(), stderr_pipe.wait_handle_read() };
  while (true) {
    DWORD wait_result = WaitForMultipleObjects(4, handles, FALSE, 1000);
    int index = wait_result - WAIT_OBJECT_0;
    if (index == 2) {
      ResetEvent(stdout_pipe.wait_handle_read());
      if (!stdout_pipe.connected()) {
        stdout_pipe.Connect();
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
        if(stderr_buffer.length()) stderr_pipe.PushWrite(stderr_buffer);
        stderr_buffer.clear();
      }
      else {
        int result = stderr_pipe.Read(str, false);
        if (stderr_pipe.error()) stderr_pipe.Reset();
      }
    }
    else  if (index >= 0 && index < 2) {
      ResetEvent(pipes[index]->wait_handle_read());
      if (!pipes[index]->connected()) {
        pipes[index]->Connect();
      }
      else {
        pipes[index]->Read(str, false);
        pipes[index]->StartRead();

        if (index) {
          if (stderr_pipe.connected()) stderr_pipe.PushWrite(str);
          else stderr_buffer += str;
        }
        else {
          if (stdout_pipe.connected()) stdout_pipe.PushWrite(str);
          else stdout_buffer += str;
        }
        //if (index) std::cerr << "** " << str << std::endl;
        //else std::cout << "|| " << str << std::endl;
      }
    }
    else if (wait_result == WAIT_TIMEOUT) {
      // std::cerr << "timeout" << std::endl;
    }
    else {
      std::cerr << "ERR in wait: " << GetLastError() << std::endl;
    }
  }

  /*
  HANDLE handles[] = { prompt_event_handle, pipes[0]->wait_handle_read(), pipes[1]->wait_handle_read() };

  while (true) {
    //DWORD wait_result = WaitForSingleObjectEx(handle, 1000, 0);
    DWORD wait_result = WaitForMultipleObjects(3, handles, FALSE, 1000);
    if (wait_result == WAIT_OBJECT_0) {
      ResetEvent(prompt_event_handle);
      PushConsoleString(prompt_string);
    }
    else if(wait_result == WAIT_OBJECT_0 + 1){
      ResetEvent(pipes[0]->wait_handle_read());
      if (!pipes[0]->connected()) {
        pipes[0]->Connect();
      }
      else {
        pipes[0]->Read(str, false);
        std::cout << "|| " << str << std::endl;
        ConsoleMessage(str.c_str(), str.length(), 0);
        pipes[0]->StartRead();
      }
    }
    else if (wait_result == WAIT_OBJECT_0 + 2) {
      ResetEvent(pipes[1]->wait_handle_read());
      if (!pipes[1]->connected()) {
        pipes[1]->Connect();
      }
      else {
        pipes[1]->Read(str, false);
        std::cerr << "|| " << str << std::endl;
        ConsoleMessage(str.c_str(), str.length(), 1);
        pipes[1]->StartRead();
      }
    }
    else if (wait_result == WAIT_TIMEOUT) {
      // std::cerr << "timeout" << std::endl;
    }
    else {
      std::cerr << "ERR in wait: " << GetLastError() << std::endl;
    }
  }
  */

  return 0;
}

void SetBreak() {
  std::cout << "SET BREAK" << std::endl;

  shell_buffer.clear();

  GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
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

int main(int argc, char **argv) {

  for (int i = 0; i < argc; i++) {
    if (!strncmp(argv[i], "-p", 2) && i < argc - 1) {
      pipename = argv[++i];
    }
  }

  if (!pipename.length()) {
    std::cerr << "call with -p pipename" << std::endl;
    return -1;
  }

  std::cout << "pipe: " << pipename << std::endl;

  // for julia, contra R, we are capturing stdio (out and err) 
  // to send to a console client. because writes can happen while
  // we're blocked, we'll need a separate thread to handle stdio 
  // writes.

  // attach cout back to the console for debug/tracing

  // FIXME: is it necessary to duplicate this handle? 

  int console_stdout = 0;
  _dup2(1, console_stdout);
  std::ofstream console_out(_fdopen(console_stdout, "w")); // NOTE this is nonstandard

  int console_stderr = 0;
  _dup2(2, console_stderr);
  std::ofstream console_err(_fdopen(console_stderr, "w")); 

  //
  char buffer[MAX_PATH];
  sprintf_s(buffer, "%s-M", pipename.c_str());
  uintptr_t management_thread_handle = _beginthreadex(0, 0, ManagementThreadFunction, buffer, 0, 0);
  //

  {
    char buf2[MAX_PATH];
    sprintf_s(buf2, "%s-STDOUT", pipename.c_str());
    stdout_pipe.Start(buf2, false);

    sprintf_s(buf2, "%s-STDERR", pipename.c_str());
    stderr_pipe.Start(buf2, false);
  }

  prompt_event_handle = CreateEvent(0, TRUE, FALSE, 0);
  Pipe *stdio_pipes[] = { new Pipe, new Pipe };

  stdio_pipes[0]->Start("stdout", false);
  HANDLE stdio_write_handle = CreateFile(stdio_pipes[0]->full_name().c_str(), FILE_ALL_ACCESS, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

  stdio_pipes[1]->Start("stderr", false);
  HANDLE stderr_write_handle = CreateFile(stdio_pipes[1]->full_name().c_str(), FILE_ALL_ACCESS, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  
  _dup2(_open_osfhandle((intptr_t)stdio_write_handle, _O_TEXT), 1); // 1 is stdout
  _dup2(_open_osfhandle((intptr_t)stderr_write_handle, _O_TEXT), 2);

  std::cout.rdbuf(console_out.rdbuf());
  std::cerr.rdbuf(console_err.rdbuf());

//  char buffer[] = "ZRRBT\n";
//  DWORD bytes;
//  WriteFile(write_handle, buffer, strlen(buffer), &bytes, 0);


  uintptr_t io_thread_handle = _beginthreadex(0, 0, StdioThreadFunction, stdio_pipes, 0, 0);

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

