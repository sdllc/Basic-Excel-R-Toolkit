#pragma once

#include <Windows.h>
#include <string>

#define DEFAULT_BASE_KEY        HKEY_CURRENT_USER
#define DEFAULT_REGISTRY_KEY    "Software\\BERT"

class APIFunctions {
public:

    /** read resource in this dll */
    static std::string ReadResource(LPTSTR resource_id);

    /** read registry string */
    static bool GetRegistryString(std::string &result_value, const char *name, const char *key = 0, HKEY base_key = 0);

    /** read registry dword */
    static bool GetRegistryDWORD(DWORD &result_value, const char *name, const char *key = 0, HKEY base_key = 0);

};
