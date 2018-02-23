
#include "stdafx.h"
#include "XLCALL.H"
#include "variable.pb.h"
#include "bert.h"
#include "language_service.h"
#include "string_utilities.h"

// by convention we don't use transaction 0. 
// this may cause a problem if it rolls over.
uint32_t LanguageService::transaction_id_ = 1;

LanguageService::LanguageService(CallbackInfo &callback_info, COMObjectMap &object_map, DWORD dev_flags, const json11::Json &config, const std::string &home_directory, const LanguageDescriptor &descriptor)
  : callback_info_(callback_info)
  , object_map_(object_map)
  , dev_flags_(dev_flags)
  , connected_(false)
  , configured_(false)
  , resource_id_(0)
{
  memset(&io_, 0, sizeof(io_));

  language_home_ = config["BERT"][descriptor.name_]["home"].string_value();
  this->configured_ = language_home_.length();
  if (!this->configured_) return;

  DWORD result = ExpandEnvironmentStringsA(language_home_.c_str(), 0, 0);
  if (result) {
    char *buffer = new char[result + 1];
    result = ExpandEnvironmentStringsA(language_home_.c_str(), buffer, result + 1);
    if (result) language_home_ = buffer;
    delete[] buffer;
  }

  file_extensions_ = descriptor.extensions_;
  language_prefix_ = descriptor.prefix_;
  language_name_ = descriptor.name_;

  resource_id_ = descriptor.startup_resource_;
  prepend_path_ = descriptor.prepend_path_;
  command_line_arguments_ = descriptor.command_arguments_;

  child_path_ = home_directory;
  child_path_.append("\\");
  child_path_.append(descriptor.executable_);

  if (dev_flags_) {
    std::string override_key = "BERT2.Override$NAMEPipeName";
    InterpolateString(override_key);
    APIFunctions::GetRegistryString(pipe_name_, override_key.c_str());
  }

  if (!pipe_name_.length()) {
    std::stringstream ss;
    ss << "BERT2-PIPE-" << descriptor.prefix_ << "-" << _getpid();
    pipe_name_ = ss.str();
  }

}

void LanguageService::Initialize() {

  if (connected_) {
    uintptr_t callback_thread_ptr = _beginthreadex(0, 0, CallbackThreadFunction, this, 0, 0);

    // get embedded startup code, split into lines
    // FIXME: why do we require that this be in multiple lines?

    if (resource_id_) {

      std::string startup_code = APIFunctions::ReadResource(MAKEINTRESOURCE(resource_id_));
      std::vector<std::string> lines;
      StringUtilities::Split(startup_code, '\n', 1, lines, true);

      BERTBuffers::CallResponse call, response;

      // should maybe wait on this, so we know it's complete before we do next steps?
      // A: no, the R process will queue it anyway (implicitly), all this does is avoid wire traffic

      call.set_wait(false);
      auto code = call.mutable_code();
      for (auto line : lines) code->add_line(line);

      // added this flag to support post-init, without requiring a separate transaction

      code->set_startup(true);
      Call(response, call);
    }
  }

}

void LanguageService::SetApplicationPointer(LPDISPATCH application_pointer) {
  BERTBuffers::CallResponse call, response;

  auto function_call = call.mutable_function_call();
  function_call->set_function("install-application-pointer"); // generic
  function_call->set_target(BERTBuffers::CallTarget::system);

  //function_call->add_arguments()->set_external_pointer(reinterpret_cast<ULONG_PTR>(application_pointer));
  object_map_.DispatchToVariable(function_call->add_arguments(), application_pointer, true);

  Call(response, call);
}

void LanguageService::RunCallbackThread() {

  int buffer_size = 1024 * 8;
  char *buffer = new char[buffer_size];
  std::stringstream ss;
  ss << "\\\\.\\pipe\\" << pipe_name_ << "-CB";

  auto bert = BERT::Instance();

  HANDLE callback_pipe_handle = CreateFileA(ss.str().c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, 0);
  if (!callback_pipe_handle || callback_pipe_handle == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    DebugOut("err opening pipe [1]: %d\n", err);
  }
  else {

    DebugOut("Connected to callback pipe\n");

    DWORD mode = PIPE_READMODE_MESSAGE;
    BOOL state = SetNamedPipeHandleState(callback_pipe_handle, &mode, 0, 0);

    DWORD bytes = 0;
    OVERLAPPED io;
    //bool reading = true;

//    if (stream_pointer_) {
//      DebugOut("stream pointer already set\n");
//    }

    memset(&io, 0, sizeof(io));
    io.hEvent = CreateEvent(0, TRUE, FALSE, 0);
    ReadFile(callback_pipe_handle, buffer, buffer_size, 0, &io);
    while (true) {
      DWORD result = WaitForSingleObject(io.hEvent, 1000);
      if (result == WAIT_OBJECT_0) {
        ResetEvent(io.hEvent);
        DWORD rslt = GetOverlappedResultEx(callback_pipe_handle, &io, &bytes, 0, FALSE);
        if (rslt) {

          BERTBuffers::CallResponse &call = callback_info_.callback_call_;
          BERTBuffers::CallResponse &response = callback_info_.callback_response_;

          call.Clear();
          response.Clear();
          MessageUtilities::Unframe(call, buffer, bytes);

          bert->HandleCallback();
          //DumpJSON(response);

          if (call.wait()) {
            std::string str_response = MessageUtilities::Frame(response);
            //memcpy(buffer, str_response.c_str(), str_response.length());
            DebugOut("callback writing response (%d)\n", str_response.size());

            // block?
            //WriteFile(callback_pipe_handle, buffer, (int32_t)str_response.size(), &bytes, &io);
            WriteFile(callback_pipe_handle, str_response.c_str(), (int32_t)str_response.size(), &bytes, &io);
            result = GetOverlappedResultEx(callback_pipe_handle, &io, &bytes, INFINITE, FALSE);
            DebugOut("result %d; wrote %d bytes\n", result, bytes);
          }

          // restart
          ResetEvent(io.hEvent);
          ReadFile(callback_pipe_handle, buffer, buffer_size, 0, &io);

        }
        else {
          DWORD err = GetLastError();
          DebugOut("ERR in GORE: %d\n", err);
          // ...
          break;
        }
      }
      else if (result != WAIT_TIMEOUT) {
        DebugOut("callback pipe error: %d\n", GetLastError());
        break;
      }
    }

    CloseHandle(io.hEvent);
    DisconnectNamedPipe(callback_pipe_handle);
    CloseHandle(callback_pipe_handle);
  }
  delete buffer;
}

int LanguageService::LaunchProcess(HANDLE job_handle, char *command_line) {

  STARTUPINFOA si;

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);

  ZeroMemory(&process_info_, sizeof(process_info_));

  int result = 0;

  DWORD creation_flags = CREATE_NO_WINDOW;
  if (dev_flags_ & 0x01) creation_flags = 0;

  if (!CreateProcessA(0, command_line, 0, 0, FALSE, creation_flags, 0, 0, &si, &process_info_))
  {
    DebugOut("CreateProcess failed (%d).\n", GetLastError());
    result = GetLastError();
  }
  else {
    child_process_id_ = process_info_.dwProcessId;
    if (job_handle) {
      if (!AssignProcessToJobObject(job_handle, process_info_.hProcess))
      {
        DebugOut("Could not AssignProcessToObject\n");
      }
    }
  }

  return result;
}

bool LanguageService::ProcessExitCode(DWORD *exit_code) {
  return GetExitCodeProcess(process_info_.hProcess, exit_code) ? true : false;
}

void LanguageService::Connect(HANDLE job_handle) {

  int rslt = StartChildProcess(job_handle);
  int errs = 0;

  buffer_ = new char[PIPE_BUFFER_SIZE];
  io_.hEvent = CreateEvent(0, TRUE, TRUE, 0); // FIXME: clean this up

  if (!rslt) {

    std::string full_name = "\\\\.\\pipe\\";
    full_name.append(pipe_name_);

    while (1) {
      pipe_handle_ = CreateFileA(full_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, 0);
      if (!pipe_handle_ || pipe_handle_ == INVALID_HANDLE_VALUE) {

        DWORD exit_code = 0;
        if (!GetExitCodeProcess(process_info_.hProcess, &exit_code)) {
          std::cerr << "error calling get exit code process: " << GetLastError() << std::endl;
        }
        else if (exit_code != STILL_ACTIVE) {
          std::cerr << "process exited with exit code " << exit_code << std::endl;
          break;
        }

        DWORD err = GetLastError();
        DebugOut("err opening pipe [2]: %d\n", err);
        if (errs++ > 10) break;
        Sleep(100);
      }
      else {
        DWORD mode = PIPE_READMODE_MESSAGE;
        BOOL state = SetNamedPipeHandleState(pipe_handle_, &mode, 0, 0);
        connected_ = true;
        DebugOut("Connected (errs: %d)\n", errs);
        break;
      }
    }
  }

}

void LanguageService::ReadSourceFile(const std::string &file) {

  BERTBuffers::CallResponse call, response;

  auto function_call = call.mutable_function_call();
  function_call->set_function("read-source-file");
  function_call->set_target(BERTBuffers::CallTarget::system);
  function_call->add_arguments()->set_str(file);
  function_call->add_arguments()->set_boolean(true); // notify

  Call(response, call);
}

void LanguageService::InterpolateString(std::string &str) {

  auto replace_function = [](std::string &haystack, std::string needle, std::string replacement) {
    for (std::string::size_type i = 0; (i = haystack.find(needle, i)) != std::string::npos;)
    {
      haystack.replace(i, needle.length(), replacement);
      i += replacement.length();
    }
  };
  
#ifdef _WIN64
  const char arch[] = "x64";
#else
  const char arch[] = "i386";
#endif

  replace_function(str, "$HOME", language_home_);
  replace_function(str, "$ARCH", arch);
  replace_function(str, "$NAME", language_name_);

}

bool LanguageService::ValidFile(const std::string &file) {

  // path functions can't operate on const strings

  static char path[MAX_PATH];

  size_t len = file.length();
  if (len > MAX_PATH - 2) len = MAX_PATH - 2;
  memcpy(path, file.c_str(), len);
  path[len] = 0;

  // FIXME: lc extension then casecompare

  // this just returns a pointer to the original string.
  // also it includes the period. docs:
  //
  // "or the address of the terminating null character otherwise."
  // so check that case.

  char *extension = PathFindExtensionA(path);
  if (extension && *extension) {
    extension++;
    for (auto compare : file_extensions_) {
      if (!StringUtilities::ICaseCompare(compare, extension)) {
        return true;
      }
    }
  }

  return false;
}

void LanguageService::Shutdown() {

  if (connected_) {
    BERTBuffers::CallResponse call;
    BERTBuffers::CallResponse rsp;

    call.set_wait(false);
    //call.set_control_message("shutdown");
    call.mutable_function_call()->set_function("shutdown");
    call.mutable_function_call()->set_target(BERTBuffers::CallTarget::system);
    Call(rsp, call);

    connected_ = false;
    CloseHandle(pipe_handle_);
    pipe_handle_ = 0;
  }

  if (buffer_) delete buffer_;

}

void LanguageService::Call(BERTBuffers::CallResponse &response, BERTBuffers::CallResponse &call) {

  DWORD bytes;
  uint32_t id = LanguageService::transaction_id();

  auto bert = BERT::Instance();

  call.set_id(id);

  std::string framed_message = MessageUtilities::Frame(call);

  ResetEvent(io_.hEvent);
  bool write_result = WriteFile(pipe_handle_, framed_message.c_str(), (int32_t)framed_message.length(), NULL, &io_);

  // wait for the write to complete. FIXME: there's no need to wait if we don't need a 
  // result, but in that case we will need to make sure it's clear before we send another message.

  GetOverlappedResultEx(pipe_handle_, &io_, &bytes, INFINITE, false);

  if (call.wait()) {

    ResetEvent(callback_info_.default_unsignaled_event_);
    HANDLE handles[2] = { io_.hEvent, callback_info_.default_unsignaled_event_ };

    ResetEvent(io_.hEvent);
    ReadFile(pipe_handle_, buffer_, PIPE_BUFFER_SIZE, 0, &io_);

    std::string message_buffer;

    while (true) {
      ResetEvent(callback_info_.default_signaled_event_); // set unsignaled
      DWORD signaled = WaitForMultipleObjectsEx(2, handles, FALSE, INFINITE, FALSE);
      if (signaled == WAIT_OBJECT_0) {

        DWORD rslt = GetOverlappedResultEx(pipe_handle_, &io_, &bytes, INFINITE, FALSE);
        if (rslt) {

          if (message_buffer.length()) {
            message_buffer.append(buffer_, bytes);
            if (!MessageUtilities::Unframe(response, message_buffer)) {
              DebugOut("parse err [2]!\n");
              response.set_err("parse error (0x10)");
              break;
            }
          }
          else {
            if (!MessageUtilities::Unframe(response, buffer_, bytes)) {
              DebugOut("parse err [1]!\n");
              response.set_err("parse error (0x11)");
              break;
            }
          }

          // check for callback
          bool complete = false;
          switch (response.operation_case()) {
          case BERTBuffers::CallResponse::OperationCase::kFunctionCall: // callback

            // temp, use existing
            //callback_info_.callback_call_.CopyFrom(response);
            bert->HandleCallbackOnThread(&response);

            // write
            ResetEvent(io_.hEvent);
            framed_message = MessageUtilities::Frame(callback_info_.callback_response_);
            WriteFile(pipe_handle_, framed_message.c_str(), (int32_t)framed_message.length(), NULL, &io_);

            // wait?
            GetOverlappedResultEx(pipe_handle_, &io_, &bytes, INFINITE, FALSE);

            // ok
            ResetEvent(io_.hEvent);
            ReadFile(pipe_handle_, buffer_, PIPE_BUFFER_SIZE, 0, &io_);

            break;
          default:
            complete = true;
            break;
          }

          if (complete) break;
        }
        else {
          DWORD err = GetLastError();
          if (err == ERROR_MORE_DATA) {
            message_buffer.append(buffer_, bytes);
            ResetEvent(io_.hEvent);
            ReadFile(pipe_handle_, buffer_, PIPE_BUFFER_SIZE, 0, &io_);
          }
          else {
            std::stringstream ss;
            ss << "pipe error " << err;
            response.set_err(ss.str());
            break;
          }
        }
      }
      else {
        ResetEvent(callback_info_.default_unsignaled_event_);
        DebugOut("other handle signaled, do something\n");
        bert->HandleCallbackOnThread();
        SetEvent(callback_info_.default_signaled_event_); // signal callback thread
      }
    }

  }
  SetEvent(callback_info_.default_signaled_event_); // default signaled

}

FUNCTION_LIST LanguageService::MapLanguageFunctions(uint32_t key) {

  FUNCTION_LIST function_list;

  if (!connected_) return function_list;

  BERTBuffers::CallResponse call;
  BERTBuffers::CallResponse rsp;

  call.mutable_function_call()->set_function("list-functions");
  call.mutable_function_call()->set_target(BERTBuffers::CallTarget::system);
  call.set_wait(true);

  Call(rsp, call);

  // DumpJSON(rsp, "c:\\temp\\dumped.json");

  if (rsp.operation_case() == BERTBuffers::CallResponse::OperationCase::kErr) return function_list; // error: no functions
  else if (rsp.operation_case() == BERTBuffers::CallResponse::OperationCase::kFunctionList) {

    for (auto descriptor : rsp.function_list().functions()) {
      ARGUMENT_LIST arglist;
      for (auto argument : descriptor.arguments()) {
        std::stringstream value;
        auto default_value = argument.default_value();
        switch (default_value.value_case()) {
        case BERTBuffers::Variable::ValueCase::kBoolean:
          value << default_value.boolean() ? "TRUE" : "FALSE";
          break;
        case BERTBuffers::Variable::ValueCase::kReal:
          value << default_value.real();
          break;
        case BERTBuffers::Variable::ValueCase::kInteger:
          value << default_value.integer();
          break;
        case BERTBuffers::Variable::ValueCase::kStr:
          value << '"' << default_value.str() << '"';
          break;
        }
        arglist.push_back(std::make_shared<ArgumentDescriptor>(argument.name(), value.str()));
      }
      function_list.push_back(std::make_shared<FunctionDescriptor>(descriptor.function().name(), key, "", "", arglist));
    }
  }

  return function_list;
}

int LanguageService::StartChildProcess(HANDLE job_handle) {

  // cache
  std::string old_path = APIFunctions::GetPath();

  std::string prepend = prepend_path_;
  InterpolateString(prepend);

  // NOTE: this doesn't work when appending path -- only prepending. why?
  APIFunctions::PrependPath(prepend);

  std::string arguments = command_line_arguments_;
  InterpolateString(arguments);

  std::stringstream command;
  command << "\"" << child_path_ << "\" -p " << pipe_name_ << " " << arguments;

  // construct shell command and launch process
  size_t len = command.str().length();
  char *args = new char[len + 1];
  memcpy_s(args, len, command.str().c_str(), len);
  args[len] = 0;

  int result = LaunchProcess(job_handle, args);

  // clean up
  delete[] args;

  // restore
  APIFunctions::SetPath(old_path);

  return result;
}

