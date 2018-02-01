
#include "stdafx.h"
#include "XLCALL.H"
#include "variable.pb.h"
#include "bert.h"
#include "language_service.h"
#include "string_utilities.h"

// by convention we don't use transaction 0. 
// this may cause a problem if it rolls over.
uint32_t LanguageService::transaction_id_ = 1;

/** default implementation probably not usable */
int LanguageService::StartChildProcess(HANDLE job_handle) {

  char *args = new char[1024];
  sprintf_s(args, 1024, "\"%s\"", child_path_.c_str());

  STARTUPINFOA si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);

  ZeroMemory(&pi, sizeof(pi));

  int rslt = 0;

  DWORD creation_flags = CREATE_NO_WINDOW;

  if (dev_flags_ & 0x01) creation_flags = 0;

  if (!CreateProcessA(0, args, 0, 0, FALSE, creation_flags, 0, 0, &si, &pi))
  {
    DebugOut("CreateProcess failed (%d).\n", GetLastError());
    rslt = GetLastError();
  }
  else {
    child_process_id_ = pi.dwProcessId;
    if (job_handle) {
      if (!AssignProcessToJobObject(job_handle, pi.hProcess))
      {
        DebugOut("Could not AssignProcessToObject\n");
      }
    }
  }

  delete[] args;
  return rslt;
}

void LanguageService::SetApplicationPointer(LPDISPATCH application_pointer) {
  BERTBuffers::CallResponse call, response;

  auto function_call = call.mutable_function_call();
  function_call->set_function("install-application-pointer"); // generic
  function_call->set_target(BERTBuffers::CallTarget::system);
  function_call->add_arguments()->set_external_pointer(reinterpret_cast<ULONG_PTR>(application_pointer));

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
    DebugOut("ERR opening pipe: %d\n", err);
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
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);

  ZeroMemory(&pi, sizeof(pi));

  int result = 0;

  DWORD creation_flags = CREATE_NO_WINDOW;
  if (dev_flags_ & 0x01) creation_flags = 0;

  if (!CreateProcessA(0, command_line, 0, 0, FALSE, creation_flags, 0, 0, &si, &pi))
  {
    DebugOut("CreateProcess failed (%d).\n", GetLastError());
    result = GetLastError();
  }
  else {
    child_process_id_ = pi.dwProcessId;
    if (job_handle) {
      if (!AssignProcessToJobObject(job_handle, pi.hProcess))
      {
        DebugOut("Could not AssignProcessToObject\n");
      }
    }
  }

  return result;
}

void LanguageService::Connect(HANDLE job_handle) {

  int rslt = StartChildProcess(job_handle);
  int errs = 0;

  buffer_ = new char[8192];
  io_.hEvent = CreateEvent(0, TRUE, TRUE, 0); // FIXME: clean this up

  if (!rslt) {

    std::string full_name = "\\\\.\\pipe\\";
    full_name.append(pipe_name_);

    while (1) {
      pipe_handle_ = CreateFileA(full_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, 0);
      if (!pipe_handle_ || pipe_handle_ == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        DebugOut("ERR opening pipe: %d\n", err);
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

  Call(response, call);
}

bool LanguageService::ValidFile(const std::string &file) {

  // path functions can't operate on const strings

  static char path[MAX_PATH];

  int len = file.length();
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
  WriteFile(pipe_handle_, framed_message.c_str(), (int32_t)framed_message.length(), NULL, &io_);

  if (call.wait()) {

    ResetEvent(callback_info_.default_unsignaled_event_);
    HANDLE handles[2] = { io_.hEvent, callback_info_.default_unsignaled_event_ };

    ResetEvent(io_.hEvent);
    ReadFile(pipe_handle_, buffer_, 8192, 0, &io_);

    while (true) {
      ResetEvent(callback_info_.default_signaled_event_); // set unsignaled
      DWORD signaled = WaitForMultipleObjectsEx(2, handles, FALSE, INFINITE, FALSE);
      if (signaled == WAIT_OBJECT_0) {

        DWORD rslt = GetOverlappedResultEx(pipe_handle_, &io_, &bytes, INFINITE, FALSE);
        if (rslt) {
          if (!MessageUtilities::Unframe(response, buffer_, bytes)) {
            DebugOut("parse err!\n");
            response.set_err("parse error (0x10)");
            break;
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
            ReadFile(pipe_handle_, buffer_, 8192, 0, &io_);

            break;
          default:
            complete = true;
            break;
          }

          if (complete) break;
        }
        else {
          DWORD err = GetLastError();
          std::stringstream ss;
          ss << "pipe error " << err;
          response.set_err(ss.str());
          break;
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
        case BERTBuffers::Variable::ValueCase::kNum:
          value << default_value.num();
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


