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
 
#include "windows_api_functions.h"

namespace APIFunctions {

  std::vector<std::pair<std::string, FILETIME>> APIFunctions::ListDirectory(const std::string &directory) {

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

  FileError APIFunctions::FileContents(std::string &contents, const std::string &path) {

    HANDLE file_handle = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (!file_handle || file_handle == INVALID_HANDLE_VALUE) {
      return FileError::FileNotFound;
    }
    else {

      FileError result = FileError::Success;
      DWORD bytes_read = 0;
      DWORD buffer_size = 8 * 1024;
      char *buffer = new char[buffer_size];

      contents = "";

      // if this returns false, that's an error, it returns true on EOF
      
      while (true) {
        if(ReadFile(file_handle, buffer, buffer_size, &bytes_read, 0)){
          if(bytes_read) contents.append(buffer, bytes_read);
          else break;
        }
        else {
          result = FileError::FileReadError;
          break;
        }
      }

      CloseHandle(file_handle);
      delete[] buffer;

      return result;
    }
  }

}


