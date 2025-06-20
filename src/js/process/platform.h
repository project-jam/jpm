#ifndef JPM_JS_PROCESS_PLATFORM_H
#define JPM_JS_PROCESS_PLATFORM_H

#ifdef USE_JAVASCRIPTCORE
#include <JavaScriptCore/JavaScript.h>

namespace jpm {
namespace js {
namespace process {

// Sets up platform-specific properties in the process object:
// - process.platform: returns 'win32', 'darwin', or 'linux'
// - process.arch: returns 'x64', 'arm64', etc.
// - process.version: returns JPM's version
// - process.versions: object containing versions of dependencies
// - process.title: program name
// - process.pid: process ID
// - process.ppid: parent process ID
// - process.uptime(): returns process uptime in seconds
// - process.cwd(): returns current working directory
// - process.chdir(dir): changes current working directory
// - process.memoryUsage(): returns memory usage stats {
//       rss: Resident Set Size
//       heapTotal: Total Size of the JS Heap
//       heapUsed: Actually Used Heap
//       external: Memory used by C++ objects bound to JS objects
//   }
void setup_platform(JSContextRef ctx, JSObjectRef process_obj);

} // namespace process
} // namespace js
} // namespace jpm

#endif // USE_JAVASCRIPTCORE
#endif // JPM_JS_PROCESS_PLATFORM_H