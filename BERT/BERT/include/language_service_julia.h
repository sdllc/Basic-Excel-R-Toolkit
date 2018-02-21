#pragma once

#include "language_service.h"

#define JULIA_LANGUAGE_NAME   "Julia"
#define JULIA_EXECUTABLE      "controljulia.exe"
#define JULIA_LANGUAGE_PREFIX "JL"
#define JULIA_EXTENSIONS      "jl", "julia"

class LanguageServiceJulia : public LanguageService {

private:
  std::string julia_home_;

public:
  LanguageServiceJulia(CallbackInfo &callback_info, COMObjectMap &object_map, DWORD dev_flags, const json11::Json &config, const std::string &home_directory)
    : LanguageService(callback_info, object_map, dev_flags)
  {

    std::string julia_home = config["BERT"]["Julia"]["home"].string_value();
    // APIFunctions::GetRegistryString(julia_home, "BERT2.JuliaHome");

    std::string pipe_name;
    APIFunctions::GetRegistryString(pipe_name, "BERT2.OverrideJuliaPipeName");
    
    std::string bin_path = home_directory;

    bin_path.append("/");
    bin_path.append(JULIA_EXECUTABLE);

    this->configured_ = julia_home.length();
    if (!this->configured_) return;

    if (!pipe_name.length()) {
      std::stringstream ss;
      ss << "BERT2-PIPE-" << JULIA_LANGUAGE_PREFIX << "-" << _getpid();
      pipe_name = ss.str();
    }

    pipe_name_ = pipe_name;
    child_path_ = bin_path;
    julia_home_ = julia_home;

    language_prefix_ = JULIA_LANGUAGE_PREFIX; 
    language_name_ = JULIA_LANGUAGE_NAME; 
    file_extensions_ = { JULIA_EXTENSIONS }; 

  }

public:

  void Initialize() {
    if(connected_)
      uintptr_t callback_thread_ptr = _beginthreadex(0, 0, CallbackThreadFunction, this, 0, 0);

    // FIXME: move startup code to control process

    // startup code
    std::string startup_code = APIFunctions::ReadResource(MAKEINTRESOURCE(IDR_RCDATA2));
    std::vector<std::string> lines;
    StringUtilities::Split(startup_code, '\n', 1, lines, true);

    {
      BERTBuffers::CallResponse call, response;
      call.set_wait(false);
      auto code = call.mutable_code();
      for (auto line : lines) code->add_line(line);
      Call(response, call);
    }

    // FIXME: make this (the post init call) generic so we can normalize

    // part two
    {
      BERTBuffers::CallResponse call, response;
      auto function_call = call.mutable_function_call();
      function_call->set_function("post-init"); 
      function_call->set_target(BERTBuffers::CallTarget::system);
      Call(response, call);
    }



  }
  
  int StartChildProcess(HANDLE job_handle) {
    
    // cache
    std::string old_path = APIFunctions::GetPath();

    // update (NOTE: if you're just concatenating strings, you can string.append)
    std::stringstream path;
    path << julia_home_ << "\\bin";
    APIFunctions::PrependPath(path.str());

    // set command line, start child process
    char *args = new char[1024];
    sprintf_s(args, 1024, "\"%s\" -p %s", child_path_.c_str(), pipe_name_.c_str());
    int result = LaunchProcess(job_handle, args);

    // restore
    APIFunctions::SetPath(old_path);

    // clean up
    delete[] args;

    return result;

  }

};
