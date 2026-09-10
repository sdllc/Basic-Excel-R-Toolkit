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

/**
 * function descriptions, in the format the Excel-DNA IntelliSense add-in
 * reads. the name pairs with the carrier add-in BERT-IntelliSense.xlam:
 * that add-in does nothing except give the IntelliSense add-in something
 * to find this file beside. see docs/FUNCTION-HELP.md.
 */
#define INTELLISENSE_FILE_NAME "BERT-IntelliSense.intellisense.xml"

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

  /**
   * excel COM pointer. initialized here because it is read to decide
   * whether we have one yet -- the constructor never set it, and reading
   * an uninitialized pointer to test it is undefined even when the ribbon
   * fills it in a moment later.
   */
  LPDISPATCH application_dispatch_ = 0;

  /** true when we acquired the Application pointer ourselves, so we hold a
    * reference on it and have to give it back */
  bool owns_application_dispatch_ = false;

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

  /** the directory this dll is in, which is also where the config lives */
  const std::string& home_directory() const { return home_directory_; }

public:

  /** single static instance of this class */
  static BERT* Instance();

public:

  /** sets COM pointers */
  void SetPointers(ULONG_PTR excel_pointer, ULONG_PTR ribbon_pointer);

  /**
   * excel's Application object, which drawing into a sheet and the EXCEL
   * object in R both need.
   *
   * the ribbon add-in hands this over as it loads, and used to be the only
   * source of it: with no ribbon there was no pointer, and graphics quietly
   * drew nothing. AcquireApplicationDispatch gets one without it.
   */
  LPDISPATCH ApplicationDispatch();

  /**
   * asks excel for the Application pointer directly, the way an add-in with
   * no COM component of its own has to. does nothing if we already have
   * one, and returns whether we have one now.
   *
   * MUST be called while excel is idle -- from xlAutoOpen, not from a
   * callback. asking for the object model while a cell is calculating hangs
   * excel. see docs/XLL-ONLY.md.
   */
  bool AcquireApplicationDispatch();

  /** takes an Application pointer, from the ribbon or from excel directly */
  void UseApplicationDispatch(LPDISPATCH application_dispatch);

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
   * binds CONTROL+SHIFT+R to the console command, with ON.KEY.
   *
   * not with xlfRegister's shortcut field: a shortcut registered there does
   * fire, but excel then resolves the command in workbook scope and reports
   * "cannot run the macro '!BERT.Console'". ON.KEY sends the name through
   * the same path as ExecuteExcel4Macro, which is what the ribbon button
   * uses and which does resolve it. pass false to give the key back.
   */
  void BindConsoleKey(bool bind = true);

  /** true if the console process we started is still running */
  bool ConsoleProcessRunning();

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
