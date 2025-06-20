#include "js/process/stdout.h"

#ifdef USE_JAVASCRIPTCORE
#include "jpm_config.h"
#include <iostream>
#include <cstring>

namespace jpm {
namespace js {
namespace process {

void setup_stdout(JSContextRef ctx, JSObjectRef process_obj) {
    // Create process.stdout object
    JSObjectRef stdout_obj = JSObjectMake(ctx, nullptr, nullptr);

    // Create stdout.write function
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
                std::cout << output.substr(0, len) << std::flush;
                JSStringRelease(str_ref);

                if (g_verbose_output) {
                    std::cout << std::endl << "[verbose] process.stdout.write called with length: " 
                              << len << std::endl;
                }
            }
            return JSValueMakeUndefined(ctx_inner);
        });

    JSObjectSetProperty(ctx, stdout_obj, write_function_name, write_function, 
                       kJSPropertyAttributeNone, nullptr);
    JSStringRelease(write_function_name);

    // Set stdout object on process
    JSStringRef stdout_name = JSStringCreateWithUTF8CString("stdout");
    JSObjectSetProperty(ctx, process_obj, stdout_name, stdout_obj, 
                       kJSPropertyAttributeNone, nullptr);
    JSStringRelease(stdout_name);

    if (g_verbose_output) {
        std::cout << "Setup process.stdout object and write function" << std::endl;
    }
}

} // namespace process
} // namespace js
} // namespace jpm

#endif // USE_JAVASCRIPTCORE