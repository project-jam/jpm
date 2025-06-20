#include "js/process/exit.h"

#ifdef USE_JAVASCRIPTCORE
#include "jpm_config.h"
#include <iostream>
#include <cstdlib>

namespace jpm {
namespace js {
namespace process {

void setup_exit(JSContextRef ctx, JSObjectRef process_obj) {
    // Create process.exit function
    JSStringRef exit_function_name = JSStringCreateWithUTF8CString("exit");
    JSObjectRef exit_function = JSObjectMakeFunctionWithCallback(ctx, exit_function_name,
        [](JSContextRef ctx_inner, JSObjectRef function, JSObjectRef thisObject,
           size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
            int exit_code = 0;
            if (argumentCount > 0 && JSValueIsNumber(ctx_inner, arguments[0])) {
                exit_code = (int)JSValueToNumber(ctx_inner, arguments[0], nullptr);
            }

            if (g_verbose_output) {
                std::cout << "process.exit called with code: " << exit_code << std::endl;
            }

            exit(exit_code);
            return JSValueMakeUndefined(ctx_inner); // Never reached due to exit()
        });

    JSObjectSetProperty(ctx, process_obj, exit_function_name, exit_function, 
                       kJSPropertyAttributeNone, nullptr);
    JSStringRelease(exit_function_name);

    if (g_verbose_output) {
        std::cout << "Setup process.exit function" << std::endl;
    }
}

} // namespace process
} // namespace js
} // namespace jpm

#endif // USE_JAVASCRIPTCORE