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

#include "stdafx.h"
#include "XLCALL.H"
#include "variable.pb.h"
#include "bert.h"
#include "language_service.h"
#include "string_utilities.h"

// by convention we don't use transaction 0. 
// this may cause a problem if it rolls over.
uint32_t LanguageService::transaction_id_ = 1;

//LanguageService::LanguageService(CallbackInfo &callback_info, COMObjectMap &object_map, DWORD dev_flags, const json11::Json &config, const std::string &home_directory, const LanguageDescriptor &descriptor)
LanguageService::LanguageService(CallbackInfo &callback_info, COMObjectMap &object_map, DWORD dev_flags, const json11::Json &config, const std::string &home_directory, const json11::Json &json)
  : callback_info_(callback_info)
  , object_map_(object_map)
  , dev_flags_(dev_flags)
  , connected_(false)
  , configured_(false)
  // , resource_id_(0)
  // , language_descriptor_(descriptor)
{
  memset(&io_, 0, sizeof(io_));

  // we're now receiving the json descriptor instead of the object, but we still
  // want to construct the object. the json descriptor may have multiple versions
  // for a given language; in that case, we want to overlay and compare.

  language_descriptor_.FromJSON(json, home_directory);

  // comment out language (or delete) to deactivate.
  
  configured_ = !(config["BERT"][language_descriptor_.name_].is_null());
  if (!configured_) return;

  std::string override_home;
  if (config["BERT"][language_descriptor_.name_]["home"].is_string()) override_home = config["BERT"][language_descriptor_.name_]["home"].string_value();

  if (json["versions"].is_array()) {

    configured_ = false;

    // to force a particular version, use the "tag" field; if we find a matching
    // "tag", we use that one. it's a straight string match, not >=. if there's 
    // no matching tag, bail.

    if (config["BERT"][language_descriptor_.name_]["tag"].is_string()) {
      std::string tag = config["BERT"][language_descriptor_.name_]["tag"].string_value();
      for (const auto &version : json["versions"].array_items()) {
        LanguageDescriptor descriptor(language_descriptor_);
        descriptor.FromJSON(version, home_directory);
        if (descriptor.tag_ == tag) {
          std::cout << "found matching tag: " << tag << std::endl;
          configured_ = true;
          language_descriptor_ = descriptor;
          break;
        }
      }
    }
    else {

      // copy first to make sure we don't modify it

      LanguageDescriptor descriptor_copy(language_descriptor_);

      // if there's no tag, we order by priority and check for existence of
      // the home directory. 1 is highest priority, then 2, etc. 0 is not used.
      // (neither are negative numbers. we used to use zero).

      int32_t priority = 0;
      for (const auto &version : json["versions"].array_items()) {
        LanguageDescriptor descriptor(descriptor_copy);
        descriptor.FromJSON(version, home_directory);

        // lower priority? we can skip.
        if (priority > 0 && descriptor.priority_ > priority) continue;

        // home directory exists?

        // FIXME: so how do we know, absent a tag, what version this is? 
        if (override_home.length()) descriptor.home_ = override_home;

        if (descriptor.home_.length()) {

          // maybe need to expand
          DWORD expanded_length = ExpandEnvironmentStringsA(descriptor.home_.c_str(), 0, 0);
          if (expanded_length) {
            char *buffer = new char[expanded_length + 1];
            expanded_length = ExpandEnvironmentStringsA(descriptor.home_.c_str(), buffer, expanded_length + 1);
            if (expanded_length) descriptor.home_ = buffer;
            delete[] buffer;
          }

          DWORD attributes = GetFileAttributesA(descriptor.home_.c_str());
          if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            priority = descriptor.priority_;
            configured_ = true;
            language_descriptor_ = descriptor;
          }

        }
        else if (descriptor.home_candidates_.size()) {
          for (auto candidate : descriptor.home_candidates_) {
            DWORD length = ExpandEnvironmentStringsA(candidate.c_str(), 0, 0);
            if (length > 0) {
              char *buffer = new char[length];
              length = ExpandEnvironmentStringsA(candidate.c_str(), buffer, length);
              if (length > 0) {
                DWORD attributes = GetFileAttributesA(buffer);
                if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
                  descriptor.home_ = buffer;
                  configured_ = true;
                  language_descriptor_ = descriptor;
                }
              }
              delete[] buffer;
            }
          }
        }

      }

    }


    if (!configured_) {
      std::cout << "No tag, not using versions" << std::endl;
      configured_ = false;
      return;
    }

  }

  // default home or override in the config. note that for R we are relying on 
  // the BERT_HOME env var being set at this point.

  if (config["BERT"][language_descriptor_.name_]["home"].is_string()) {
    language_descriptor_.home_ = config["BERT"][language_descriptor_.name_]["home"].string_value();
  }

  // we now support an array here, find the latest entry that exists

  if (!language_descriptor_.home_.length() && language_descriptor_.home_candidates_.size()) {

    for (auto candidate : language_descriptor_.home_candidates_) {
      DWORD length = ExpandEnvironmentStringsA(candidate.c_str(), 0, 0);
      if (length > 0) {
        char *buffer = new char[length];
        length = ExpandEnvironmentStringsA(candidate.c_str(), buffer, length);
        if (length > 0) {

          DWORD attributes = GetFileAttributesA(buffer);
          if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            language_descriptor_.home_ = buffer;
          }
        }
        delete[] buffer;
      }
    }
  }

  configured_ = language_descriptor_.home_.length();
  if (!configured_) return;

  DWORD result = ExpandEnvironmentStringsA(language_descriptor_.home_.c_str(), 0, 0);
  if (result) {
    char *buffer = new char[result + 1];
    result = ExpandEnvironmentStringsA(language_descriptor_.home_.c_str(), buffer, result + 1);
    if (result) language_descriptor_.home_ = buffer;
    delete[] buffer;
  }

  child_path_ = home_directory;
  child_path_.append("\\");
  child_path_.append(language_descriptor_.executable_);

  if (dev_flags_) {
    std::string override_key = "BERT2.Override$NAMEPipeName";
    InterpolateString(override_key);
    APIFunctions::GetRegistryString(pipe_name_, override_key.c_str());
  }

  if (!pipe_name_.length()) {
    std::stringstream ss;
    ss << "BERT2-PIPE-" << language_descriptor_.prefix_ << "-" << _getpid();
    pipe_name_ = ss.str();
  }

}

void LanguageService::Initialize() {

  if (connected_) {
    uintptr_t callback_thread_ptr = _beginthreadex(0, 0, CallbackThreadFunction, this, 0, 0);

    // get embedded startup code, split into lines
    // FIXME: why do we require that this be in multiple lines?

    //if (resource_id_) {
    if (language_descriptor_.startup_resource_path_.length()){

      std::string startup_code;
      APIFunctions::FileError err = APIFunctions::FileContents(startup_code, language_descriptor_.startup_resource_path_);

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

    std::string message_buffer;

    memset(&io, 0, sizeof(io));
    io.hEvent = CreateEvent(0, TRUE, FALSE, 0);
    ReadFile(callback_pipe_handle, buffer, buffer_size, 0, &io);
    while (true) {
      DWORD result = WaitForSingleObject(io.hEvent, 1000);
      if (result == WAIT_OBJECT_0) {
        ResetEvent(io.hEvent);
        //DWORD rslt = GetOverlappedResultEx(callback_pipe_handle, &io, &bytes, 0, FALSE);
        DWORD rslt = GetOverlappedResult(callback_pipe_handle, &io, &bytes, FALSE);
        if (rslt) {

          BERTBuffers::CallResponse &call = callback_info_.callback_call_;
          BERTBuffers::CallResponse &response = callback_info_.callback_response_;

          call.Clear();
          response.Clear();

          if (message_buffer.length()) {
            message_buffer.append(buffer, bytes);
            MessageUtilities::Unframe(call, message_buffer);
            message_buffer = "";
          }
          else {
            MessageUtilities::Unframe(call, buffer, bytes);
          }

          bert->HandleCallback(language_descriptor_.name_);
          //DumpJSON(response);

          if (call.wait()) {
            std::string str_response = MessageUtilities::Frame(response);
            WriteFile(callback_pipe_handle, str_response.c_str(), (int32_t)str_response.size(), &bytes, &io);
            //result = GetOverlappedResultEx(callback_pipe_handle, &io, &bytes, INFINITE, FALSE);
            result = GetOverlappedResult(callback_pipe_handle, &io, &bytes, TRUE);
          }

          // restart
          ResetEvent(io.hEvent);
          ReadFile(callback_pipe_handle, buffer, buffer_size, 0, &io);

        }
        else {
          DWORD err = GetLastError();
          if (err == ERROR_MORE_DATA) {
            message_buffer.append(buffer, bytes);
            ResetEvent(io.hEvent);
            ReadFile(callback_pipe_handle, buffer, buffer_size, 0, &io);
          }
          else {
            DebugOut("ERR in GORE: %d\n", err);
            // ...
            break;
          }
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
        if (errs++ > 30) break;
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
  call.set_wait(true); // prevent race

  auto function_call = call.mutable_function_call();
  function_call->set_function("read-source-file");
  function_call->set_target(BERTBuffers::CallTarget::system);
  function_call->add_arguments()->set_str(file);
  function_call->add_arguments()->set_boolean(true); // notify

  Call(response, call);

}

void LanguageService::InterpolateString(std::string &str, const std::vector<std::pair<std::string, std::string>> &additional_replacements){

  auto replace_function = [](std::string &haystack, std::string needle, std::string replacement) {
    for (std::string::size_type i = 0; (i = haystack.find(needle, i)) != std::string::npos;)
    {
      haystack.replace(i, needle.length(), replacement);
      i += replacement.length();
    }
  };
  
  // NOTE: this is designed for switching x86/x64 R, but we actually
  // want 64-bit R even with 32-bit Excel; the only reason to do this
  // would be if you were running on a 32-bit machine, which we don't
  // ncessarily want to support.

#ifdef _WIN64
  const char arch[] = "x64";
#else
  const char arch[] = "i386";
#endif

  replace_function(str, "$HOME", language_descriptor_.home_);
  replace_function(str, "$ARCH", arch);
  replace_function(str, "$NAME", language_descriptor_.name_);

  for (const auto &pair : additional_replacements) {
    replace_function(str, pair.first, pair.second);
  }

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
    for (auto compare : language_descriptor_.extensions_) {
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

//  GetOverlappedResultEx(pipe_handle_, &io_, &bytes, INFINITE, false);
  GetOverlappedResult(pipe_handle_, &io_, &bytes, TRUE);

  if (call.wait()) {

    ResetEvent(callback_info_.default_unsignaled_event_);
    HANDLE handles[2] = { io_.hEvent, callback_info_.default_unsignaled_event_ };

    ResetEvent(io_.hEvent);
    ReadFile(pipe_handle_, buffer_, PIPE_BUFFER_SIZE, 0, &io_);

    std::string message_buffer;

    // ::MessageBoxA(0, "Call wait", "CB", MB_OK);
    Sleep(1000);

    while (true) {
      ResetEvent(callback_info_.default_signaled_event_); // set unsignaled
      DWORD signaled = WaitForMultipleObjectsEx(2, handles, FALSE, INFINITE, FALSE);
      if (signaled == WAIT_OBJECT_0) {

        // ::MessageBoxA(0, "Call signaled", "CB", MB_OK);

        //DWORD rslt = GetOverlappedResultEx(pipe_handle_, &io_, &bytes, INFINITE, FALSE);
        DWORD rslt = GetOverlappedResult(pipe_handle_, &io_, &bytes, TRUE);
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
            bert->HandleCallbackOnThread(language_descriptor_.name_, &response);

            // write
            ResetEvent(io_.hEvent);
            framed_message = MessageUtilities::Frame(callback_info_.callback_response_);
            WriteFile(pipe_handle_, framed_message.c_str(), (int32_t)framed_message.length(), NULL, &io_);

            // wait?
            //GetOverlappedResultEx(pipe_handle_, &io_, &bytes, INFINITE, FALSE);
            GetOverlappedResult(pipe_handle_, &io_, &bytes, TRUE);

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
      else if( signaled != WAIT_TIMEOUT) {
        ResetEvent(callback_info_.default_unsignaled_event_);
        DebugOut("other handle signaled, do something\n");
        bert->HandleCallbackOnThread(language_descriptor_.name_);
        SetEvent(callback_info_.default_signaled_event_); // signal callback thread
      }
    }

  }
  SetEvent(callback_info_.default_signaled_event_); // default signaled

}

FUNCTION_LIST LanguageService::CreateFunctionList(const BERTBuffers::CallResponse &message, uint32_t key, const std::string &name, std::shared_ptr<LanguageService> language_service_pointer) {

  FUNCTION_LIST function_list;

  if (message.operation_case() == BERTBuffers::CallResponse::OperationCase::kErr) return function_list; // error: no functions
  else if (message.operation_case() == BERTBuffers::CallResponse::OperationCase::kFunctionList) {

    for (auto descriptor : message.function_list().functions()) {
      ARGUMENT_LIST arglist;
      for (auto argument : descriptor.arguments()) {
        std::stringstream value;
        auto default_value = argument.default_value();
        int value_case = default_value.value_case();
        switch (default_value.value_case()) {
        case BERTBuffers::Variable::ValueCase::kBoolean:
          value << (default_value.boolean() ? "TRUE" : "FALSE");
          break;
        case BERTBuffers::Variable::ValueCase::kReal:
          value << default_value.real();
          break;
        case BERTBuffers::Variable::ValueCase::kInteger:
          value << default_value.integer();
          break;
        case BERTBuffers::Variable::ValueCase::kStr:
          // value << '"' << default_value.str() << '"';
          value << default_value.str();
          break;
        }
        arglist.push_back(std::make_shared<ArgumentDescriptor>(argument.name(), value.str(), argument.description()));
      }
      function_list.push_back(std::make_shared<FunctionDescriptor>(
        descriptor.function().name(), 
        descriptor.function().name(), 
        name, key, descriptor.category(), descriptor.function().description(), 
        arglist, descriptor.flags(), language_service_pointer));
    }
  }

  return function_list;

}

FUNCTION_LIST LanguageService::MapLanguageFunctions(uint32_t key, std::shared_ptr<LanguageService> language_service) {

  if (!connected_) return {}; 

  BERTBuffers::CallResponse call;
  BERTBuffers::CallResponse response;

  call.mutable_function_call()->set_function("list-functions");
  call.mutable_function_call()->set_target(BERTBuffers::CallTarget::system);
  call.set_wait(true);

  Call(response, call);
  
  return LanguageService::CreateFunctionList(response, key, name(), language_service);

}

int LanguageService::StartChildProcess(HANDLE job_handle) {

  // cache
  std::string old_path = APIFunctions::GetPath();

  std::string prepend = language_descriptor_.prepend_path_;
  InterpolateString(prepend);

  // NOTE: this doesn't work when appending path -- only prepending. why?
  APIFunctions::PrependPath(prepend);

  std::string arguments = language_descriptor_.command_arguments_; 
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

