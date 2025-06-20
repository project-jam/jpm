#ifndef JPM_JS_PROCESS_EXIT_H
#define JPM_JS_PROCESS_EXIT_H

#ifdef USE_JAVASCRIPTCORE
#include <JavaScriptCore/JavaScript.h>

namespace jpm {
namespace js {
namespace process {

// Sets up process.exit() in the given JavaScript context
// process.exit([code]) will terminate the program with the given exit code
// If no code is provided, exits with 0
void setup_exit(JSContextRef ctx, JSObjectRef process_obj);

} // namespace process
} // namespace js
} // namespace jpm

#endif // USE_JAVASCRIPTCORE
#endif // JPM_JS_PROCESS_EXIT_H