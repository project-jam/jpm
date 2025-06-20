#ifndef JPM_JS_PROCESS_STDIN_H
#define JPM_JS_PROCESS_STDIN_H

#ifdef USE_JAVASCRIPTCORE
#include <JavaScriptCore/JavaScript.h>

namespace jpm {
namespace js {
namespace process {

// Sets up process.stdin in the given JavaScript context
// Implements process.stdin.on('data', callback) for reading from standard input
// Currently implements a synchronous read when 'data' event is registered
void setup_stdin(JSContextRef ctx, JSObjectRef process_obj);

} // namespace process
} // namespace js
} // namespace jpm

#endif // USE_JAVASCRIPTCORE
#endif // JPM_JS_PROCESS_STDIN_H