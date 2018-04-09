/**
 * Copyright (c) 2017-2018 Structured Data, LLC
 * 
 * This file is part of BERT.
 *
 * BERT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BERT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with BERT.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#include "pipe.h"

Pipe::Pipe()
  : buffer_size_(DEFAULT_BUFFER_SIZE)
  , read_buffer_(0)
  , reading_(false)
  , writing_(false)
  , connected_(false)
{
}

Pipe::~Pipe() {
  if (read_buffer_) delete read_buffer_;
}

HANDLE Pipe::pipe_handle() { return handle_; }

HANDLE Pipe::wait_handle_read() { return read_io_.hEvent; }

HANDLE Pipe::wait_handle_write() { return write_io_.hEvent; }

DWORD Pipe::buffer_size() { return buffer_size_; }

void Pipe::QueueWrites(std::vector<std::string> &list) {
  for (auto entry : list) {
    write_stack_.push_back(entry);
  }
}

void Pipe::PushWrite(const std::string &message) {
  write_stack_.push_back(message);
  NextWrite();
}

int Pipe::StartRead() {
  if (reading_ || error_ || !connected_) return 0;
  reading_ = true;
  ResetEvent(read_io_.hEvent);
  ReadFile(handle_, read_buffer_, buffer_size_, 0, &read_io_);
  return 0;
}

void Pipe::ClearError() {
  error_ = false;
}

void Pipe::Connect(bool start_read) {
  connected_ = true;
  std::cout << "pipe connected (" << name_ << ")" << std::endl;
  if (start_read) StartRead();
}

DWORD Pipe::Reset() {

  CancelIo(handle_);
  ResetEvent(read_io_.hEvent);
  ResetEvent(write_io_.hEvent);
  DisconnectNamedPipe(handle_);

  connected_ = reading_ = writing_ = error_ = false;
  message_buffer_.clear();

  if (ConnectNamedPipe(handle_, &read_io_)) {
    std::cerr << "ERR in connectNamedPipe" << std::endl;
  }
  else {
    switch (GetLastError()) {
    case ERROR_PIPE_CONNECTED:
      SetEvent(read_io_.hEvent);
      break;
    case ERROR_IO_PENDING:
      break;
    default:
      std::cerr << "connect failed with " << GetLastError() << std::endl;
      return GetLastError();
      break;
    }
  }

  return 0;
}

int Pipe::NextWrite() {

  DWORD bytes, result;

  if (write_stack_.size() == 0) {
    ResetEvent(write_io_.hEvent);
    writing_ = false;
    return 0; // no write
  }
  else if (writing_) {

    // if we are currently in a write operation, 
    // check if it's complete.

//    result = GetOverlappedResultEx(handle_, &write_io_, &bytes, 0, FALSE);
    result = GetOverlappedResult(handle_, &write_io_, &bytes, FALSE);
    if (!result) {
      DWORD err = GetLastError();
      switch (err) {
      case WAIT_TIMEOUT:
      case WAIT_IO_COMPLETION:
      case ERROR_IO_INCOMPLETE:
        return 0; // write in progress
      default:
        error_ = true;
        std::cout << "Pipe err " << err << std::endl;
        return 0; // err (need a flag here)
      }
    }
  }

  // at this point we're safe to write. we can push more than
  // one write if they complete immediately

  while (write_stack_.size()) {

    ResetEvent(write_io_.hEvent);
    std::string message = write_stack_.front();
    write_stack_.pop_front();
    WriteFile(handle_, message.c_str(), (DWORD)message.length(), NULL, &write_io_);
    //result = GetOverlappedResultEx(handle_, &write_io_, &bytes, 0, FALSE);
    result = GetOverlappedResult(handle_, &write_io_, &bytes, FALSE);
    if (result) {
      // std::cout << "immediate write success (" << bytes << ")" << std::endl;
      ResetEvent(write_io_.hEvent);
      writing_ = false;
    }
    else {
      // std::cout << "write pending" << std::endl;
      writing_ = true;
      return 0;
    }
  }

  return 1;
}

DWORD Pipe::Read(std::string &buf, bool block) {

  // FIXME: what happens in message mode when the buffer is too small? 
  //
  // Docs:
  // In this situation, GetLastError returns ERROR_MORE_DATA, and the client can read the 
  // remainder of the message using additional calls to ReadFile.
  // 

  // that implies we will need to hold on to the buffer.

  DWORD bytes = 0;
//  DWORD success = GetOverlappedResultEx(handle_, &read_io_, &bytes, block ? INFINITE : 0, FALSE);
  DWORD success = GetOverlappedResult(handle_, &read_io_, &bytes, block ? TRUE : FALSE);

  if (success) {
    if (message_buffer_.length()) {
      // std::cout << "appending long mesage (" << bytes << "), this is the end" << std::endl;
      buf = message_buffer_;
      buf.append(read_buffer_, bytes);
      message_buffer_.clear();
    }
    else buf.assign(read_buffer_, bytes);
    reading_ = false;
    return 0;
  }
  else {
    DWORD err = GetLastError();
    if (err == ERROR_MORE_DATA) {
      // std::cout << "appending long mesage (" << bytes << "), this is a chunk" << std::endl;
      message_buffer_.append(read_buffer_, bytes);
      reading_ = false;
      StartRead();
      return err;
    }
    if (err != WAIT_TIMEOUT) error_ = true;
    return err;
  }

}

std::string Pipe::full_name() {
  std::stringstream pipename;
  pipename << "\\\\.\\pipe\\" << name_;
  return pipename.str().c_str();
}

DWORD Pipe::Start(std::string name, bool wait) {

  name_ = name;

  handle_ = CreateNamedPipeA(full_name().c_str(),
    PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
    MAX_PIPE_COUNT,
    DEFAULT_BUFFER_SIZE,
    DEFAULT_BUFFER_SIZE,
    100,
    NULL);

  if (NULL == handle_ || handle_ == INVALID_HANDLE_VALUE) {
    std::cerr << "create pipe failed" << std::endl;
    return -1;
  }

  read_io_.hEvent = CreateEvent(0, TRUE, FALSE, 0);
  write_io_.hEvent = CreateEvent(0, TRUE, FALSE, 0);

  read_buffer_ = new char[buffer_size_];
  
  if (ConnectNamedPipe(handle_, &read_io_)) {
    std::cerr << "ERR in connectNamedPipe" << std::endl;
  }
  else {
    switch (GetLastError()) {
    case ERROR_PIPE_CONNECTED:
      SetEvent(read_io_.hEvent);
      break;
    case ERROR_IO_PENDING:
      break;
    default:
      std::cerr << "connect failed with " << GetLastError() << std::endl;
      return GetLastError();
      break;
    }
  }

  if (!wait) return 0;

  while (true) {

    DWORD bytes;
    //DWORD rslt = GetOverlappedResultEx(handle_, &read_io_, &bytes, 1000, FALSE);

    WaitForSingleObject(handle_, 1000);
    DWORD rslt = GetOverlappedResult(handle_, &read_io_, &bytes, FALSE);


    if (rslt) {
      std::cout << "connected (" << name << ")" << std::endl;
      return 0;
    }
    else {
      DWORD err = GetLastError();
      if (err != WAIT_TIMEOUT) {
        error_ = true;
        return err;
      }
    }

  }


  return -2;
}



