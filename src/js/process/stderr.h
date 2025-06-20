#ifndef JPM_JS_PROCESS_STDERR_H
#define JPM_JS_PROCESS_STDERR_H

#ifdef USE_JAVASCRIPTCORE
#include <JavaScriptCore/JavaScript.h>

namespace jpm {
namespace js {
namespace process {

// Sets up process.stderr in the given JavaScript context
// Implements process.stderr.write() for writing to standard error output
void setup_stderr(JSContextRef ctx, JSObjectRef process_obj);

} // namespace process
} // namespace js
} // namespace jpm

#endif // USE_JAVASCRIPTCORE
#endif // JPM_JS_PROCESS_STDERR_H