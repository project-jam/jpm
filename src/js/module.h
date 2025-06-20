#ifndef JPM_MODULE_H
#define JPM_MODULE_H

#include <JavaScriptCore/JavaScript.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <filesystem>

namespace jpm::js {

class ModuleSystem {
public:
    static ModuleSystem& getInstance() {
        static ModuleSystem instance;
        return instance;
    }

    // Initialize the module system
    void init(JSContextRef ctx, JSObjectRef globalObject);
    
    // Setup the require function in global scope
    void setup_require(JSContextRef ctx, JSObjectRef globalObject);
    
    // Core require implementation
    JSObjectRef require(JSContextRef ctx, const std::string& modulePath);

    // Register a built-in module (for process modules)
    void registerBuiltinModule(const std::string& name, JSObjectRef module);

private:
    ModuleSystem() = default;
    ~ModuleSystem() = default;
    ModuleSystem(const ModuleSystem&) = delete;
    ModuleSystem& operator=(const ModuleSystem&) = delete;

    // Cache to prevent duplicate loading
    std::unordered_map<std::string, JSObjectRef> moduleCache;
    
    // Built-in modules registry (process modules)
    std::unordered_map<std::string, JSObjectRef> builtinModules;
    
    // Working directory for relative paths
    std::filesystem::path workingDir;
    
    // Module resolution
    std::string resolveModulePath(const std::string& requestedModule);
    bool isBuiltinModule(const std::string& moduleName) const;
    JSObjectRef loadBuiltinModule(JSContextRef ctx, const std::string& moduleName);
    JSObjectRef loadNodeModule(JSContextRef ctx, const std::string& filePath);
    
    // Utility functions
    std::string findPackageInNodeModules(const std::string& packageName);
    std::string readFile(const std::string& filePath);
    JSObjectRef createModuleExports(JSContextRef ctx);
    void wrapModuleCode(std::string& code);
    
    // Context storage
    JSContextRef context{nullptr};
    JSObjectRef globalObj{nullptr};
};

// Helper to set up the module system
void setup_module_system(JSContextRef ctx, JSObjectRef globalObject);

} // namespace jpm::js

#endif // JPM_MODULE_H