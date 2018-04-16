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

#include "variable.pb.h"
#include "message_utilities.h"
#include "function_descriptor.h"
#include "callback_info.h"
#include <vector>
#include <string>
#include <regex>

#include "windows_api_functions.h"
#include "process_exit_codes.h"

#include "json11/json11.hpp"
#include "language_desc.h"

#define PIPE_BUFFER_SIZE (1024*8)

/**
 * class abstracts common language service features
 */
class LanguageService {

private:

  /** ID generator */
  static uint32_t transaction_id_;

protected:

  /** get next id */
  static uint32_t transaction_id() { return transaction_id_++; }

  /** FIXME: unify threads, move to BERT */
  static unsigned __stdcall CallbackThreadFunction(void *param) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    auto language_service = reinterpret_cast<LanguageService*>(param);
    language_service->RunCallbackThread();
    CoUninitialize();
    return 0;
  }
  
protected:

  LanguageDescriptor language_descriptor_;

  /** hold on to process handles, we can check for exit (and exit values) */
  PROCESS_INFORMATION process_info_;

  /** pipe name. FIXME: we don't need this to persist? */
  std::string pipe_name_;

  /** comms pipe */
  HANDLE pipe_handle_;

  /** file extensions (lowercase) */
  //std::vector< std::string > file_extensions_;

  /** prefix for function names */
  //std::string language_prefix_;

  /** name for ui/reference */
  //std::string language_name_;

  /** overlapped structure for nonblocking io */
  OVERLAPPED io_;

  /** child process */
  DWORD child_process_id_;
    
  /** flag */
  bool connected_;

  /** flag */
  bool configured_;

  /** read/write buffer */
  char *buffer_;
    
  /** path to executable */
  std::string child_path_;

  /** some dev flags that get passed around */
  DWORD dev_flags_;

  /** single reference. FIXME: why? */
  CallbackInfo &callback_info_;

  COMObjectMap &object_map_;

  /** 
   * resource ID of startup code 
   * (TEMP, FIXME: move startup code to control processes)
   */
  // int32_t resource_id_;

  /** extra command-line arguments (parameterized) */
  //std::string command_line_arguments_;

  /** home directory */
  //std::string language_home_;

  /** will be prepended to the path; parameterized */
  //std::string prepend_path_;

public:

  //LanguageService(CallbackInfo &callback_info, COMObjectMap &object_map, DWORD dev_flags, const json11::Json &config, const std::string &home_directory, const LanguageDescriptor &descriptor);
  LanguageService(CallbackInfo &callback_info, COMObjectMap &object_map, DWORD dev_flags, const json11::Json &config, const std::string &home_directory, const json11::Json &descriptor);

  /** preferentially use the shutdown method instead of destructor */
  ~LanguageService() {}

public:

  /** 
   * wrapper for the WIN32 api function. returns true if the function succeeds 
   * or false; usually that means invalid handle, if the process was never created.
   * you can test that with GetLastError().
   */
  bool ProcessExitCode(DWORD *exit_code);

  bool configured() { return configured_; }

  bool connected() { return connected_; }

  /** accessor */
  std::string prefix() { return language_descriptor_.prefix_;  }

  /** accessor */
  std::string name() { return language_descriptor_.name_; }

  /** accessor */
  std::string pipe_name() { return pipe_name_; }

  /** accessor */
  bool named_arguments() { return language_descriptor_.named_arguments_;  }

protected:

  /** abstracts process launch (we use common properties) */
  int LaunchProcess(HANDLE job_handle, char *command_line);

public:

  /**
   * read/load the file. FIXME: move functionality to service?
   *
   * UPDATE: we now export this to the services, there's a generic
   * implementation (you can still override if desired)
   */
  virtual void ReadSourceFile(const std::string &file);

  /** 
   * can we process this file (via extension)?
   */
  bool ValidFile(const std::string &path);

  /** 
   * starts child process. assigns child process to job to control shutdown. 
   * for R/julia the important part is setting path for module resolution.
   */
  virtual int StartChildProcess(HANDLE job_handle);

  /**
   * connects to child process. this part is generic. language-specific parts
   * are now in the Initialize() method.
   */
  void Connect(HANDLE job_handle);

  /** 
   * second of two-part connect/initialize. abstract. 
   * UPDATE: not abstract. parameterized.
   */
  void Initialize();

  /**
   * clean up processes, pipes, resources
   */
  virtual void Shutdown();

  /**
   * FIXME: we don't need to run separate threads for each client, just
   * use a single thread (with mutliple pipes, of course). note that
   * this may require either (1) splitting init in two, or (2) some 
   * jujitsu on the running thread to insert a read handle.
   */
  void RunCallbackThread();

  /**
   * set COM pointer
   */
  void SetApplicationPointer(LPDISPATCH application_pointer);

  /** 
   * generate function descriptions. generic.
   */
  FUNCTION_LIST MapLanguageFunctions(uint32_t key, std::shared_ptr<LanguageService> language_service);

  /** 
   * make a function call (or other type of call). note that call is not const; we are going 
   * to assign an ID for transaction management.
   *
   * function call is based on class fields only, so the default should be generally usable.
   */
  void Call(BERTBuffers::CallResponse &response, BERTBuffers::CallResponse &call);

  /**
   * replace tokens in string. FIXME: make more generic
   *
   * UPDATE: the second parameter takes pairs of strings for needle -> replacement
   */
  void InterpolateString(std::string &str, const std::vector<std::pair<std::string, std::string>> &additional_replacements = {});

public:

  /** 
   * FIXME: move to conversion lib? 
   */
  static FUNCTION_LIST CreateFunctionList(const BERTBuffers::CallResponse &message, uint32_t key, const std::string &name, std::shared_ptr<LanguageService> service_pointer);

};
