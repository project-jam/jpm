#include "js/js.h"
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

    JSObjectRef globalObject = JSContextGetGlobalObject(ctx);

    // Implement console.log
    {
        JSObjectRef consoleObj = JSObjectMake(ctx, nullptr, nullptr);
        JSStringRef logName = JSStringCreateWithUTF8CString("log");
        // Non-capturing lambda for console.log
        JSObjectRef logFunction = JSObjectMakeFunctionWithCallback(ctx, logName,
            [](JSContextRef ctxInner, JSObjectRef function, JSObjectRef thisObject,
               size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
                for (size_t i = 0; i < argumentCount; ++i) {
                    JSStringRef strRef = JSValueToStringCopy(ctxInner, arguments[i], nullptr);
                    size_t maxSize = JSStringGetMaximumUTF8CStringSize(strRef);
                    std::string s;
                    s.resize(maxSize);
                    JSStringGetUTF8CString(strRef, s.data(), maxSize);
                    size_t len = strlen(s.c_str());
                    std::cout << s.substr(0, len);
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

    // Extend JavaScript environment with interactive input function `input()`
    {
        JSStringRef inputFunctionName = JSStringCreateWithUTF8CString("input");
        JSObjectRef inputFunction = JSObjectMakeFunctionWithCallback(ctx, inputFunctionName,
            [](JSContextRef ctxInner, JSObjectRef function, JSObjectRef thisObject,
               size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
                // Print prompt if provided
                if (argumentCount > 0 && JSValueIsString(ctxInner, arguments[0])) {
                    JSStringRef promptStrRef = JSValueToStringCopy(ctxInner, arguments[0], nullptr);
                    size_t maxSize = JSStringGetMaximumUTF8CStringSize(promptStrRef);
                    std::string prompt_s;
                    prompt_s.resize(maxSize);
                    JSStringGetUTF8CString(promptStrRef, prompt_s.data(), maxSize);
                    size_t len = strlen(prompt_s.c_str());
                    std::cout << prompt_s.substr(0, len) << std::flush;
                    JSStringRelease(promptStrRef);
                }

                std::string line;
                if (!std::getline(std::cin, line)) {
                    line.clear();  // EOF or error
                }

                JSStringRef resultStrRef = JSStringCreateWithUTF8CString(line.c_str());
                JSValueRef result = JSValueMakeString(ctxInner, resultStrRef);
                JSStringRelease(resultStrRef);
                return result;
            });
        JSObjectSetProperty(ctx, globalObject, inputFunctionName, inputFunction, kJSPropertyAttributeNone, nullptr);
        JSStringRelease(inputFunctionName);
    }

    // Add process object and sub-properties
    JSObjectRef processObj = JSObjectMake(ctx, nullptr, nullptr);

    // process.exit(...)
    {
        JSStringRef exitFunctionName = JSStringCreateWithUTF8CString("exit");
        JSObjectRef exitFunction = JSObjectMakeFunctionWithCallback(ctx, exitFunctionName,
            [](JSContextRef ctxInner, JSObjectRef function, JSObjectRef thisObject,
               size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
                int exitCode = 0;
                if (argumentCount > 0 && JSValueIsNumber(ctxInner, arguments[0])) {
                    exitCode = (int)JSValueToNumber(ctxInner, arguments[0], nullptr);
                }
                exit(exitCode);
                return JSValueMakeUndefined(ctxInner);
            });
        JSObjectSetProperty(ctx, processObj, exitFunctionName, exitFunction, kJSPropertyAttributeNone, nullptr);
        JSStringRelease(exitFunctionName);
    }

    // process.argv = ["node", "<file_path>"]
    {
        JSValueRef argvValues[2];
        JSStringRef nodeStr = JSStringCreateWithUTF8CString("node");
        JSStringRef fileStr = JSStringCreateWithUTF8CString(file_path.c_str());
        argvValues[0] = JSValueMakeString(ctx, nodeStr);
        argvValues[1] = JSValueMakeString(ctx, fileStr);
        JSObjectRef argvArray = JSObjectMakeArray(ctx, 2, argvValues, nullptr);
        JSStringRelease(nodeStr);
        JSStringRelease(fileStr);
        JSStringRef argvName = JSStringCreateWithUTF8CString("argv");
        JSObjectSetProperty(ctx, processObj, argvName, argvArray, kJSPropertyAttributeNone, nullptr);
        JSStringRelease(argvName);
    }

    // process.stdout.write(...)
    {
        JSObjectRef stdoutObj = JSObjectMake(ctx, nullptr, nullptr);
        JSStringRef writeFunctionName = JSStringCreateWithUTF8CString("write");
        JSObjectRef writeFunction = JSObjectMakeFunctionWithCallback(ctx, writeFunctionName,
            [](JSContextRef ctxInner, JSObjectRef function, JSObjectRef thisObject,
               size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
                if (argumentCount > 0) {
                    JSStringRef strRef = JSValueToStringCopy(ctxInner, arguments[0], nullptr);
                    size_t maxSize = JSStringGetMaximumUTF8CStringSize(strRef);
                    std::string s;
                    s.resize(maxSize);
                    JSStringGetUTF8CString(strRef, s.data(), maxSize);
                    size_t len = strlen(s.c_str());
                    std::cout << s.substr(0, len) << std::flush;
                    JSStringRelease(strRef);
                }
                return JSValueMakeUndefined(ctxInner);
            });
        JSObjectSetProperty(ctx, stdoutObj, writeFunctionName, writeFunction, kJSPropertyAttributeNone, nullptr);
        JSStringRelease(writeFunctionName);

        JSStringRef stdoutName = JSStringCreateWithUTF8CString("stdout");
        JSObjectSetProperty(ctx, processObj, stdoutName, stdoutObj, kJSPropertyAttributeNone, nullptr);
        JSStringRelease(stdoutName);
    }

    // process.stderr.write(...)
    {
        JSObjectRef stderrObj = JSObjectMake(ctx, nullptr, nullptr);
        JSStringRef stderrWriteFunctionName = JSStringCreateWithUTF8CString("write");
        JSObjectRef stderrWriteFunction = JSObjectMakeFunctionWithCallback(ctx, stderrWriteFunctionName,
            [](JSContextRef ctxInner, JSObjectRef function, JSObjectRef thisObject,
               size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
                if (argumentCount > 0) {
                    JSStringRef strRef = JSValueToStringCopy(ctxInner, arguments[0], nullptr);
                    size_t maxSize = JSStringGetMaximumUTF8CStringSize(strRef);
                    std::string s;
                    s.resize(maxSize);
                    JSStringGetUTF8CString(strRef, s.data(), maxSize);
                    size_t len = strlen(s.c_str());
                    std::cerr << s.substr(0, len) << std::flush;
                    JSStringRelease(strRef);
                }
                return JSValueMakeUndefined(ctxInner);
            });
        JSObjectSetProperty(ctx, stderrObj, stderrWriteFunctionName, stderrWriteFunction, kJSPropertyAttributeNone, nullptr);
        JSStringRelease(stderrWriteFunctionName);

        JSStringRef stderrName = JSStringCreateWithUTF8CString("stderr");
        JSObjectSetProperty(ctx, processObj, stderrName, stderrObj, kJSPropertyAttributeNone, nullptr);
        JSStringRelease(stderrName);
    }

    // process.stdin.on(...) stub that reads one line synchronously and invokes callback
    {
        JSObjectRef stdinObj = JSObjectMake(ctx, nullptr, nullptr);
        JSStringRef onFunctionName = JSStringCreateWithUTF8CString("on");
        JSObjectRef onFunction = JSObjectMakeFunctionWithCallback(ctx, onFunctionName,
            [](JSContextRef ctxInner, JSObjectRef function, JSObjectRef thisObject,
               size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
                // Expect: arguments[0] = event name (string), arguments[1] = callback (function)
                if (argumentCount >= 2 && JSValueIsString(ctxInner, arguments[0]) && JSValueIsObject(ctxInner, arguments[1])) {
                    JSStringRef eventStrRef = JSValueToStringCopy(ctxInner, arguments[0], nullptr);
                    size_t maxSize = JSStringGetMaximumUTF8CStringSize(eventStrRef);
                    std::string eventName;
                    eventName.resize(maxSize);
                    JSStringGetUTF8CString(eventStrRef, eventName.data(), maxSize);
                    size_t len = strlen(eventName.c_str());
                    eventName = eventName.substr(0, len);
                    JSStringRelease(eventStrRef);

                    if (eventName == "data") {
                        // Read one line from stdin synchronously
                        std::string line;
                        if (!std::getline(std::cin, line)) {
                            line.clear();
                        }
                        // Convert to JS string
                        JSStringRef jsLine = JSStringCreateWithUTF8CString(line.c_str());
                        JSValueRef jsValue = JSValueMakeString(ctxInner, jsLine);
                        JSStringRelease(jsLine);

                        JSObjectRef callback = (JSObjectRef)arguments[1];
                        JSValueRef callbackException = nullptr;
                        JSObjectCallAsFunction(ctxInner, callback, nullptr, 1, &jsValue, &callbackException);
                        if (callbackException) {
                            JSStringRef excStr = JSValueToStringCopy(ctxInner, callbackException, nullptr);
                            size_t excSize = JSStringGetMaximumUTF8CStringSize(excStr);
                            std::string buf;
                            buf.resize(excSize);
                            JSStringGetUTF8CString(excStr, buf.data(), excSize);
                            size_t excLen = strlen(buf.c_str());
                            std::cerr << "Error in process.stdin.on callback: " << buf.substr(0, excLen) << std::endl;
                            JSStringRelease(excStr);
                        }
                    }
                    // Other events (e.g., 'end') are ignored for now.
                }
                return JSValueMakeUndefined(ctxInner);
            });
        JSObjectSetProperty(ctx, stdinObj, onFunctionName, onFunction, kJSPropertyAttributeNone, nullptr);
        JSStringRelease(onFunctionName);

        JSStringRef stdinName = JSStringCreateWithUTF8CString("stdin");
        JSObjectSetProperty(ctx, processObj, stdinName, stdinObj, kJSPropertyAttributeNone, nullptr);
        JSStringRelease(stdinName);
    }

    // Finally add process to global
    {
        JSStringRef processName = JSStringCreateWithUTF8CString("process");
        JSObjectSetProperty(ctx, globalObject, processName, processObj, kJSPropertyAttributeNone, nullptr);
        JSStringRelease(processName);
    }

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
        (void)result;
    }

    JSStringRelease(script);
    JSGlobalContextRelease(ctx);
}
#endif // USE_JAVASCRIPTCORE

} // namespace jpm
