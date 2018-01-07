#pragma once

#define _ATL_NO_DEBUG_CRT

#include <atlbase.h>
#include <atlcom.h>
#include <atlsafe.h>

#include <unordered_map>

#include "com_object_map.h"

class CallbackInfo {
public:
  HANDLE default_unsignaled_event_;
  HANDLE default_signaled_event_;
  BERTBuffers::CallResponse callback_call_;
  BERTBuffers::CallResponse callback_response_;

public:
  CallbackInfo() {
    default_signaled_event_ = CreateEvent(0, TRUE, TRUE, 0);
    default_unsignaled_event_ = CreateEvent(0, TRUE, FALSE, 0);
  }

  ~CallbackInfo() {
    CloseHandle(default_signaled_event_);
    CloseHandle(default_unsignaled_event_);
  }

};

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

  /** pipename */
  std::string pipe_name_;

  /** pipe handle */
  HANDLE pipe_handle_;

  /** overlapped structure for nonblocking io */
  OVERLAPPED io_;

  /** child process */
  DWORD child_process_id_;

  /** console process */
  DWORD console_process_id_;

  /** flag (used? FIXME) */
  bool connected_;

  /** read/write buffer */
  char *buffer_;

  /** ID generator */
  uint32_t transaction_id_;

  /** RHOME field */
  std::string r_home_;

  /** path to controlr executable */
  std::string child_path_;

  /** some dev flags that get passed around */
  DWORD dev_flags_;

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
  static unsigned CallbackThreadFunction(void *param);
  void RunCallbackThread();

private:
  /** starts the external R host process */
  int StartChildProcess();

  /** starts the console process. this can be delayed until needed. */
  int StartConsoleProcess();

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

  /** maps global R functions to Excel functions */
  void MapFunctions();

  /** calls R; the call object has the details and semantics */
  BERTBuffers::CallResponse* RCall(BERTBuffers::CallResponse &response, BERTBuffers::CallResponse &call);

  /** Excel API function callback */
  int ExcelCallback(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response);

  /** handles callback functions from R */
  int HandleCallbackOnThread(const BERTBuffers::CallResponse *call = 0, BERTBuffers::CallResponse *response = 0);

};
