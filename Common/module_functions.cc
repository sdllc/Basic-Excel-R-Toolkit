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

