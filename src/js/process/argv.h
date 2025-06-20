#ifndef JPM_JS_PROCESS_ARGV_H
#define JPM_JS_PROCESS_ARGV_H

#ifdef USE_JAVASCRIPTCORE
#include <JavaScriptCore/JavaScript.h>
#include <string>

namespace jpm {
namespace js {
namespace process {

// Sets up process.argv in the given JavaScript context
// process.argv will be initialized as ["node", "<script_path>"]
void setup_argv(JSContextRef ctx, JSObjectRef process_obj, const std::string& script_path);

} // namespace process
} // namespace js
} // namespace jpm

#endif // USE_JAVASCRIPTCORE
#endif // JPM_JS_PROCESS_ARGV_H