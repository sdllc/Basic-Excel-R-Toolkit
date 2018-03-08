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

#include <vector>
#include <string>

// the loop timeout also functions as a debounce timeout,
// because we receive duplicate notifications on file changes.
// we can use separate timeouts for debouncing...

#define FILE_WATCH_LOOP_DEBOUNCE_TIMEOUT 150
#define FILE_WATCH_LOOP_NORMAL_TIMEOUT 500

#define FILE_WATCH_EVENT_MASK (FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_FILE_NAME)

typedef void (*FileWatchCallback)(void*, const std::vector<std::string>&);

/**
 * utility for watching directory changes (and implicitly file changes).
 * we need to run this on a separate thread, and call back to BERT via
 * COM to get on the correct thread.
 */
class FileChangeWatcher {

private:
  std::vector < std::string > watched_directories_;
  HANDLE update_watch_list_handle_;
  CRITICAL_SECTION critical_section_;

  void *callback_argument_;
  FileWatchCallback callback_function_;

private:

  /** thread start routine */
  static uint32_t __stdcall StartThread(void *data);

  void NotifyDirectoryChanges(const std::vector<std::string> &directory_list, FILETIME update_time);

  /** instance thread routine */
  uint32_t InstanceStartThread();

  /** control flag */
  bool running_;

public:
  FileChangeWatcher(FileWatchCallback callback = 0, void *argument = 0);
  ~FileChangeWatcher();

public:
  /** add a directory to the watch list */
  void WatchDirectory(const std::string &directory);

  /** remove a watched directory */
  void UnwatchDirectory(const std::string &directory);

  /** start watch thread */
  void StartWatch();

  /** clean up */
  void Shutdown();

};
