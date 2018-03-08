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
 
#pragma once

#include <deque>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#include <SDKDDKVer.h>
#include <windows.h>
#include <process.h>

/**
 * FIXME:
 *
 * (1) consolidate pipe with controlR; they're diverging during development
 *     (which is OK), but unify.
 *
 * (2) if this becomes cross-platform we probably have to do more packet management.
 *     we can merge in the PB framing we are already doing (but maybe add
 *     signifier/magic header?)
 */

#define DEFAULT_BUFFER_SIZE (8 * 1024)

 // we really only need 2 connections, except for dev/debug
#define MAX_PIPE_COUNT  4

class Pipe {

private:
  HANDLE handle_;
  OVERLAPPED read_io_;
  OVERLAPPED write_io_;
  DWORD buffer_size_;
  std::string name_;

  /** read buffer is a single read, limited to pipe buffer size */
  char *read_buffer_;

  /**
   * message buffer is for messages that exceed the read buffer size, they're
   * constructed over multiple reads
   */
  std::string message_buffer_;

  std::deque<std::string> write_stack_;

  bool connected_;
  bool reading_;
  bool writing_;
  bool error_;

public:

  /** accessor */
  bool connected() { return connected_; }

  /** accessor */
  bool reading() { return reading_; }

  /** accessor */
  bool writing() { return writing_; }

  /** accessor */
  bool error() { return error_; }

  /** create pipe, accept connection and optionally block */
  DWORD Start(std::string name, bool wait);

  /** we have a notification about connection, do any housekeeping */
  void Connect(bool start_read = true);

  //DWORD BlockingRead(std::string &buf);


  DWORD Read(std::string &buffer, bool block = false);

  void PushWrite(const std::string &message);
  void QueueWrites(std::vector<std::string> &list);

  /**
   * returns non-zero if data was written (without considering whether
   * more data is avaialable); returns 0 if either no write took place,
   * or a write was started but not completed.
   */
  int NextWrite();

  int StartRead();

  void ClearError();

  DWORD Reset();

  std::string full_name();

  /** accessor */
  HANDLE wait_handle_read();

  /** accessor */
  HANDLE wait_handle_write();

  /** accessor */
  DWORD buffer_size();

  /** accessor */
  HANDLE pipe_handle();

public:
  Pipe();
  ~Pipe();

};



