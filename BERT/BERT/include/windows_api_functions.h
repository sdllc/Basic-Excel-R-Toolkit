#pragma once

#include <Windows.h>
#include <string>

#define DEFAULT_BASE_KEY        HKEY_CURRENT_USER
#define DEFAULT_REGISTRY_KEY    "Software\\BERT"

#define PATH_PATH_SEPARATOR ";"

class APIFunctions {
public:

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

};
