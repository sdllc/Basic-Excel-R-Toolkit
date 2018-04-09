
#ifndef __IO_REDIRECTOR_H
#define __IO_REDIRECTOR_H

#include <stdint.h>
#include <sstream>
#include <iostream>
#include <string>

#define IOR_BUFFER_SIZE (1024 * 4)

/**
 * apparently we can't override stdout with an async pipe. we can use 
 * a synchronous pipe, and pipe _that_ into an async pipe. which seems
 * like a big headache (not to mention a waste of resources), but maybe
 * is still the best solution.
 *
 * we could also just have the sync pipe write buffers and set events,
 * which would work but will require some additional locks.
 */
class IORedirector {

protected:
  static unsigned __stdcall ThreadFunc(void *argument) {
    IORedirector* instance = static_cast<IORedirector*>(argument);
    instance->StartThread();
    return 0;
  }

protected:
  int id_;
  bool run_;
  CRITICAL_SECTION critical_section_;
  char *buffer_;
  std::string name;
  std::string text_;

public:
  HANDLE data_available_;

public:
  IORedirector() {
    buffer_ = new char(IOR_BUFFER_SIZE);
    data_available_ = CreateEvent(0, true, false, 0);
    InitializeCriticalSectionAndSpinCount(&critical_section_, 0x00000400);
  }

protected:

  void StartThread() {

    char *buffer = new char[IOR_BUFFER_SIZE];
    DWORD bytes_read;

    HANDLE pipe_handle = CreateFileA(name.c_str(), GENERIC_READ, 0,
      0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    while (run_) {
      bytes_read = 0;
      ReadFile(pipe_handle, buffer, IOR_BUFFER_SIZE, &bytes_read, 0);
      if (bytes_read > 0) {
        EnterCriticalSection(&critical_section_);
        
        // FIXME: we should impose some sort of reasonable limit on this buffer

        text_.append(buffer, bytes_read);

        LeaveCriticalSection(&critical_section_);

        SetEvent(data_available_);
      }

    }

    delete[] buffer;

  }

public:

  void GetData(std::string &target) {

    // we should move the string to pass it out and then have a 
    // new, empty buffer on this side. maybe? FIXME

    EnterCriticalSection(&critical_section_);
    target = text_;
    text_ = "";
    LeaveCriticalSection(&critical_section_);

  }

  void Stop() {
    run_ = false;
  }
  
  void Start(FILE *file) {

    std::stringstream sync_pipe_name;
    sync_pipe_name << "\\\\.\\pipe\\iop." << _getpid() << "." << _fileno(file);

    run_ = true;
    name = sync_pipe_name.str();

    HANDLE pipe_handle = CreateNamedPipeA(name.c_str(), 
      PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE | PIPE_WAIT,
      4, 0, IOR_BUFFER_SIZE, 1000, NULL);

    int stream = _open_osfhandle((intptr_t)pipe_handle, 0);
    FILE *write_file = _fdopen(stream, "wt");
    _dup2(_fileno(write_file), _fileno(file));
    setvbuf(file, NULL, _IONBF, 0);

    _beginthreadex(0, 0, IORedirector::ThreadFunc, static_cast<void*>(this), 0, 0);

    ConnectNamedPipe(pipe_handle, 0);

    DWORD mode;

    DWORD x = FILE_TYPE_PIPE;

    std::cerr << "GFT for pipe handle: " << GetFileType(pipe_handle) << std::endl;

    BOOL gcm = GetConsoleMode(pipe_handle, &mode);
    std::cerr << "GCM for pipe handle returned " << (gcm ? "TRUE" : "FALSE") << ", mode is " << mode << std::endl;

  }

};

#endif // #ifndef __IO_REDIRECTOR_H
