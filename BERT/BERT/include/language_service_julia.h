#pragma once

#include "language_service.h"

#define JULIA_LANGUAGE_NAME   "Julia"
#define JULIA_EXECUTABLE      "controljulia.exe"
#define JULIA_LANGUAGE_PREFIX "JL"
#define JULIA_EXTENSIONS      "jl", "julia"
#define JULIA_STARTUP_RSRC    IDR_RCDATA2

#define JULIA_EXTRA_ARGUMENTS ""
#define JULIA_PREPEND_PATH    "$HOME\\bin"

class LanguageServiceJulia : public LanguageService {

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
    language_home_ = julia_home;

    language_prefix_ = JULIA_LANGUAGE_PREFIX; 
    language_name_ = JULIA_LANGUAGE_NAME; 
    file_extensions_ = { JULIA_EXTENSIONS }; 

    resource_id_ = JULIA_STARTUP_RSRC;

  }

public:

  int StartChildProcess(HANDLE job_handle) {
    
    // cache
    std::string old_path = APIFunctions::GetPath();

    // update (NOTE: if you're just concatenating strings, you can string.append)
    std::stringstream path;
    path << language_home_ << "\\bin";
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
