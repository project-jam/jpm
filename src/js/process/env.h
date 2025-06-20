#ifndef JPM_JS_PROCESS_ENV_H
#define JPM_JS_PROCESS_ENV_H

#ifdef USE_JAVASCRIPTCORE
#include <JavaScriptCore/JavaScript.h>

namespace jpm {
namespace js {
namespace process {

// Sets up process.env in the given JavaScript context
// Provides access to environment variables
// Example: process.env.PATH, process.env.HOME, etc.
void setup_env(JSContextRef ctx, JSObjectRef process_obj);

// Helper function to set environment variables
// Returns true if successful, false otherwise
bool set_env(JSContextRef ctx, JSObjectRef env_obj, const char* name, const char* value);

} // namespace process
} // namespace js
} // namespace jpm

#endif // USE_JAVASCRIPTCORE
#endif // JPM_JS_PROCESS_ENV_H