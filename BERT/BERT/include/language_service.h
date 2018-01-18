#pragma once

#include "variable.pb.h"
#include "message_utilities.h"
#include "function_descriptor.h"
#include "callback_info.h"
#include <vector>
#include <string>

#include "language_keys.h"
#include "windows_api_functions.h"

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

  /** pipe name. FIXME: we don't need this to persist? */
  std::string pipe_name_;

  /** comms pipe */
  HANDLE pipe_handle_;

  /** key for indexing */
  uint32_t language_key_;

  /** file extensions (lowercase) */
  std::vector< std::string > file_extensions_;

  /** prefix for function names */
  std::string language_prefix_;

  /** name for ui/reference */
  std::string language_name_;

  /** overlapped structure for nonblocking io */
  OVERLAPPED io_;

  /** child process */
  DWORD child_process_id_;

  /** flag (used? FIXME) */
  bool connected_;

  /** read/write buffer */
  char *buffer_;
    
  /** path to executable */
  std::string child_path_;

  /** some dev flags that get passed around */
  DWORD dev_flags_;

  /** single reference. FIXME: why? */
  CallbackInfo &callback_info_;

  COMObjectMap &object_map_;

public:
  LanguageService(uint32_t language_key, CallbackInfo &callback_info, COMObjectMap &object_map, DWORD dev_flags,
                    const std::string &pipe_name, const std::string &child_path, 
                    const std::string &language_prefix, const std::string &language_name )
    : callback_info_(callback_info)
    , object_map_(object_map)
    , dev_flags_(dev_flags)
    , language_key_(language_key)
    , pipe_name_(pipe_name)
    , child_path_(child_path)
    , language_prefix_(language_prefix)
    , language_name_(language_name)
  {
    memset(&io_, 0, sizeof(io_));
  }

  /** preferentially use the shutdown method instead of destructor */
  ~LanguageService() {}

public:

  /** accessor */
  uint32_t key() { return language_key_; }

  /** accessor */
  std::string prefix() { return language_prefix_;  }

  /** accessor */
  std::string name() { return language_name_; }

  /** accessor */
  std::string pipe_name() { return pipe_name_; }

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
   */
  virtual void Initialize() = 0;

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
   * generate function descriptions. this one is pure virtual.
   */
  virtual FUNCTION_LIST MapLanguageFunctions() = 0;

  /** 
   * make a function call (or other type of call). note that call is not const; we are going 
   * to assign an ID for transaction management.
   *
   * function call is based on class fields only, so the default should be generally usable.
   */
  void Call(BERTBuffers::CallResponse &response, BERTBuffers::CallResponse &call);

};
