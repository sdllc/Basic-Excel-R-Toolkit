#pragma once

#include "language_service.h"

#define R_LANGUAGE_NAME   "R"
#define R_EXECUTABLE      "controlr.exe"
#define R_LANGUAGE_PREFIX "R"
#define R_EXTENSIONS      "r", "rsrc", "rscript"
#define R_STARTUP_RSRC    IDR_RCDATA1

#define R_EXTRA_ARGUMENTS "-r $HOME"
#define R_PREPEND_PATH    "$HOME\\bin\\$ARCH"

class LanguageServiceR : public LanguageService {


public:

  LanguageServiceR(CallbackInfo &callback_info, COMObjectMap &object_map, DWORD dev_flags, const json11::Json &config, const std::string &home_directory)
    : LanguageService(callback_info, object_map, dev_flags)
  {
    std::string r_home = config["BERT"]["R"]["home"].string_value();

    std::string pipe_name;
    APIFunctions::GetRegistryString(pipe_name, "BERT2.OverrideRPipeName");
    
    this->configured_ = r_home.length();
    if (!this->configured_) return;

    std::string bin_path = home_directory;

    bin_path.append("/");
    bin_path.append(R_EXECUTABLE);
    //child_path = bin_path;

    if (!pipe_name.length()) {
      std::stringstream ss;
      ss << "BERT2-PIPE-" << R_LANGUAGE_PREFIX << "-" << _getpid();
      pipe_name = ss.str();
    }

    pipe_name_ = pipe_name;
    child_path_ = bin_path;
    language_home_ = r_home;
    
    file_extensions_ = { R_EXTENSIONS };
    language_prefix_ = R_LANGUAGE_PREFIX;
    language_name_ = R_LANGUAGE_NAME;

    resource_id_ = R_STARTUP_RSRC;

  }

public:
  
  int StartChildProcess(HANDLE job_handle){

    // cache
    std::string old_path = APIFunctions::GetPath();

    // append; this is architecture-specific
    std::stringstream r_path;
    r_path << language_home_ << "\\";

    // we're not officially supporting 32-bit windows (or 32-bit R) anymore,
    // so this can drop (or rather get hardcoded to 64).

#ifdef _WIN64
    r_path << "bin\\x64";
#else
    r_path << "bin\\i386";
#endif

    // NOTE: this doesn't work when appending path -- only prepending. why?
    APIFunctions::PrependPath(r_path.str());

    // construct shell command and launch process
    char *args = new char[1024];
    sprintf_s(args, 1024, "\"%s\" -p %s -r %s", child_path_.c_str(), pipe_name_.c_str(), language_home_.c_str());

    int result = LaunchProcess(job_handle, args);

    // clean up
    delete[] args;

    // restore
    APIFunctions::SetPath(old_path);

    return result;
  }


};

