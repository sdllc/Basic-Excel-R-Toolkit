#pragma once

#include <Windows.h>
#include <Shlwapi.h>

#include <string>
#include <vector>

#define DEFAULT_BASE_KEY        HKEY_CURRENT_USER
#define DEFAULT_REGISTRY_KEY    "Software\\BERT"

#define PATH_PATH_SEPARATOR ";"

class APIFunctions {
public:

  /** 
   * list directory. returns a tuple of filename, last write. lists files only, 
   * not directories. the return value is the _full_path_, including the directory.
   *
   * TODO: options?
   */
  static std::vector<std::pair<std::string, FILETIME>> ListDirectory(const std::string &directory);

  /** reads resource in this dll */
  static std::string ReadResource(LPTSTR resource_id);

  /** reads registry string */
  static bool GetRegistryString(std::string &result_value, const char *name, const char *key = 0, HKEY base_key = 0);

  /** reads registry dword */
  static bool GetRegistryDWORD(DWORD &result_value, const char *name, const char *key = 0, HKEY base_key = 0);

  /** gets path. we are caching before modifying */
  static std::string GetPath();

  /** appends given string to path. returns new path. */
  static std::string AppendPath(const std::string &new_path);

  /** prepends given string to path. returns new path. */
  static std::string PrependPath(const std::string &new_path);

  /** sets path (for uncaching) */
  static void SetPath(const std::string &path);

  /** get the path of the current module (not containing process) */
  static std::string ModulePath();

};
