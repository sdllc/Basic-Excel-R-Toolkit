#pragma once

#include "language_service.h"

class LanguageServiceR : public LanguageService {

public:
  std::string r_home_;

public:

  LanguageServiceR(CallbackInfo &callback_info, COMObjectMap &object_map, DWORD dev_flags)
    : LanguageService(callback_info, object_map, dev_flags)
  {
    std::string r_home;
    std::string child_path;
    std::string pipe_name;

    APIFunctions::GetRegistryString(r_home, "BERT2.RHome");
    APIFunctions::GetRegistryString(child_path, "BERT2.ControlRCommand");
    APIFunctions::GetRegistryString(pipe_name, "BERT2.OverrideRPipeName");
    
    std::string bin_path;
    APIFunctions::GetRegistryString(bin_path, "BERT2.BinDir");

    bin_path.append("/");
    bin_path.append(child_path);
    child_path = bin_path;

    if (!pipe_name.length()) {
      std::stringstream ss;
      ss << "BERT2-PIPE-R-" << _getpid();
      pipe_name = ss.str();
    }

    pipe_name_ = pipe_name;
    child_path_ = child_path;
    r_home_ = r_home;
    
    file_extensions_ = { "r", "rscript", "rsrc" };
    language_prefix_ = "R";
    language_name_ = "R";
  }

public:
  
  void Initialize() {

    if (connected_) {
      uintptr_t callback_thread_ptr = _beginthreadex(0, 0, CallbackThreadFunction, this, 0, 0);
      
      std::string library_path;
      APIFunctions::GetRegistryString(library_path, "BERT2.LibraryPath");
      library_path = StringUtilities::EscapeBackslashes(library_path);
      std::string library_command = "library(BERTModule, lib.loc = \"";
      library_command += library_path;
      library_command += "\")";

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
      code->add_line(library_command);
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

