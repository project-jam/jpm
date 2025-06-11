#include "commands/js_command.h"
#include "jpm_config.h"

#ifdef USE_JAVASCRIPTCORE
#include <JavaScriptCore/JavaScript.h>
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

namespace jpm {

JSCommand::JSCommand() {
#ifdef USE_JAVASCRIPTCORE
    if (g_verbose_output) {
        std::cout << "JSCommand initialized." << std::endl;
    }
#else
    if (g_verbose_output) {
        std::cout << "JSCommand: built without JavaScriptCore support." << std::endl;
    }
#endif
}

void JSCommand::execute(const std::vector<std::string>& args) {
#ifdef USE_JAVASCRIPTCORE
    if (args.empty()) {
        std::cerr << "No JavaScript file specified." << std::endl;
        return;
    }
    execute_js_file(args[0]);
#else
    std::cerr << "Error: JavaScriptCore support is not enabled in this build." << std::endl;
#endif
}

#ifdef USE_JAVASCRIPTCORE
// Only compiled when USE_JAVASCRIPTCORE is defined
void JSCommand::execute_js_file(const std::string& file_path) {
    // Read the file into a string
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << file_path << std::endl;
        return;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string js_code = buffer.str();
    if (js_code.empty()) {
        std::cerr << "Warning: JavaScript file is empty: " << file_path << std::endl;
        return;
    }

    // Create JavaScript context
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    if (!ctx) {
        std::cerr << "Failed to create JavaScript context" << std::endl;
        return;
    }

    // Implement console.log
    JSObjectRef globalObject = JSContextGetGlobalObject(ctx);
    JSObjectRef consoleObj = JSObjectMake(ctx, nullptr, nullptr);
    JSStringRef logName = JSStringCreateWithUTF8CString("log");
    JSObjectRef logFunction = JSObjectMakeFunctionWithCallback(ctx, logName,
        [](JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
           size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
            for (size_t i = 0; i < argumentCount; ++i) {
                JSStringRef strRef = JSValueToStringCopy(ctx, arguments[i], nullptr);
                size_t maxSize = JSStringGetMaximumUTF8CStringSize(strRef);
                std::string s;
                s.resize(maxSize);
                JSStringGetUTF8CString(strRef, s.data(), maxSize);
                // Trim trailing nulls:
                size_t len = strlen(s.c_str());
                std::cout << s.substr(0, len);
                if (i + 1 < argumentCount) std::cout << " ";
                JSStringRelease(strRef);
            }
            std::cout << std::endl;
            return JSValueMakeUndefined(ctx);
        });
    JSObjectSetProperty(ctx, consoleObj, logName, logFunction, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(logName);
    JSStringRef consoleName = JSStringCreateWithUTF8CString("console");
    JSObjectSetProperty(ctx, globalObject, consoleName, consoleObj, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(consoleName);

    // Evaluate the script
    JSStringRef script = JSStringCreateWithUTF8CString(js_code.c_str());
    JSValueRef exception = nullptr;
    JSValueRef result = JSEvaluateScript(ctx, script, nullptr, nullptr, 1, &exception);
    if (exception) {
        JSStringRef exceptionString = JSValueToStringCopy(ctx, exception, nullptr);
        size_t maxSize = JSStringGetMaximumUTF8CStringSize(exceptionString);
        std::string buf;
        buf.resize(maxSize);
        JSStringGetUTF8CString(exceptionString, buf.data(), maxSize);
        size_t len = strlen(buf.c_str());
        std::cerr << "JavaScript Error: " << buf.substr(0, len) << std::endl;
        JSStringRelease(exceptionString);
    } else {
        // Optionally, handle `result` if needed
    }
    JSStringRelease(script);
    JSGlobalContextRelease(ctx);
}
#endif // USE_JAVASCRIPTCORE

} // namespace jpm
