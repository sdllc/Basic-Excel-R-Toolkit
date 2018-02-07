#pragma once

#include "language_service.h"

class LanguageServiceJulia : public LanguageService {

private:
  std::string julia_home_;

public:
  /*
  LanguageServiceJulia(CallbackInfo &callback_info, COMObjectMap &object_map, DWORD dev_flags,
    const std::string &pipe_name, const std::string &child_path, const std::string &julia_home)
    : LanguageService(LANGUAGE_JULIA, callback_info, object_map, dev_flags, pipe_name, child_path, "Jl", "Julia")
    , julia_home_(julia_home)
  {

    // set extensions we want to handle
    file_extensions_ = { "jl", "julia" };
  }
  */
  LanguageServiceJulia(CallbackInfo &callback_info, COMObjectMap &object_map, DWORD dev_flags)
    : LanguageService(callback_info, object_map, dev_flags)
  {

    std::string julia_home;
    std::string child_path;
    std::string pipe_name;

    APIFunctions::GetRegistryString(julia_home, "BERT2.JuliaHome");
    APIFunctions::GetRegistryString(child_path, "BERT2.ControlJuliaCommand");
    APIFunctions::GetRegistryString(pipe_name, "BERT2.OverrideJuliaPipeName");

    if (!pipe_name.length()) {
      std::stringstream ss;
      ss << "BERT2-PIPE-JL-" << _getpid();
      pipe_name = ss.str();
    }

    pipe_name_ = pipe_name;
    child_path_ = child_path;
    julia_home_ = julia_home;

    language_prefix_ = "Jl";
    language_name_ = "Julia";
    file_extensions_ = { "jl", "julia" };

  }

public:

  void Initialize() {
    if(connected_)
      uintptr_t callback_thread_ptr = _beginthreadex(0, 0, CallbackThreadFunction, this, 0, 0);

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
