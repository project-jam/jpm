#include "js/process/stdin.h"

#ifdef USE_JAVASCRIPTCORE
#include "jpm_config.h"
#include <iostream>
#include <string>
#include <cstring>

namespace jpm {
namespace js {
namespace process {

void setup_stdin(JSContextRef ctx, JSObjectRef process_obj) {
    // Create process.stdin object
    JSObjectRef stdin_obj = JSObjectMake(ctx, nullptr, nullptr);

    // Create stdin.on function for event handling
    JSStringRef on_function_name = JSStringCreateWithUTF8CString("on");
    JSObjectRef on_function = JSObjectMakeFunctionWithCallback(ctx, on_function_name,
        [](JSContextRef ctx_inner, JSObjectRef function, JSObjectRef thisObject,
           size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
            // Expect: arguments[0] = event name (string), arguments[1] = callback (function)
            if (argumentCount >= 2 && JSValueIsString(ctx_inner, arguments[0]) && 
                JSValueIsObject(ctx_inner, arguments[1])) {
                
                // Get the event name
                JSStringRef event_str_ref = JSValueToStringCopy(ctx_inner, arguments[0], nullptr);
                size_t max_size = JSStringGetMaximumUTF8CStringSize(event_str_ref);
                std::string event_name;
                event_name.resize(max_size);
                JSStringGetUTF8CString(event_str_ref, event_name.data(), max_size);
                size_t len = strlen(event_name.c_str());
                event_name = event_name.substr(0, len);
                JSStringRelease(event_str_ref);

                if (event_name == "data") {
                    if (g_verbose_output) {
                        std::cout << "process.stdin.on('data') registered" << std::endl;
                    }

                    // Read one line from stdin synchronously
                    std::string line;
                    if (!std::getline(std::cin, line)) {
                        line.clear();
                        if (g_verbose_output) {
                            std::cout << "Failed to read from stdin" << std::endl;
                        }
                    }

                    // Convert the line to a JS string and call the callback
                    JSStringRef js_line = JSStringCreateWithUTF8CString(line.c_str());
                    JSValueRef js_value = JSValueMakeString(ctx_inner, js_line);
                    JSStringRelease(js_line);

                    JSObjectRef callback = (JSObjectRef)arguments[1];
                    JSValueRef callback_exception = nullptr;
                    JSObjectCallAsFunction(ctx_inner, callback, nullptr, 1, &js_value, &callback_exception);

                    if (callback_exception) {
                        JSStringRef exc_str = JSValueToStringCopy(ctx_inner, callback_exception, nullptr);
                        size_t exc_size = JSStringGetMaximumUTF8CStringSize(exc_str);
                        std::string error_msg;
                        error_msg.resize(exc_size);
                        JSStringGetUTF8CString(exc_str, error_msg.data(), exc_size);
                        size_t exc_len = strlen(error_msg.c_str());
                        std::cerr << "Error in process.stdin.on callback: " 
                                 << error_msg.substr(0, exc_len) << std::endl;
                        JSStringRelease(exc_str);
                    }
                } else if (g_verbose_output) {
                    std::cout << "Ignoring unsupported stdin event: " << event_name << std::endl;
                }
            }
            return JSValueMakeUndefined(ctx_inner);
        });

    JSObjectSetProperty(ctx, stdin_obj, on_function_name, on_function, 
                       kJSPropertyAttributeNone, nullptr);
    JSStringRelease(on_function_name);

    // Set stdin object on process
    JSStringRef stdin_name = JSStringCreateWithUTF8CString("stdin");
    JSObjectSetProperty(ctx, process_obj, stdin_name, stdin_obj, 
                       kJSPropertyAttributeNone, nullptr);
    JSStringRelease(stdin_name);

    if (g_verbose_output) {
        std::cout << "Setup process.stdin object and on() function" << std::endl;
    }
}

} // namespace process
} // namespace js
} // namespace jpm

#endif // USE_JAVASCRIPTCORE