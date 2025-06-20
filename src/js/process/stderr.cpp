#include "js/process/stderr.h"

#ifdef USE_JAVASCRIPTCORE
#include "jpm_config.h"
#include <iostream>
#include <cstring>

namespace jpm {
namespace js {
namespace process {

void setup_stderr(JSContextRef ctx, JSObjectRef process_obj) {
    // Create process.stderr object
    JSObjectRef stderr_obj = JSObjectMake(ctx, nullptr, nullptr);

    // Create stderr.write function
    JSStringRef write_function_name = JSStringCreateWithUTF8CString("write");
    JSObjectRef write_function = JSObjectMakeFunctionWithCallback(ctx, write_function_name,
        [](JSContextRef ctx_inner, JSObjectRef function, JSObjectRef thisObject,
           size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
            if (argumentCount > 0) {
                JSStringRef str_ref = JSValueToStringCopy(ctx_inner, arguments[0], nullptr);
                size_t max_size = JSStringGetMaximumUTF8CStringSize(str_ref);
                std::string output;
                output.resize(max_size);
                JSStringGetUTF8CString(str_ref, output.data(), max_size);
                size_t len = strlen(output.c_str());
                std::cerr << output.substr(0, len) << std::flush;
                JSStringRelease(str_ref);

                if (g_verbose_output) {
                    std::cout << std::endl << "[verbose] process.stderr.write called with length: " 
                              << len << std::endl;
                }
            }
            return JSValueMakeUndefined(ctx_inner);
        });

    JSObjectSetProperty(ctx, stderr_obj, write_function_name, write_function, 
                       kJSPropertyAttributeNone, nullptr);
    JSStringRelease(write_function_name);

    // Set stderr object on process
    JSStringRef stderr_name = JSStringCreateWithUTF8CString("stderr");
    JSObjectSetProperty(ctx, process_obj, stderr_name, stderr_obj, 
                       kJSPropertyAttributeNone, nullptr);
    JSStringRelease(stderr_name);

    if (g_verbose_output) {
        std::cout << "Setup process.stderr object and write function" << std::endl;
    }
}

} // namespace process
} // namespace js
} // namespace jpm

#endif // USE_JAVASCRIPTCORE