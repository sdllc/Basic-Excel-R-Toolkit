
#include "module_functions.h"

extern HMODULE global_module_handle;

namespace ModuleFunctions {

  std::string ModulePath() {

    char path[MAX_PATH];
    GetModuleFileNameA(global_module_handle, path, sizeof(path));
    PathRemoveFileSpecA(path); // deprecated, but the replacement is windows 8+ only

    std::string module_path(path);
    module_path.append("\\");
    return module_path;

  }
  
  std::string ReadResource(LPTSTR resource_id) {

    HRSRC resource_handle;
    HGLOBAL global_handle = 0;
    DWORD resource_size = 0;
    std::string resource_text;

    resource_handle = FindResource(global_module_handle, resource_id, RT_RCDATA);

    if (resource_handle) {
      resource_size = SizeofResource(global_module_handle, resource_handle);
      global_handle = LoadResource(global_module_handle, resource_handle);
    }

    if (global_handle && resource_size > 0) {
      const void *data = LockResource(global_handle);
      if (data) resource_text.assign(reinterpret_cast<const char*>(data), resource_size);
    }

    return resource_text;

  }

}

