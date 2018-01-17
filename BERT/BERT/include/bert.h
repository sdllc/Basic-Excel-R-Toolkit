#pragma once

#define _ATL_NO_DEBUG_CRT

#include <atlbase.h>
#include <atlcom.h>
#include <atlsafe.h>

#include <unordered_map>

#include "com_object_map.h"
#include "language_service.h"
#include "callback_info.h"

#include "file_change_watcher.h"

class BERT {

private:
  /** the single instance */
  static BERT *instance_;

private:

  COMObjectMap object_map_;

  /** marshalled excel pointer for calls from separate threads */
  IStream *stream_pointer_;

  /** excel COM pointer */
  LPDISPATCH application_dispatch_;

  CallbackInfo callback_info_;

  /**
   * handle to the job object we use to manage child-processes (it
   * will kill all children when the parent process exits for any
   * reason -- reducing zombies)
   */
  HANDLE job_handle_;

  /** console process */
  DWORD console_process_id_;

  /** some dev flags that get passed around */
  DWORD dev_flags_;

  /** watch file changes */
  FileChangeWatcher file_watcher_;

  std::unordered_map <uint32_t, std::shared_ptr<LanguageService>> language_services_;

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

  /** starts the console process. this can be delayed until needed. */
  int StartConsoleProcess();

public:

  std::shared_ptr<LanguageService> GetLanguageService(uint32_t key);

  /** dispatch call */
  void CallLanguage(uint32_t language_key, BERTBuffers::CallResponse &response, BERTBuffers::CallResponse &call);

public:

  /** handles callback functions from R */
  void HandleCallback();

public:

  /** single static instance of this class */
  static BERT* Instance();

public:

  /** sets COM pointers */
  void SetPointers(ULONG_PTR excel_pointer, ULONG_PTR ribbon_pointer);

  /** opens the console, creating the process if necessary */
  void OpenConsole();

  /** initializes; starts R, threads, pipes */
  void Init();

  /** shuts down pipes and processes */
  void Close();

  /** maps language functions to Excel functions */
  void MapFunctions();

  /** Excel API function callback */
  int ExcelCallback(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response);

  /** handles callback functions from R */
  int HandleCallbackOnThread(const BERTBuffers::CallResponse *call = 0, BERTBuffers::CallResponse *response = 0);

};
