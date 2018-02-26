
#include "stdafx.h"
#include "variable.pb.h"
#include "XLCALL.h"
#include "function_descriptor.h"
#include "bert.h"
#include "basic_functions.h"
#include "type_conversions.h"
#include "string_utilities.h"
#include "windows_api_functions.h"
#include "message_utilities.h"
#include "..\resource.h"

#include "excel_com_type_libraries.h"
#include "excel_api_functions.h"

#include "bert_version.h"

// --- temp (pending move to config) --------------

std::vector < LanguageDescriptor > language_descriptors = {
  LanguageDescriptor("R", "controlr.exe", "R", { "r", "rsrc", "rscript" }, "-r \"$HOME\"", "$HOME\\bin\\$ARCH", IDR_RCDATA1),
  LanguageDescriptor("Julia", "controljulia.exe", "JL", { "jl", "julia" }, "", "$HOME\\bin", IDR_RCDATA2)
};

// ------------------------------------------------

BERT* BERT::instance_ = 0;

BERT* BERT::Instance() {
  if (!instance_) instance_ = new BERT;
  return instance_;
}

void BERT::ReadConfigFile() {

  std::string config_file_path;
  std::string config_contents;

  // FIXME: set defaults?
  config_ = json11::Json();

  // home directory set in ctor, will have trailing path sep
  config_file_path = home_directory_;
  config_file_path.append(CONFIG_FILE_NAME);

  HANDLE file_handle = CreateFileA(config_file_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (!file_handle || file_handle == INVALID_HANDLE_VALUE) {
    std::cerr << "WARNING: config file not found" << std::endl;
    return; // FIXME: warn user
  }

  DWORD bytes_read = 0;
  DWORD buffer_size = 8 * 1024;
  char *buffer = new char[buffer_size];
  std::string config_data;

  // if this returns false, that's an error, it returns true on EOF

  while (ReadFile(file_handle, buffer, buffer_size, &bytes_read, 0) && bytes_read) {
    config_data.append(buffer, bytes_read);
  }
  CloseHandle(file_handle);

  delete[] buffer;

  std::string parse_error;
  config_ = json11::Json::parse(config_data, parse_error, json11::COMMENTS);

}

void BERT::FileWatcherCallback(void *argument, const std::vector<std::string> &files) {
  BERT *bert = reinterpret_cast<BERT*>(argument);
  bert->FileWatchUpdate(files);
}

bool BERT::LoadLanguageFile(const std::string &file) {

  // are we looping twice, once over files and then over languages? 
  // is that the best way to do this? [obviously not]

  // check available langauges
  for (auto language_service : language_services_) {
    if (language_service->ValidFile(file)) {
      DebugOut("File %s matches %s\n", file.c_str(), language_service->name().c_str());

      // FIXME: log to console (or have the internal routine do that)
      language_service->ReadSourceFile(file);

      // match
      return true;
    }
  }

  // no matching language
  return false;
}

void BERT::FileWatchUpdate(const std::vector<std::string> &files) {

  bool updated = false;
  for (auto file : files) {
    updated = updated || LoadLanguageFile(file);
  }

  // any changes?
  if (updated) {

    DebugOut("Updating...\n");

    // NOTE: this has to get on the correct thread. use COM to switch contexts
    // (and use the marshaled pointer) 
    // (and don't forget to release reference)

    LPDISPATCH dispatch_pointer = 0;
    HRESULT hresult = AtlUnmarshalPtr(stream_pointer_, IID_IDispatch, (LPUNKNOWN*)&dispatch_pointer);
    if (SUCCEEDED(hresult) && dispatch_pointer) {
      CComQIPtr<Excel::_Application> application(dispatch_pointer);
      if (application) {
        CComVariant variant_macro = "BERT.UpdateFunctions";
        CComVariant variant_result = application->Run(variant_macro);
      }
      dispatch_pointer->Release();
    }

  }
}

int BERT::UpdateFunctions() {

  // FIXME: notify user via console

  // unregister uses IDs assigned in the registration process. so we need
  // to call unregister before map, which will change the function list.
  UnregisterFunctions();

  // scrub
  function_list_.clear();

  // now update
  MapFunctions();
  RegisterFunctions();

  return 0;

}

BERT::BERT()
  : dev_flags_(0)
  , file_watcher_(BERT::FileWatcherCallback, this)
  , stream_pointer_(0)
  , console_notification_handle_(0)
{
  APIFunctions::GetRegistryDWORD(dev_flags_, "BERT2.DevOptions");
  home_directory_ = APIFunctions::ModulePath();
  
  if (!home_directory_.length()) {
    std::cerr << "WARNING: home directory not found" << std::endl;
  }

}

BOOL CALLBACK BERT::ShowConsoleWindowCallback(HWND hwnd, LPARAM lParam)
{
  DWORD pid;
  GetWindowThreadProcessId(hwnd, &pid);

  bool show = (bool)lParam;

  char buffer[256];
  char className[256];

  if (pid == Instance()->console_process_id_) {

    //
    // FIXME: watch out, this could be fragile...
    // probably match on just the Chrome_ part, should be 
    // specific enough (since we're matching PID)
    //

    ::RealGetWindowClassA(hwnd, className, 256);
    if (!strcmp(className, "Chrome_WidgetWin_1")) {

      GetWindowTextA(hwnd, buffer, 256);
      DebugOut("WINDOW %X: %s\n", pid, buffer);

      long style = GetWindowLong(hwnd, GWL_STYLE);
      long exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
      DebugOut("PRE: 0x%X, 0x%X\n", style, exstyle);

      if (show) {
        style |= WS_VISIBLE;
        exstyle |= WS_EX_APPWINDOW;
        exstyle &= ~(WS_EX_TOOLWINDOW);
      }
      else {
        style &= ~(WS_VISIBLE);
        exstyle |= WS_EX_TOOLWINDOW;
        exstyle &= ~(WS_EX_APPWINDOW);
      }

      ShowWindow(hwnd, SW_HIDE);

      SetWindowLong(hwnd, GWL_EXSTYLE, exstyle);
      SetWindowLong(hwnd, GWL_STYLE, style);

      if (show) {
        if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
      }

      style = GetWindowLong(hwnd, GWL_STYLE);
      exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
      DebugOut("POST: 0x%X, 0x%X\n", style, exstyle);

    }
  }
  return TRUE;
}

struct handle_data {
  unsigned long process_id;
  HWND best_handle;
};

BOOL CALLBACK BERT::FocusExcelWindowCallback(HWND hwnd, LPARAM lParam) {

  DWORD pid = (DWORD)lParam;
  DWORD window_pid;

  GetWindowThreadProcessId(hwnd, &window_pid);

  if ((pid != window_pid)
    || (GetWindow(hwnd, GW_OWNER) != (HWND)0)
    || !IsWindowVisible(hwnd)) return TRUE;

  /*
  char className[256];
  ::RealGetWindowClassA(hwnd, className, 256);
  std::cout << "found window, focusing: " << className << std::endl;
  */

  SetForegroundWindow(hwnd);
  SetActiveWindow(hwnd);
  SetFocus(hwnd);

  return FALSE;
}

void BERT::HideConsole() {
  EnumWindows(BERT::ShowConsoleWindowCallback, false);

  DWORD pid = _getpid();
  EnumWindows(BERT::FocusExcelWindowCallback, (LPARAM)pid);
}

void BERT::ShowConsole() {
  if (console_process_id_) {
    EnumWindows(BERT::ShowConsoleWindowCallback, true);
  }
  else {
    StartConsoleProcess();
  }
}

void BERT::SetPointers(ULONG_PTR excel_pointer, ULONG_PTR ribbon_pointer) {

  application_dispatch_ = reinterpret_cast<LPDISPATCH>(excel_pointer);

  // marshall pointer
  AtlMarshalPtrInProc(application_dispatch_, IID_IDispatch, &stream_pointer_);

  // set pointer in various language services
  for (const auto &language_service : language_services_) {
    language_service->SetApplicationPointer(application_dispatch_);
  }

}

void BERT::ShutdownConsole() {
  if (!console_process_id_) return;
  if (!console_notification_handle_) return; // can't

  BERTBuffers::CallResponse message;
  auto function_call = message.mutable_function_call();
  function_call->set_target(BERTBuffers::CallTarget::system);
  function_call->set_function("shutdown-console");

  console_notifications_.push_back(MessageUtilities::Frame(message));
  ::SetEvent(console_notification_handle_);

}

unsigned __stdcall BERT::ConsoleThreadFunction(void *param) {
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  unsigned result = Instance()->InstanceConsoleThreadFunction();
  CoUninitialize();
  return result;
}

unsigned BERT::InstanceConsoleThreadFunction(){

  std::string pipe_name = "\\\\.\\pipe\\";
  pipe_name.append(console_pipe_name_);

  uint32_t buffer_size = 2048;
  char *buffer = 0;

  OVERLAPPED read_io, write_io;
  DWORD error = 0;
  DWORD bytes_read, bytes_written;
  DWORD result;

  bool reading = false;
  bool connected = false;

  console_notification_handle_ = ::CreateEvent(0, TRUE, FALSE, 0);

  HANDLE handle = CreateNamedPipeA(pipe_name.c_str(),
    PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
    4, buffer_size, buffer_size, 100, NULL);

  if (NULL == handle || handle == INVALID_HANDLE_VALUE) {
    std::cerr << "create pipe failed" << std::endl;
    return -1;
  }

  memset(&read_io, 0, sizeof(read_io));
  memset(&write_io, 0, sizeof(write_io));

  read_io.hEvent = CreateEvent(0, TRUE, FALSE, 0);
  write_io.hEvent = CreateEvent(0, TRUE, FALSE, 0);

  buffer = new char[buffer_size];

  if (ConnectNamedPipe(handle, &read_io)) {
    std::cerr << "ERR in connectNamedPipe" << std::endl;
    error = -1;
  }
  else {
    error = GetLastError();
    switch (error) {
    case ERROR_PIPE_CONNECTED:
      SetEvent(read_io.hEvent);
      error = 0;
      break;
    case ERROR_IO_PENDING:
      error = 0;
      break;
    default:
      std::cerr << "connect failed with " << error << std::endl;
      break;
    }
  }

  std::vector < HANDLE > handles = { read_io.hEvent, console_notification_handle_ };

  while (!error) {

    if (!reading && connected) {
      ReadFile(handle, buffer, buffer_size, 0, &read_io);
      reading = true;
    }

    //result = WaitForSingleObject(read_io.hEvent, 1000);
    result = WaitForMultipleObjects(2, &(handles[0]), FALSE, 1000);
    if (result == WAIT_OBJECT_0 + 1) {
      ::ResetEvent(handles[1]);
      std::string message = console_notifications_[0];
      console_notifications_.erase(console_notifications_.begin());
      WriteFile(handle, message.c_str(), (DWORD)message.length(), &bytes_written, &write_io);
      result = GetOverlappedResultEx(handle, &write_io, &bytes_written, INFINITE, FALSE);
    }
    else if (result == WAIT_OBJECT_0) {
      ResetEvent(read_io.hEvent);
      reading = false;
      result = GetOverlappedResultEx(handle, &read_io, &bytes_read, 0, FALSE);
      if (result) {
        if (!connected) {
          std::cout << " * connected to mgmt pipe " << std::endl;
          connected = true;
        }
        else {
          std::cout << " * read message mgmt pipe " << std::endl;
          BERTBuffers::CallResponse call, reply;
          MessageUtilities::Unframe(call, buffer, bytes_read);

          reply.set_id(call.id());
          auto result = reply.mutable_result();
          bool success = false;

          switch (call.operation_case()) {
          case BERTBuffers::CallResponse::OperationCase::kFunctionCall:
          {
            const std::string &function = call.function_call().function();
            // we should validate target
            std::cout << "console: " << function << std::endl;
            if (!function.compare("hide-console")) {

              // can we do this on this thread? (...)
              HideConsole();
            }
            break;
          }
          default:
            std::cout << "Unexpected operation case: " << call.operation_case() << std::endl;
            break;
          }
          
          result->set_boolean(success);
          std::string data = MessageUtilities::Frame(reply);

          WriteFile(handle, data.c_str(), (DWORD)data.length(), NULL, &write_io);

        }
      }
      else {
        error = GetLastError();
        if (error == ERROR_BROKEN_PIPE) {

          // reset and wait for reconnect

          std::cout << " * broken pipe [2] " << std::endl;

          ResetEvent(read_io.hEvent);
          ResetEvent(write_io.hEvent);
          DisconnectNamedPipe(handle);

          connected = false;
          error = 0;

          if (ConnectNamedPipe(handle, &read_io)) {
            std::cerr << "ERR in connectNamedPipe" << std::endl;
            error = -1;
          }

        }
        else {
          DebugOut("error in GORE (console pipe): %d\n", error);
          break;
        }
      }
    }
    else if (result == ERROR_BROKEN_PIPE) {
      std::cout << " * broken pipe [1] " << std::endl;
      connected = false;
    }
    else if (result != WAIT_TIMEOUT) {
      error = GetLastError();
      DebugOut("error in WaitForSingleObject: %d\n", error);
    }
  }

  CloseHandle(read_io.hEvent);
  CloseHandle(write_io.hEvent);
  CloseHandle(handle);
  CloseHandle(console_notification_handle_);

  console_notification_handle_ = 0;

  delete[] buffer;

  return 0;
}

int BERT::StartConsoleProcess() {

  std::string console_command;
  std::string console_arguments;

  std::stringstream pipe_name;
  pipe_name << "BERT-console-pipe-";
  pipe_name << _getpid();

  if (dev_flags_) {
    APIFunctions::GetRegistryString(console_pipe_name_, "BERT2.OverrideManagementPipeName");
  }

  if (!console_pipe_name_.length()) console_pipe_name_ = pipe_name.str();
  uintptr_t console_thread_ptr = _beginthreadex(0, 0, ConsoleThreadFunction, this, 0, 0);

  if (dev_flags_) {
    APIFunctions::GetRegistryString(console_command, "BERT2.OverrideConsoleCommand");
    APIFunctions::GetRegistryString(console_arguments, "BERT2.OverrideConsoleArguments");
  }

  if (!console_command.length()) {
    console_command = home_directory_;
    console_command.append("console\\win-unpacked\\bert2-console.exe");
  }

  int rslt = 0;

  STARTUPINFOA si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&pi, sizeof(pi));
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);

  std::stringstream command_line;
  command_line << "\"" << console_command << "\"";

  if(console_arguments.length()) command_line << " " << console_arguments;

  // add flag for management pipe
  command_line << " -m " << console_pipe_name_;

  // add a -p (pipe) for each language
  for (auto language_service : language_services_) {
    command_line << " -p " << language_service->pipe_name();
  }

  // pass complete dev flags, process can parse
  command_line << " -d " << dev_flags_;

  // we need non-const char buffer for createprocess

  auto command_line_string = command_line.str();
  size_t len = command_line_string.length();
  char *command_line_buffer = new char[len + 1];

  memcpy(command_line_buffer, command_line_string.c_str(), len);
  command_line_buffer[len] = 0;

  if (!CreateProcessA(0, command_line_buffer, 0, 0, FALSE, 0, 0, 0, &si, &pi)) {
    DebugOut("CreateProcess failed (%d).\n", GetLastError());
    rslt = GetLastError();
  }
  else {
    console_process_id_ = pi.dwProcessId;
    if (job_handle_) {
      if (!AssignProcessToJobObject(job_handle_, pi.hProcess))
      {
        DebugOut("Could not AssignProcessToObject\n");
      }
    }
  }

  delete[] command_line_buffer;

  return rslt;
}

void BERT::HandleCallback() {

  // this function gets called from a thread (i.e. not the main excel thread),
  // so we cannot call the excel API but we can call excel via COM using the 
  // marshalled pointer.

  // if we want to support the Excel API (and I suppose we do) we'll need to 
  // get back on the main thread. we could do that with COM, and a call-through,
  // but I'm not sure if that will preserve context (from a function call).

  // if COM doesn't work in this situation we'll need to use an event we can 
  // signal the blocked thread, but we will need to know if we are in fact 
  // blocking that thread. we'll also need a way to pass data back and forth.

  // SO: COM doesn't work in that case. we need to signal. 

  // there are two possible cases. either we are being called from a spreadsheet
  // function or we're being called from a shell function. the semantics are different
  // because the main thread may or may not be blocked.

  DWORD wait_result = WaitForSingleObject(callback_info_.default_signaled_event_, 0);
  if (wait_result == WAIT_OBJECT_0) {
    // DebugOut("event 2 is already signaled; this is a shell function\n");

    BERTBuffers::CallResponse &call = callback_info_.callback_call_;
    BERTBuffers::CallResponse &response = callback_info_.callback_response_;

    if (stream_pointer_) {
      LPDISPATCH dispatch_pointer = 0;
      CComVariant variant_result;
      HRESULT hresult = AtlUnmarshalPtr(stream_pointer_, IID_IDispatch, (LPUNKNOWN*)&dispatch_pointer);

      if (SUCCEEDED(hresult)) {
        CComQIPtr<Excel::_Application> application(dispatch_pointer);
        if (application) {
          CComVariant variant_macro = "BERT.ContextSwitch";
          CComVariant variant_result = application->Run(variant_macro);
        }
        else response.set_err("qi failed");
        dispatch_pointer->Release();
      }
      else response.set_err("unmarshal failed");
    }
    else {
      response.set_err("invalid stream pointer");
    }

  }
  else {
    DebugOut("event 2 is not signaled; this is a spreadsheet function\n");
    DebugOut("callback waiting for signal\n");

    // let main thread handle
    SetEvent(callback_info_.default_unsignaled_event_);
    WaitForSingleObject(callback_info_.default_signaled_event_, INFINITE);
    DebugOut("callback signaled\n");
  }
}

int BERT::HandleCallbackOnThread(const BERTBuffers::CallResponse *call, BERTBuffers::CallResponse *response) {

  if (!call) call = &(callback_info_.callback_call_);
  if (!response) response = &(callback_info_.callback_response_);

  int return_value = 0;

  // DumpJSON(call);
  response->set_id(call->id());

  if (call->operation_case() == BERTBuffers::CallResponse::OperationCase::kFunctionCall) {

    auto callback = call->function_call();
    if (callback.target() == BERTBuffers::CallTarget::language) {

      auto function = callback.function();
      if (!function.compare("excel")) {
        return_value = ExcelCallback(*call, *response);
      }
      else if (!function.compare("release-pointer")) {
        if (callback.arguments_size() > 0) {
          uint64_t pointer = callback.arguments(0).com_pointer().pointer();
          DebugOut("release pointer 0x%lx\n", pointer);
          object_map_.RemoveCOMPointer(static_cast<ULONG_PTR>(pointer));
        }
      }
      else {
        response->mutable_result()->set_boolean(false);
      }
    }
    else if (callback.target() == BERTBuffers::CallTarget::COM) {
      object_map_.InvokeCOMFunction(callback, *response);
    }
    else {
      response->mutable_result()->set_boolean(false);
    }
  }
  else {
    response->mutable_result()->set_boolean(false);
  }

  // response.mutable_value()->set_boolean(true);
  return return_value;

}

int BERT::ExcelCallback(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response) {

  auto callback = call.function_call();
  auto function = callback.function();

  int32_t command = 0;
  int32_t success = -1;

  if (callback.arguments_size() > 0) {
    auto arguments_array = callback.arguments(0).arr();

    int count = arguments_array.data().size();
    if (count > 0) {
      if (arguments_array.data(0).value_case() == BERTBuffers::Variable::ValueCase::kReal) command = (int32_t)arguments_array.data(0).real();
      else command = (int32_t)arguments_array.data(0).integer();
    }
    if (command) {
      XLOPER12 excel_result;
      std::vector<LPXLOPER12> excel_arguments;
      for (int i = 1; i < count; i++) {
        LPXLOPER12 argument = new XLOPER12;
        excel_arguments.push_back(Convert::VariableToXLOPER(argument, arguments_array.data(i)));
      }
      if (excel_arguments.size()) success = Excel12v(command, &excel_result, (int32_t)excel_arguments.size(), &(excel_arguments[0]));
      else success = Excel12(command, &excel_result, 0, 0);
      Convert::XLOPERToVariable(response.mutable_result(), &excel_result);
      Excel12(xlFree, 0, 1, &excel_result);
    }
  }

  return success;
}

void BERT::Init() {

  ReadConfigFile();

  // the job object created here is used to kill child processes
  // in the event of an excel exit (for any reason).

  job_handle_ = CreateJobObject(0, 0);

  if (job_handle_) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
    memset(&jeli, 0, sizeof(jeli));
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job_handle_, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli)))
    {
      DebugOut("Could not SetInformationJobObject\n");
    }
  }
  else {
    DebugOut("Create job object failed\n");
  }

  // before creating child processes, set env var
  SetEnvironmentVariableA("BERT2_HOME_DIRECTORY", home_directory_.c_str());

  // who uses this one? [it's in the banners]
  SetEnvironmentVariableW(L"BERT_VERSION", BERT_VERSION);

  // set up initial languages 
  // connect all first; then initialize
  for (const auto &descriptor : language_descriptors) {
    auto service = std::make_shared<LanguageService>(callback_info_, object_map_, dev_flags_, config_, home_directory_, descriptor);
    if (service->configured()) {
      service->Connect(job_handle_); // is this synchronous? we can do these in parallel
      language_services_.push_back(service);
    }
    else std::cerr << "r service not configured, skipping" << std::endl;
  }
  
  // ... insert callback thread ...

  // we need to check if connection failed and if so, remove the service
  // FIXME: too long? max is 1s
  // UPDATE: it will check for exit, so that 1s is unlikely unless it's hung

  std::vector<std::shared_ptr<LanguageService>> connected;
  for (const auto &language_service : language_services_) {
    if (language_service->connected()) {
      language_service->Initialize();
      connected.push_back(language_service);
    }
    else {
      std::cerr << "ERR: language " << language_service->name() << " not connected" << std::endl;
      DWORD exit_code;
      if (language_service->ProcessExitCode(&exit_code)) {
        std::cerr << "exit code was " << exit_code << std::endl;
      }
      else {
        std::cerr << "exit code failed; probably never started" << std::endl;
      }
    }
  }
  language_services_.clear();
  language_services_.assign(connected.begin(), connected.end());

  bool start_console = false;

  std::string command_line = GetCommandLineA();
  ::MessageBoxA(0, command_line.c_str(), "CL", MB_OK);
  if (command_line.find("/x:BERT") != std::string::npos) {
    start_console = true;
  }

  if (start_console || (dev_flags_ & 0x02)) StartConsoleProcess();

  // load code from starup folder(s). then start watching folders.

  {
    std::string functions_directory = config_["BERT"]["functionsDirectory"].string_value();
    if (functions_directory.length()) {

      DWORD result = ExpandEnvironmentStringsA(functions_directory.c_str(), 0, 0);
      if (result) {
        char *buffer = new char[result + 1];
        result = ExpandEnvironmentStringsA(functions_directory.c_str(), buffer, result+1);
        if (result) functions_directory = buffer;
        delete[] buffer;
      }

      // list...
      std::vector< std::pair< std::string, FILETIME >> directory_entries = APIFunctions::ListDirectory(functions_directory);

      // load...
      for (auto file_info : directory_entries) {
        LoadLanguageFile(file_info.first);
      }

      // now watch
      file_watcher_.WatchDirectory(functions_directory);
    }
    file_watcher_.StartWatch();
  }

  // create exec and call functions for languages


}

void BERT::RegisterLanguageCalls() {
  function_list_.clear();
  int index = 0;
  for (auto language_service : language_services_) {
    ExcelRegisterLanguageCalls(language_service->name().c_str(), index++);
  }
}

void BERT::MapFunctions() {
  function_list_.clear();
  int index = 0;
  for (auto language_service : language_services_) {
    FUNCTION_LIST functions = language_service->MapLanguageFunctions(index++);
    function_list_.insert(function_list_.end(), functions.begin(), functions.end());
  }
}

std::shared_ptr<LanguageService> BERT::GetLanguageService(uint32_t key) {
  return language_services_[key];
}

void BERT::CallLanguage(uint32_t language_key, BERTBuffers::CallResponse &response, BERTBuffers::CallResponse &call) {
  auto language_service = GetLanguageService(language_key);
  if (language_service) language_service->Call(response, call);
}

void BERT::Close() {

  // file watch thread
  file_watcher_.Shutdown();

  ShutdownConsole();

  // shutdown services
  for (const auto &language_service : language_services_) {
    language_service->Shutdown();
  }

  // free marshalled pointer
  if (stream_pointer_) AtlFreeMarshalStream(stream_pointer_);

}
