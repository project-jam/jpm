#include "js/process/env.h"

#ifdef USE_JAVASCRIPTCORE
#include "jpm_config.h"
#include <iostream>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

namespace jpm {
namespace js {
namespace process {

// Helper function to get environment variable value
static std::string get_env_var(const char* name) {
    const char* value = std::getenv(name);
    return value ? value : "";
}

// Helper function to set environment variables
bool set_env(JSContextRef ctx, JSObjectRef env_obj, const char* name, const char* value) {
    JSStringRef name_str = JSStringCreateWithUTF8CString(name);
    JSStringRef value_str = JSStringCreateWithUTF8CString(value);
    JSValueRef value_ref = JSValueMakeString(ctx, value_str);
    
    JSObjectSetProperty(ctx, env_obj, name_str, value_ref, 
                       kJSPropertyAttributeNone, nullptr);
    
    JSStringRelease(name_str);
    JSStringRelease(value_str);
    return true;
}

void setup_env(JSContextRef ctx, JSObjectRef process_obj) {
    // Create process.env object
    JSObjectRef env_obj = JSObjectMake(ctx, nullptr, nullptr);

    // Common environment variables
    const char* common_vars[] = {
        "PATH", "HOME", "USER", "LANG", "SHELL", "PWD",
        "TEMP", "TMP", "HOSTNAME", "TERM"
    };

    for (const char* var : common_vars) {
        std::string value = get_env_var(var);
        if (!value.empty()) {
            set_env(ctx, env_obj, var, value.c_str());
        }
    }

    // Platform-specific environment handling
    #ifdef _WIN32
    // Windows-specific environment variables
    const char* win_vars[] = {
        "USERPROFILE", "APPDATA", "LOCALAPPDATA", "COMPUTERNAME",
        "OS", "PROCESSOR_ARCHITECTURE", "SystemRoot", "windir"
    };
    for (const char* var : win_vars) {
        std::string value = get_env_var(var);
        if (!value.empty()) {
            set_env(ctx, env_obj, var, value.c_str());
        }
    }
    #else
    // Unix-specific environment variables
    const char* unix_vars[] = {
        "DISPLAY", "XAUTHORITY", "SSH_AUTH_SOCK", "XDG_SESSION_TYPE",
        "XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS"
    };
    for (const char* var : unix_vars) {
        std::string value = get_env_var(var);
        if (!value.empty()) {
            set_env(ctx, env_obj, var, value.c_str());
        }
    }
    #endif

    // Add custom environment variables
    set_env(ctx, env_obj, "JPM_VERSION", PROJECT_VERSION);
    set_env(ctx, env_obj, "JPM_PLATFORM", 
        #ifdef _WIN32
            "win32"
        #elif defined(__APPLE__)
            "darwin"
        #else
            "linux"
        #endif
    );

    // Make process.env properties non-enumerable but configurable
    JSStringRef env_name = JSStringCreateWithUTF8CString("env");
    JSObjectSetProperty(ctx, process_obj, env_name, env_obj,
                       kJSPropertyAttributeDontEnum | kJSPropertyAttributeNone,
                       nullptr);
    JSStringRelease(env_name);

    if (g_verbose_output) {
        std::cout << "Setup process.env with environment variables" << std::endl;
    }
}

} // namespace process
} // namespace js
} // namespace jpm

#endif // USE_JAVASCRIPTCORE