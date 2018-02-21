
#include "windows_api_functions.h"

extern HMODULE global_module_handle;

std::string APIFunctions::ModulePath() {

  char path[MAX_PATH];
  GetModuleFileNameA(global_module_handle, path, sizeof(path));
  PathRemoveFileSpecA(path); // deprecated, but the replacement is windows 8+ only

  std::string module_path(path);
  module_path.append("\\");
  return module_path;

}

std::vector<std::pair<std::string, FILETIME>> APIFunctions::ListDirectory(const std::string &directory){
  
  char path[MAX_PATH];
  WIN32_FIND_DATAA find_data_info;

  std::vector<std::pair<std::string, FILETIME>> directory_entries;

  strcpy_s(path, directory.c_str());
  strcat_s(path, "\\*");

  HANDLE find_handle = FindFirstFileA(path, &find_data_info);
  if (find_handle && find_handle != INVALID_HANDLE_VALUE) {
    do {
      if (!(find_data_info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE))) {
        std::string match = directory;
        match += "\\";
        match += find_data_info.cFileName;
        directory_entries.push_back({ match, find_data_info.ftLastWriteTime });
       }
    } while (FindNextFileA(find_handle, &find_data_info));
  }

  return directory_entries;
}

std::string APIFunctions::ReadResource(LPTSTR resource_id) {

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

bool APIFunctions::GetRegistryString(std::string &result_value, const char *name, const char *key, HKEY base_key) {

  char buffer[MAX_PATH];
  DWORD data_size = MAX_PATH;

  if (!base_key) base_key = DEFAULT_BASE_KEY;
  if (!key) key = DEFAULT_REGISTRY_KEY;

  LSTATUS status = RegGetValueA(base_key, key, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, 0, buffer, &data_size);

  // don't add the terminating null to the string, or it will cause problems 
  // if we want to concatenate (which we do)

  if (!status && data_size > 0) result_value.assign(buffer, data_size - 1);

  return (status == 0);
}

bool APIFunctions::GetRegistryDWORD(DWORD &result_value, const char *name, const char *key, HKEY base_key) {

  DWORD data_size = sizeof(DWORD);

  if (!base_key) base_key = DEFAULT_BASE_KEY;
  if (!key) key = DEFAULT_REGISTRY_KEY;

  return(0 == RegGetValueA(base_key, key, name, RRF_RT_DWORD, 0, &result_value, &data_size));
}

std::string APIFunctions::GetPath() {

  // we do this infrequently enough that it's not worth having a persistent 
  // buffer, just allocate

  int length = ::GetEnvironmentVariableA("PATH", 0, 0);
  char *buffer = new char[length + 1];
  if (length > 0) ::GetEnvironmentVariableA("PATH", buffer, length);
  buffer[length] = 0; // not necessary
  std::string path(buffer, length);
  delete[] buffer;

  return path;
}

std::string APIFunctions::AppendPath(const std::string &new_path) {
  std::string path = GetPath();
  path += PATH_PATH_SEPARATOR;
  path += new_path;
  SetPath(path);
  return path;
}

std::string APIFunctions::PrependPath(const std::string &new_path) {
  std::string old_path = GetPath();
  std::string path = new_path;
  path += PATH_PATH_SEPARATOR;
  path += old_path;
  SetPath(path);
  return path;
}

/** sets path (for uncaching) */
void APIFunctions::SetPath(const std::string &path) {
  ::SetEnvironmentVariableA("PATH", path.c_str());
}


