#include "js/module.h"
#include "js/process/events.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>

namespace jpm::js {

namespace fs = std::filesystem;

void ModuleSystem::init(JSContextRef ctx, JSObjectRef globalObject) {
    context = ctx;
    globalObj = globalObject;
    workingDir = fs::current_path();
    setup_require(ctx, globalObject);
}

void ModuleSystem::setup_require(JSContextRef ctx, JSObjectRef globalObject) {
    JSStringRef requireName = JSStringCreateWithUTF8CString("require");

    // Create the require function
    JSObjectRef requireFunc = JSObjectMakeFunctionWithCallback(ctx, requireName,
        [](JSContextRef ctxInner, JSObjectRef function, JSObjectRef thisObject,
           size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
            if (argumentCount < 1) {
                *exception = JSValueMakeString(ctxInner,
                    JSStringCreateWithUTF8CString("require() requires a module name"));
                return JSValueMakeUndefined(ctxInner);
            }

            JSStringRef modulePathStr = JSValueToStringCopy(ctxInner, arguments[0], nullptr);
            size_t bufSize = JSStringGetMaximumUTF8CStringSize(modulePathStr);
            std::string modulePath(bufSize, '\0');
            JSStringGetUTF8CString(modulePathStr, &modulePath[0], bufSize);
            modulePath.resize(strlen(modulePath.c_str()));
            JSStringRelease(modulePathStr);

            return ModuleSystem::getInstance().require(ctxInner, modulePath);
        });

    // Set require in global scope
    JSObjectSetProperty(ctx, globalObject, requireName, requireFunc,
        kJSPropertyAttributeNone, nullptr);
    JSStringRelease(requireName);
}

JSObjectRef ModuleSystem::require(JSContextRef ctx, const std::string& modulePath) {
    // Check if module is already cached
    if (moduleCache.find(modulePath) != moduleCache.end()) {
        return moduleCache[modulePath];
    }

    // Check if it's a built-in module first
    if (isBuiltinModule(modulePath)) {
        return loadBuiltinModule(ctx, modulePath);
    }

    // Resolve the full module path
    std::string resolvedPath = resolveModulePath(modulePath);
    if (resolvedPath.empty()) {
        JSStringRef errorMsg = JSStringCreateWithUTF8CString(
            ("Module not found: " + modulePath).c_str());
        throw std::runtime_error(
            "Module not found: " + modulePath);
    }

    return loadNodeModule(ctx, resolvedPath);
}

bool ModuleSystem::isBuiltinModule(const std::string& moduleName) const {
    return builtinModules.find(moduleName) != builtinModules.end();
}

void ModuleSystem::registerBuiltinModule(const std::string& name, JSObjectRef module) {
    builtinModules[name] = module;
}

JSObjectRef ModuleSystem::loadBuiltinModule(JSContextRef ctx, const std::string& moduleName) {
    return builtinModules[moduleName];
}

std::string ModuleSystem::resolveModulePath(const std::string& requestedModule) {
    // If it starts with ./ or ../, resolve relative to current directory
    if (requestedModule.substr(0, 2) == "./" || requestedModule.substr(0, 3) == "../") {
        fs::path fullPath = workingDir / requestedModule;
        if (fs::exists(fullPath)) {
            return fullPath.string();
        }
        // Try with .js extension
        fullPath += ".js";
        if (fs::exists(fullPath)) {
            return fullPath.string();
        }
    } else {
        // Look in node_modules
        return findPackageInNodeModules(requestedModule);
    }
    return "";
}

std::string ModuleSystem::findPackageInNodeModules(const std::string& packageName) {
    fs::path currentDir = workingDir;

    while (true) {
        fs::path nodeModulesDir = currentDir / "node_modules";
        fs::path packageDir = nodeModulesDir / packageName;

        // Check if package directory exists
        if (fs::exists(packageDir)) {
            // Look for package.json
            fs::path packageJson = packageDir / "package.json";
            if (fs::exists(packageJson)) {
                std::string content = readFile(packageJson.string());
                // Very basic package.json parsing - in real implementation, use proper JSON parser
                if (content.find("\"main\"") != std::string::npos) {
                    size_t start = content.find("\"main\"") + 7;
                    start = content.find("\"", start) + 1;
                    size_t end = content.find("\"", start);
                    std::string mainFile = content.substr(start, end - start);
                    fs::path mainPath = packageDir / mainFile;
                    if (fs::exists(mainPath)) {
                        return mainPath.string();
                    }
                }
            }

            // Default to index.js if no package.json or main not found
            fs::path indexJs = packageDir / "index.js";
            if (fs::exists(indexJs)) {
                return indexJs.string();
            }
        }

        // Move up one directory
        fs::path parent = currentDir.parent_path();
        if (parent == currentDir) {
            break;  // Reached root
        }
        currentDir = parent;
    }

    return "";
}

JSObjectRef ModuleSystem::loadNodeModule(JSContextRef ctx, const std::string& filePath) {
    // Create module object with exports
    JSObjectRef moduleExports = createModuleExports(ctx);

    // Cache the exports object before executing to handle circular dependencies
    moduleCache[filePath] = moduleExports;

    // Read and execute the module code
    std::string code = readFile(filePath);
    wrapModuleCode(code);

    JSStringRef script = JSStringCreateWithUTF8CString(code.c_str());
    JSValueRef exception = nullptr;
    JSEvaluateScript(ctx, script, nullptr, nullptr, 1, &exception);
    JSStringRelease(script);

    if (exception) {
        JSStringRef exStr = JSValueToStringCopy(ctx, exception, nullptr);
        size_t bufSize = JSStringGetMaximumUTF8CStringSize(exStr);
        std::string errorMsg(bufSize, '\0');
        JSStringGetUTF8CString(exStr, &errorMsg[0], bufSize);
        JSStringRelease(exStr);
        throw std::runtime_error("Module execution failed: " + errorMsg);
    }

    return moduleExports;
}

JSObjectRef ModuleSystem::createModuleExports(JSContextRef ctx) {
    return JSObjectMake(ctx, nullptr, nullptr);
}

std::string ModuleSystem::readFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filePath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void ModuleSystem::wrapModuleCode(std::string& code) {
    // Wrap the code in a function to create module scope
    code = "(function(exports, require, module, __filename, __dirname) {\n" +
           code + "\n})(exports, require, module, __filename, __dirname);";
}

static JSValueRef emitter_on(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
    size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    if (argumentCount < 2) return JSValueMakeUndefined(ctx);

    JSStringRef key = JSStringCreateWithUTF8CString("__events");
    JSValueRef events = JSObjectGetProperty(ctx, thisObject, key, exception);
    JSObjectRef eventsObj;

    if (!events || JSValueIsUndefined(ctx, events)) {
        eventsObj = JSObjectMake(ctx, nullptr, nullptr);
        JSObjectSetProperty(ctx, thisObject, key, eventsObj, kJSPropertyAttributeDontEnum, exception);
    } else {
        eventsObj = (JSObjectRef)events;
    }

    JSStringRef eventName = JSValueToStringCopy(ctx, arguments[0], exception);
    JSObjectSetProperty(ctx, eventsObj, eventName, arguments[1], kJSPropertyAttributeNone, exception);

    JSStringRelease(eventName);
    JSStringRelease(key);
    return JSValueMakeUndefined(ctx);
}

static JSValueRef emitter_emit(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
    size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    if (argumentCount < 1) return JSValueMakeUndefined(ctx);

    JSStringRef key = JSStringCreateWithUTF8CString("__events");
    JSValueRef events = JSObjectGetProperty(ctx, thisObject, key, exception);

    if (events && !JSValueIsUndefined(ctx, events)) {
        JSStringRef eventName = JSValueToStringCopy(ctx, arguments[0], exception);
        JSValueRef listener = JSObjectGetProperty(ctx, (JSObjectRef)events, eventName, exception);

        if (listener && !JSValueIsUndefined(ctx, listener)) {
            JSObjectCallAsFunction(ctx, (JSObjectRef)listener, thisObject,
                argumentCount - 1, arguments + 1, exception);
        }
        JSStringRelease(eventName);
    }

    JSStringRelease(key);
    return JSValueMakeUndefined(ctx);
}

static JSValueRef emitter_constructor(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
    size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    JSObjectRef newEmitter = JSObjectMake(ctx, nullptr, nullptr);

    // Add methods
    JSStringRef onName = JSStringCreateWithUTF8CString("on");
    JSObjectRef onFunc = JSObjectMakeFunctionWithCallback(ctx, onName, emitter_on);
    JSObjectSetProperty(ctx, newEmitter, onName, onFunc, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(onName);

    JSStringRef emitName = JSStringCreateWithUTF8CString("emit");
    JSObjectRef emitFunc = JSObjectMakeFunctionWithCallback(ctx, emitName, emitter_emit);
    JSObjectSetProperty(ctx, newEmitter, emitName, emitFunc, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(emitName);

    return newEmitter;
}

void setup_module_system(JSContextRef ctx, JSObjectRef globalObject) {
    ModuleSystem::getInstance().init(ctx, globalObject);

    // Create the 'events' module object
    JSObjectRef eventsModule = JSObjectMake(ctx, nullptr, nullptr);

    // Create EventEmitter constructor function
    JSStringRef emitterName = JSStringCreateWithUTF8CString("EventEmitter");
    JSObjectRef emitterCtor = JSObjectMakeFunctionWithCallback(ctx, emitterName, emitter_constructor);

    // Create prototype object
    JSObjectRef emitterProto = JSObjectMake(ctx, nullptr, nullptr);

    // Add 'on' method to prototype
    JSStringRef onName = JSStringCreateWithUTF8CString("on");
    JSObjectRef onFunc = JSObjectMakeFunctionWithCallback(ctx, onName, emitter_on);
    JSObjectSetProperty(ctx, emitterProto, onName, onFunc, kJSPropertyAttributeDontEnum, nullptr);
    JSStringRelease(onName);

    // Add 'emit' method to prototype
    JSStringRef emitName = JSStringCreateWithUTF8CString("emit");
    JSObjectRef emitFunc = JSObjectMakeFunctionWithCallback(ctx, emitName, emitter_emit);
    JSObjectSetProperty(ctx, emitterProto, emitName, emitFunc, kJSPropertyAttributeDontEnum, nullptr);
    JSStringRelease(emitName);

    // Set prototype on constructor
    JSStringRef protoName = JSStringCreateWithUTF8CString("prototype");
    JSObjectSetProperty(ctx, emitterCtor, protoName, emitterProto, kJSPropertyAttributeDontEnum, nullptr);
    JSStringRelease(protoName);

    // Export EventEmitter constructor as the 'exports' of the module
    JSStringRef exportsName = JSStringCreateWithUTF8CString("exports");
    JSObjectSetProperty(ctx, eventsModule, exportsName, emitterCtor, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(exportsName);
    JSStringRelease(emitterName);

    // Register the 'events' module as a builtin module
    ModuleSystem::getInstance().registerBuiltinModule("events", eventsModule);
}

} // namespace jpm::js
