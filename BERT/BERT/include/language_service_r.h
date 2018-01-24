#pragma once

#include "language_service.h"

class LanguageServiceR : public LanguageService {

public:
  std::string r_home_;

public:
  LanguageServiceR( CallbackInfo &callback_info, COMObjectMap &object_map, DWORD dev_flags, 
                    const std::string &pipe_name, const std::string &child_path, const std::string &r_home)
    : LanguageService( LANGUAGE_R, callback_info, object_map, dev_flags, pipe_name, child_path, "R", "R" ) 
    , r_home_(r_home) {

    // set extensions we want to handle
    file_extensions_ = { "r", "rscript", "rsrc" };
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

  FUNCTION_LIST MapLanguageFunctions() {

    FUNCTION_LIST function_list;

    if (!connected_) return function_list;

    BERTBuffers::CallResponse call;
    BERTBuffers::CallResponse rsp;

    //call.mutable_function_call()->set_function("BERT$list.functions");
    call.mutable_function_call()->set_function("list-functions");
    call.mutable_function_call()->set_target(BERTBuffers::CallTarget::system);
    call.set_wait(true);

    Call(rsp, call);

    //function_list_.clear();
    if (rsp.operation_case() == BERTBuffers::CallResponse::OperationCase::kErr) return function_list; // error: no functions
    int count = 0;

    // shoehorning functions into our very simple variable syntax results 
    // in a lot of nesting. it's not clear that order is guaranteed here,
    // although that has more to do with R than protobuf.

    // this is a mess. should create a dedicated message for this (...)

    auto ParseArguments = [](BERTBuffers::Variable &args) {
      ARGUMENT_LIST arglist;
      for (auto arg : args.arr().data()) {
        std::string name;
        std::string defaultValue;
        for (auto field : arg.arr().data()) {
          if (!field.name().compare("name")) name = field.str();
          if (!field.name().compare("default")) {
            std::stringstream ss;
            switch (field.value_case()) {
            case BERTBuffers::Variable::ValueCase::kBoolean:
              ss << field.boolean() ? "TRUE" : "FALSE";
              break;
            case BERTBuffers::Variable::ValueCase::kNum:
              ss << field.num();
              break;
            case BERTBuffers::Variable::ValueCase::kStr:
              ss << '"' << field.str() << '"';
              break;
            }
            defaultValue = ss.str();
          }
        }
        if (name.length()) {
          arglist.push_back(std::make_shared<ArgumentDescriptor>(name, defaultValue));
        }
      }
      return arglist;
    };

    for (auto descriptor : rsp.result().arr().data()) {
      std::string function;
      ARGUMENT_LIST arglist;
      for (auto entry : descriptor.arr().data()) {
        if (!entry.name().compare("name")) function = entry.str();
        else if (!entry.name().compare("arguments")) arglist = ParseArguments(entry);
      }
      if (function.length()) {
        function_list.push_back(std::make_shared<FunctionDescriptor>(function, language_key_, "", "", arglist));
      }
    }

    return function_list;
  }

  int StartChildProcess(HANDLE job_handle){

    // cache
    std::string old_path = APIFunctions::GetPath();

    // append; this is architecture-specific
    std::stringstream r_path;
    r_path << r_home_ << "\\";

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

