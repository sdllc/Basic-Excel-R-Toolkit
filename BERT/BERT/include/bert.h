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

#define _ATL_NO_DEBUG_CRT

#include <atlbase.h>
#include <atlcom.h>
#include <atlsafe.h>

#include <unordered_map>

#include "json11/json11.hpp"

#include "com_object_map.h"
#include "language_service.h"
#include "callback_info.h"
#include "file_change_watcher.h"
#include "user_button.h"

#define CONFIG_FILE_NAME "bert-config.json"
#define LANGUAGE_CONFIG_FILE_NAME "bert-languages.json"

class BERT {

private:
  /** the single instance */
  static BERT *instance_;

private:

  uint32_t next_user_button_id_;

  std::vector< UserButton > pending_user_buttons_;

  /** home directory comes from registry (for the time being) */
  std::string home_directory_;

  /** config data root */
  json11::Json config_;

  /** generated object map */
  COMObjectMap object_map_;

  /** marshalled excel pointer for calls from separate threads */
  IStream *stream_pointer_;

  /** excel COM pointer */
  LPDISPATCH application_dispatch_;

  /** pointer to ribbon menu */
  LPDISPATCH ribbon_menu_dispatch_;

  CallbackInfo callback_info_;

  /**
   * handle to the job object we use to manage child-processes (it
   * will kill all children when the parent process exits for any
   * reason -- reducing zombies)
   */
  HANDLE job_handle_;

  /** console process */
  DWORD console_process_id_;

  /** */
  HANDLE console_notification_handle_;

  /** FIXME: synchronize */
  std::vector <std::string> console_notifications_;

  /** pipe name for talking to console */
  std::string console_pipe_name_;

  /** some dev flags that get passed around */
  DWORD dev_flags_;

  /** watch file changes */
  FileChangeWatcher file_watcher_;

  // std::unordered_map <uint32_t, std::shared_ptr<LanguageService>> language_services_;
  std::vector< std::shared_ptr<LanguageService>> language_services_;

public:
  /** mapped functions */
  FUNCTION_LIST function_list_;

private:
  /** constructors are private (singleton) */
  BERT();

  /** constructors are private (singleton) */
  BERT(BERT const&) {};

  /** constructors are private (singleton) */
  BERT& operator = (BERT const&) {};

private:

  /** 
   * static callback function for function argument 
   * FIXME: can we use a lambda for this?
   */
  static void FileWatcherCallback(void *argument, const std::vector<std::string> &files);

  void FileWatchUpdate(const std::vector<std::string> &files);

  /** 
   * reads/loads a file, if we have a language that supports it. returns true if we 
   * have a language that supports it (not the success of the read/load operation).
   *
   * FIXME: wouldn't it be useful to return success of the read operation? we still
   * want the other result (have a language) in order to stop additional comparisons, 
   * but it's certainly useful to know if the read failed.
   */
  bool LoadLanguageFile(const std::string &file);

protected:

  /** thread for running console pipe */
  static unsigned __stdcall ConsoleThreadFunction(void *param);

  /** instance function for console pipe thread */
  unsigned InstanceConsoleThreadFunction();

  /** starts the console process. this can be delayed until needed. */
  int StartConsoleProcess();

  /** 
   * asks the console process to stop. it will stop when Excel closes regardless, 
   * because it's part of the same process job object. this will send a message to
   * the console asking it to shut down cleanly.
   */
  void ShutdownConsole();

  /**
   * updated to be generic 
   */
  json11::Json ReadConfigFile(const std::string &path);

public:

  /** 
   * get language by index. this used to be a hash key, it's now just an index
   * into a vector. it's still useful to have this method to hide the raw vector.
   */
  std::shared_ptr<LanguageService> GetLanguageService(uint32_t key);

  /** dispatch call, by language ID */
  void CallLanguage(uint32_t language_key, BERTBuffers::CallResponse &response, BERTBuffers::CallResponse &call);

  /** update (rebuild) function list; this must be done on the main thread */
  int UpdateFunctions();
  
  void RegisterLanguageCalls();

public:

  /** handles callback functions from R */
  void HandleCallback(const std::string &language);

public:

  /** single static instance of this class */
  static BERT* Instance();

public:

  /** sets COM pointers */
  void SetPointers(ULONG_PTR excel_pointer, ULONG_PTR ribbon_pointer);

  /** 
   * tail routine for enum windows proc; hides or shows matching windows
   */
  static BOOL CALLBACK ShowConsoleWindowCallback(HWND hwnd, LPARAM lParam);

  /**
   * tail routine for 2d enum proc, finding topmost excel window
   */
  static BOOL CALLBACK FocusExcelWindowCallback(HWND hwnd, LPARAM lParam);

  /** 
   * opens the console, creating the process if necessary 
   */
  void ShowConsole();

  /**
   * hides the console, without actually closing the process
   */
  void HideConsole();

  /** initializes; starts R, threads, pipes */
  void Init();

  /** shuts down pipes and processes */
  void Close();

  /** maps language functions to Excel functions */
  void MapFunctions();

  /** Excel API function callback */
  int ExcelCallback(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response);

  HRESULT AddUserButtonInternal(const UserButton &button);

  /** */
  int AddUserButton(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response, const std::string &language);

  /** */
  void RemoveUserButton(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response);

  /** */
  void ExecUserButton(uint32_t id, const std::string &language);

  /** */
  void ClearUserButtons();

  /** handles callback functions from R */
  int HandleCallbackOnThread(const std::string &language, const BERTBuffers::CallResponse *call = 0, BERTBuffers::CallResponse *response = 0);

  /** updates or inserts graphics objects */
  void UpdateGraphics(const BERTBuffers::CompositeFunctionCall &call, BERTBuffers::CallResponse &response);

};
