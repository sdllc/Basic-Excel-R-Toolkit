
#include "windows_api_functions.h"

extern HMODULE global_module_handle;

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


