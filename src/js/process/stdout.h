#ifndef JPM_JS_PROCESS_STDOUT_H
#define JPM_JS_PROCESS_STDOUT_H

#ifdef USE_JAVASCRIPTCORE
#include <JavaScriptCore/JavaScript.h>

namespace jpm {
namespace js {
namespace process {

// Sets up process.stdout in the given JavaScript context
// Implements process.stdout.write() for writing to standard output
void setup_stdout(JSContextRef ctx, JSObjectRef process_obj);

} // namespace process
} // namespace js
} // namespace jpm

#endif // USE_JAVASCRIPTCORE
#endif // JPM_JS_PROCESS_STDOUT_H