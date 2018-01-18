#pragma once

#include "language_service.h"

class LanguageServiceJulia : public LanguageService {

private:
  std::string julia_home_;

public:
  LanguageServiceJulia(CallbackInfo &callback_info, COMObjectMap &object_map, DWORD dev_flags,
    const std::string &pipe_name, const std::string &child_path, const std::string &julia_home)
    : LanguageService(LANGUAGE_JULIA, callback_info, object_map, dev_flags, pipe_name, child_path, "Jl", "Julia")
    , julia_home_(julia_home)
  {
    // set extensions we want to handle
    file_extensions_ = { "jl", "julia" };
  }

public:

  void Initialize() {

    /*
      uintptr_t callback_thread_ptr = _beginthreadex(0, 0, BERT::CallbackThreadFunction, this, 0, 0);

    std::string library_path;
    APIFunctions::GetRegistryString(library_path, "BERT2.LibraryPath");
    library_path = StringUtilities::EscapeBackslashes(library_path);

    std::string library_command = "library(BERTModule, lib.loc = \"";
    library_command += library_path;
    library_command += "\")";

    // get embedded startup code, split into lines
    // FIXME: why do we require that this be in multiple lines?

    */

    std::string startup_code = APIFunctions::ReadResource(MAKEINTRESOURCE(IDR_RCDATA2));
    std::vector<std::string> lines;
    StringUtilities::Split(startup_code, '\n', 1, lines, true);

    BERTBuffers::CallResponse call;

    // should maybe wait on this, so we know it's complete before we do next steps?
    // A: no, the R process will queue it anyway (implicitly), all this does is avoid wire traffic

    call.set_wait(false);
    auto code = call.mutable_code();
    for (auto line : lines) code->add_line(line);

    BERTBuffers::CallResponse rsp;
    Call(rsp, call);
          
  }
  
  FUNCTION_LIST MapLanguageFunctions() {

    FUNCTION_LIST function_list;

    if (!connected_) return function_list;

    BERTBuffers::CallResponse call;
    BERTBuffers::CallResponse response;

    //call.mutable_function_call()->set_function("BERT.ListFunctions");
    call.mutable_code()->add_line("BERT.ListFunctions()");
    call.set_wait(true);

    Call(response, call);

    if (response.operation_case() == BERTBuffers::CallResponse::OperationCase::kErr) return function_list; // error: no functions

    for (auto descriptor : response.result().arr().data()) {
      ARGUMENT_LIST arglist;

      int len = descriptor.arr().data_size();
      if (len > 0) {
        std::string function = descriptor.arr().data(0).str();
        for (int i = 1; i < len; i++) {
          arglist.push_back(std::make_shared<ArgumentDescriptor>(descriptor.arr().data(i).str()));
        }
        function_list.push_back(std::make_shared<FunctionDescriptor>(function, language_key_, "", "", arglist));
      }

    }
  
    return function_list;
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
