#pragma once

#include "language_service.h"

#define R_LANGUAGE_NAME   "R"
#define R_EXECUTABLE      "controlr.exe"
#define R_LANGUAGE_PREFIX "R"
#define R_EXTENSIONS      "r", "rsrc", "rscript"

class LanguageServiceR : public LanguageService {

public:
  std::string r_home_;

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
    r_home_ = r_home;
    
    file_extensions_ = { R_EXTENSIONS };
    language_prefix_ = R_LANGUAGE_PREFIX;
    language_name_ = R_LANGUAGE_NAME;
  }

public:
  
  void Initialize() {

    if (connected_) {
      uintptr_t callback_thread_ptr = _beginthreadex(0, 0, CallbackThreadFunction, this, 0, 0);
      
      // get embedded startup code, split into lines
      // FIXME: why do we require that this be in multiple lines?

      std::string startup_code = APIFunctions::ReadResource(MAKEINTRESOURCE(IDR_RCDATA1));
      std::vector<std::string> lines;
      StringUtilities::Split(startup_code, '\n', 1, lines, true);

      BERTBuffers::CallResponse call, response;

      // should maybe wait on this, so we know it's complete before we do next steps?
      // A: no, the R process will queue it anyway (implicitly), all this does is avoid wire traffic

      call.set_wait(false);
      auto code = call.mutable_code();
      for (auto line : lines) code->add_line(line);
      Call(response, call);
    }

  }


  int StartChildProcess(HANDLE job_handle){

    // cache
    std::string old_path = APIFunctions::GetPath();

    // append; this is architecture-specific
    std::stringstream r_path;
    r_path << r_home_ << "\\";

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
    sprintf_s(args, 1024, "\"%s\" -p %s -r %s", child_path_.c_str(), pipe_name_.c_str(), r_home_.c_str());

    int result = LaunchProcess(job_handle, args);

    // clean up
    delete[] args;

    // restore
    APIFunctions::SetPath(old_path);

    return result;
  }


};

