#include "js/js.h"
#include "jpm_config.h"
#ifdef USE_JAVASCRIPTCORE
#include <JavaScriptCore/JavaScript.h>
#include "js/module.h"
#include "js/process/argv.h"
#include "js/process/exit.h"
#include "js/process/stdout.h"
#include "js/process/stderr.h"
#include "js/process/stdin.h"
#include "js/process/env.h"
#include "js/process/platform.h"
#include "js/process/events.h"
#endif
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>

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
void JSCommand::execute_js_file(const std::string& file_path) {
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

    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    if (!ctx) {
        std::cerr << "Failed to create JavaScript context" << std::endl;
        return;
    }

    JSObjectRef globalObject = JSContextGetGlobalObject(ctx);

    {
        JSObjectRef consoleObj = JSObjectMake(ctx, nullptr, nullptr);
        JSStringRef logName = JSStringCreateWithUTF8CString("log");
        JSObjectRef logFunction = JSObjectMakeFunctionWithCallback(ctx, logName,
            [](JSContextRef ctxInner, JSObjectRef, JSObjectRef,
               size_t argumentCount, const JSValueRef arguments[], JSValueRef*) -> JSValueRef {
                for (size_t i = 0; i < argumentCount; ++i) {
                    JSStringRef strRef = JSValueToStringCopy(ctxInner, arguments[i], nullptr);
                    size_t maxSize = JSStringGetMaximumUTF8CStringSize(strRef);
                    std::string s(maxSize, '\0');
                    JSStringGetUTF8CString(strRef, &s[0], maxSize);
                    s.resize(strlen(s.c_str()));
                    std::cout << s;
                    if (i + 1 < argumentCount) std::cout << " ";
                    JSStringRelease(strRef);
                }
                std::cout << std::endl;
                return JSValueMakeUndefined(ctxInner);
            });
        JSObjectSetProperty(ctx, consoleObj, logName, logFunction, kJSPropertyAttributeNone, nullptr);
        JSStringRelease(logName);
        JSStringRef consoleName = JSStringCreateWithUTF8CString("console");
        JSObjectSetProperty(ctx, globalObject, consoleName, consoleObj, kJSPropertyAttributeNone, nullptr);
        JSStringRelease(consoleName);
    }

    JSObjectRef processObj = JSObjectMake(ctx, nullptr, nullptr);

    js::process::setup_argv(ctx, processObj, file_path);
    js::process::setup_exit(ctx, processObj);
    js::process::setup_stdout(ctx, processObj);
    js::process::setup_stderr(ctx, processObj);
    js::process::setup_stdin(ctx, processObj);
    js::process::setup_env(ctx, processObj);
    js::process::setup_platform(ctx, processObj);
    js::process::setup_events(ctx, processObj);
    js::process::setup_hrtime(ctx, processObj);

    JSStringRef processName = JSStringCreateWithUTF8CString("process");
    JSObjectSetProperty(ctx, globalObject, processName, processObj, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(processName);

    // Initialize module system and register process as a built-in module
    js::setup_module_system(ctx, globalObject);

    // Create the global events object for process events
    JSObjectRef eventsObj = JSObjectMake(ctx, nullptr, nullptr);
    JSStringRef emitName = JSStringCreateWithUTF8CString("emit");
    JSObjectRef emitFunc = JSObjectMakeFunctionWithCallback(ctx, emitName,
        [](JSContextRef ctx_inner, JSObjectRef, JSObjectRef,
           size_t argc, const JSValueRef args[], JSValueRef*) -> JSValueRef {
            if (argc < 1 || !JSValueIsString(ctx_inner, args[0]))
                return JSValueMakeUndefined(ctx_inner);

            JSStringRef event_str = JSValueToStringCopy(ctx_inner, args[0], nullptr);
            size_t len = JSStringGetMaximumUTF8CStringSize(event_str);
            std::string event_name(len, '\0');
            JSStringGetUTF8CString(event_str, &event_name[0], len);
            event_name.resize(strlen(event_name.c_str()));
            JSStringRelease(event_str);

            std::vector<JSValueRef> event_args;
            if (argc > 1)
                event_args.assign(args + 1, args + argc);

            js::process::ProcessEventEmitter::getInstance().emit(event_name, ctx_inner, event_args);
            return JSValueMakeUndefined(ctx_inner);
        });
    JSObjectSetProperty(ctx, eventsObj, emitName, emitFunc, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(emitName);

    js::ModuleSystem::getInstance().registerBuiltinModule("process", processObj);
    js::ModuleSystem::getInstance().registerBuiltinModule("events", eventsObj);

    JSStringRef script = JSStringCreateWithUTF8CString(js_code.c_str());
    JSValueRef exception = nullptr;
    JSValueRef result = JSEvaluateScript(ctx, script, nullptr, nullptr, 1, &exception);
    JSStringRelease(script);

    if (exception) {
        JSStringRef exceptionString = JSValueToStringCopy(ctx, exception, nullptr);
        size_t maxSize = JSStringGetMaximumUTF8CStringSize(exceptionString);
        std::string buf(maxSize, '\0');
        JSStringGetUTF8CString(exceptionString, &buf[0], maxSize);
        buf.resize(strlen(buf.c_str()));
        std::cerr << "JavaScript Error: " << buf << std::endl;
        JSStringRelease(exceptionString);

        JSStringRef errorStr = JSStringCreateWithUTF8CString(buf.c_str());
        JSValueRef errorValue = JSValueMakeString(ctx, errorStr);
        std::vector<JSValueRef> args = {errorValue};
        js::process::ProcessEventEmitter::getInstance().emit("uncaughtException", ctx, args);
        JSStringRelease(errorStr);

        std::vector<JSValueRef> exitArgs = {JSValueMakeNumber(ctx, 0)};
        js::process::ProcessEventEmitter::getInstance().emit("exit", ctx, exitArgs);

        JSGlobalContextRelease(ctx);
        return;
    }

    JSGlobalContextRelease(ctx);
}
#endif // USE_JAVASCRIPTCORE

} // namespace jpm
