#include "js/process/argv.h"

#ifdef USE_JAVASCRIPTCORE
#include "jpm_config.h"
#include <iostream>

namespace jpm {
namespace js {
namespace process {

void setup_argv(JSContextRef ctx, JSObjectRef process_obj, const std::string& script_path) {
    // Create process.argv = ["node", "<script_path>"]
    JSValueRef argv_values[2];
    JSStringRef node_str = JSStringCreateWithUTF8CString("node");
    JSStringRef file_str = JSStringCreateWithUTF8CString(script_path.c_str());
    argv_values[0] = JSValueMakeString(ctx, node_str);
    argv_values[1] = JSValueMakeString(ctx, file_str);
    JSObjectRef argv_array = JSObjectMakeArray(ctx, 2, argv_values, nullptr);
    JSStringRelease(node_str);
    JSStringRelease(file_str);

    JSStringRef argv_name = JSStringCreateWithUTF8CString("argv");
    JSObjectSetProperty(ctx, process_obj, argv_name, argv_array, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(argv_name);

    if (g_verbose_output) {
        std::cout << "Setup process.argv with script path: " << script_path << std::endl;
    }
}

} // namespace process
} // namespace js
} // namespace jpm

#endif // USE_JAVASCRIPTCORE