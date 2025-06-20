#include "js/process/platform.h"

#ifdef USE_JAVASCRIPTCORE
#include "jpm_config.h"
#include <iostream>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <processthreadsapi.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#endif

namespace jpm {
namespace js {
namespace process {

namespace {
    // Store the program start time
    const auto program_start_time = std::chrono::steady_clock::now();

    // Helper function to create a string property
    void set_string_property(JSContextRef ctx, JSObjectRef obj, const char* name, const char* value) {
        JSStringRef name_str = JSStringCreateWithUTF8CString(name);
        JSStringRef value_str = JSStringCreateWithUTF8CString(value);
        JSValueRef value_ref = JSValueMakeString(ctx, value_str);
        JSObjectSetProperty(ctx, obj, name_str, value_ref, kJSPropertyAttributeNone, nullptr);
        JSStringRelease(name_str);
        JSStringRelease(value_str);
    }

    // Helper function to create a number property
    void set_number_property(JSContextRef ctx, JSObjectRef obj, const char* name, double value) {
        JSStringRef name_str = JSStringCreateWithUTF8CString(name);
        JSValueRef value_ref = JSValueMakeNumber(ctx, value);
        JSObjectSetProperty(ctx, obj, name_str, value_ref, kJSPropertyAttributeNone, nullptr);
        JSStringRelease(name_str);
    }

    // Get architecture string
    const char* get_arch() {
        #if defined(__x86_64__) || defined(_M_X64)
            return "x64";
        #elif defined(__aarch64__) || defined(_M_ARM64)
            return "arm64";
        #elif defined(__arm__) || defined(_M_ARM)
            return "arm";
        #elif defined(__i386__) || defined(_M_IX86)
            return "x86";
        #else
            return "unknown";
        #endif
    }
}

void setup_platform(JSContextRef ctx, JSObjectRef process_obj) {
    // Add memoryUsage() function
    JSStringRef memory_usage_name = JSStringCreateWithUTF8CString("memoryUsage");
    JSObjectRef memory_usage_func = JSObjectMakeFunctionWithCallback(ctx, memory_usage_name,
        [](JSContextRef ctx_inner, JSObjectRef function, JSObjectRef thisObject,
           size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
            JSObjectRef result = JSObjectMake(ctx_inner, nullptr, nullptr);

            #ifdef _WIN32
            PROCESS_MEMORY_COUNTERS_EX pmc;
            if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
                set_number_property(ctx_inner, result, "rss", static_cast<double>(pmc.WorkingSetSize));
                set_number_property(ctx_inner, result, "heapTotal", static_cast<double>(pmc.PrivateUsage));
                set_number_property(ctx_inner, result, "heapUsed", static_cast<double>(pmc.WorkingSetSize - pmc.PagefileUsage));
            }
            #else
            // On Unix-like systems, read from /proc/self/statm
            FILE* statm = fopen("/proc/self/statm", "r");
            if (statm) {
                unsigned long size, resident, shared, text, lib, data, dt;
                if (fscanf(statm, "%lu %lu %lu %lu %lu %lu %lu",
                          &size, &resident, &shared, &text, &lib, &data, &dt) == 7) {
                    long page_size = sysconf(_SC_PAGESIZE);
                    set_number_property(ctx_inner, result, "rss", static_cast<double>(resident * page_size));
                    set_number_property(ctx_inner, result, "heapTotal", static_cast<double>(size * page_size));
                    set_number_property(ctx_inner, result, "heapUsed", static_cast<double>(data * page_size));
                }
                fclose(statm);
            }
            #endif
            
            // External memory is harder to track accurately, set to 0 for now
            set_number_property(ctx_inner, result, "external", 0.0);

            return result;
        });
    JSObjectSetProperty(ctx, process_obj, memory_usage_name, memory_usage_func, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(memory_usage_name);

    // Set platform
    #ifdef _WIN32
        set_string_property(ctx, process_obj, "platform", "win32");
    #elif defined(__APPLE__)
        set_string_property(ctx, process_obj, "platform", "darwin");
    #else
        set_string_property(ctx, process_obj, "platform", "linux");
    #endif

    // Set architecture
    set_string_property(ctx, process_obj, "arch", get_arch());

    // Set version (using PROJECT_VERSION from CMake)
    set_string_property(ctx, process_obj, "version", PROJECT_VERSION);

    // Set versions object with dependency versions
    JSObjectRef versions_obj = JSObjectMake(ctx, nullptr, nullptr);
    set_string_property(ctx, versions_obj, "jpm", PROJECT_VERSION);
    #ifdef __clang__
        set_string_property(ctx, versions_obj, "clang", __clang_version__);
    #elif defined(__GNUC__)
        set_string_property(ctx, versions_obj, "gcc", __VERSION__);
    #endif
    JSStringRef versions_str = JSStringCreateWithUTF8CString("versions");
    JSObjectSetProperty(ctx, process_obj, versions_str, versions_obj, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(versions_str);

    // Set process title
    set_string_property(ctx, process_obj, "title", "jpm");

    // Set process ID
    #ifdef _WIN32
        set_number_property(ctx, process_obj, "pid", static_cast<double>(GetCurrentProcessId()));
        set_number_property(ctx, process_obj, "ppid", static_cast<double>(GetCurrentProcessId())); // Simplified for Windows
    #else
        set_number_property(ctx, process_obj, "pid", static_cast<double>(getpid()));
        set_number_property(ctx, process_obj, "ppid", static_cast<double>(getppid()));
    #endif

    // Add uptime() function
    JSStringRef uptime_name = JSStringCreateWithUTF8CString("uptime");
    JSObjectRef uptime_func = JSObjectMakeFunctionWithCallback(ctx, uptime_name,
        [](JSContextRef ctx_inner, JSObjectRef function, JSObjectRef thisObject,
           size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
            auto now = std::chrono::steady_clock::now();
            auto duration = now - program_start_time;
            double seconds = std::chrono::duration<double>(duration).count();
            return JSValueMakeNumber(ctx_inner, seconds);
        });
    JSObjectSetProperty(ctx, process_obj, uptime_name, uptime_func, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(uptime_name);

    // Add cwd() function
    JSStringRef cwd_name = JSStringCreateWithUTF8CString("cwd");
    JSObjectRef cwd_func = JSObjectMakeFunctionWithCallback(ctx, cwd_name,
        [](JSContextRef ctx_inner, JSObjectRef function, JSObjectRef thisObject,
           size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
            std::string cwd = std::filesystem::current_path().string();
            JSStringRef cwd_str = JSStringCreateWithUTF8CString(cwd.c_str());
            JSValueRef result = JSValueMakeString(ctx_inner, cwd_str);
            JSStringRelease(cwd_str);
            return result;
        });
    JSObjectSetProperty(ctx, process_obj, cwd_name, cwd_func, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(cwd_name);

    // Add chdir() function
    JSStringRef chdir_name = JSStringCreateWithUTF8CString("chdir");
    JSObjectRef chdir_func = JSObjectMakeFunctionWithCallback(ctx, chdir_name,
        [](JSContextRef ctx_inner, JSObjectRef function, JSObjectRef thisObject,
           size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
            if (argumentCount < 1 || !JSValueIsString(ctx_inner, arguments[0])) {
                return JSValueMakeBoolean(ctx_inner, false);
            }

            JSStringRef path_str = JSValueToStringCopy(ctx_inner, arguments[0], nullptr);
            size_t path_max_size = JSStringGetMaximumUTF8CStringSize(path_str);
            std::string path;
            path.resize(path_max_size);
            JSStringGetUTF8CString(path_str, path.data(), path_max_size);
            JSStringRelease(path_str);

            try {
                std::filesystem::current_path(path);
                return JSValueMakeBoolean(ctx_inner, true);
            } catch (const std::filesystem::filesystem_error& e) {
                if (exception) {
                    JSStringRef err_str = JSStringCreateWithUTF8CString(e.what());
                    *exception = JSValueMakeString(ctx_inner, err_str);
                    JSStringRelease(err_str);
                }
                return JSValueMakeBoolean(ctx_inner, false);
            }
        });
    JSObjectSetProperty(ctx, process_obj, chdir_name, chdir_func, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(chdir_name);

    if (g_verbose_output) {
        std::cout << "Setup process platform information and functions" << std::endl;
    }
}

} // namespace process
} // namespace js
} // namespace jpm

#endif // USE_JAVASCRIPTCORE