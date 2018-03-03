#pragma once

#include <Windows.h>
#include <Shlwapi.h>

#include <string>
#include <vector>

#define DEFAULT_BASE_KEY        HKEY_CURRENT_USER
#define DEFAULT_REGISTRY_KEY    "Software\\BERT2"

#define PATH_PATH_SEPARATOR ";"

namespace ModuleFunctions {

  /** reads resource in this dll */
  std::string ReadResource(LPTSTR resource_id);

  /** get the path of the current module (not containing process) */
  std::string ModulePath();

}
