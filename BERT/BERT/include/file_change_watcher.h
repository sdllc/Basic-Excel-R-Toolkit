#pragma once

#include <vector>
#include <string>

// the loop timeout also functions as a debounce timeout,
// because we receive duplicate notifications on file changes.
// we can use separate timeouts for debouncing...

#define FILE_WATCH_LOOP_DEBOUNCE_TIMEOUT 200
#define FILE_WATCH_LOOP_NORMAL_TIMEOUT 500

#define FILE_WATCH_EVENT_MASK (FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_FILE_NAME)

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

private:

  /** thread start routine */
  static uint32_t __stdcall StartThread(void *data);

  /** instance thread routine */
  uint32_t InstanceStartThread();

  /** control flag */
  bool running_;

public:
  FileChangeWatcher();
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
