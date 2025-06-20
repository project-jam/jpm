#ifndef JPM_JS_PROCESS_EVENTS_H
#define JPM_JS_PROCESS_EVENTS_H

#ifdef USE_JAVASCRIPTCORE
#include <JavaScriptCore/JavaScript.h>
#include <vector>
#include <string>
#include <map>
#include <functional>

namespace jpm {
namespace js {
namespace process {

// EventEmitter-like functionality for process events
class ProcessEventEmitter {
public:
    static ProcessEventEmitter& getInstance();

    // Add event listener
    void on(const std::string& event_name, JSContextRef ctx, JSObjectRef callback);

    // Remove event listener
    void removeListener(const std::string& event_name, JSObjectRef callback);

    // Emit event
    void emit(const std::string& event_name, JSContextRef ctx, const std::vector<JSValueRef>& args = {});

    // Check if has listeners
    bool hasListeners(const std::string& event_name) const;

private:
    ProcessEventEmitter() = default;
    ~ProcessEventEmitter() = default;
    ProcessEventEmitter(const ProcessEventEmitter&) = delete;
    ProcessEventEmitter& operator=(const ProcessEventEmitter&) = delete;

    struct EventListener {
        JSContextRef ctx;
        JSObjectRef callback;
    };

    std::map<std::string, std::vector<EventListener>> listeners;
};

// Sets up process event handling in the given JavaScript context
// Implements:
// - process.on(event, callback)
// - process.emit(event, ...args)
// - process.removeListener(event, callback)
// - process.nextTick(callback)
// 
// Standard events:
// - 'exit' - when the process is about to exit
// - 'beforeExit' - when Node.js event loop is empty
// - 'uncaughtException' - when an uncaught exception occurs
// - 'warning' - when a warning occurs
void setup_events(JSContextRef ctx, JSObjectRef process_obj);

// High-resolution time functionality
// Sets up:
// - process.hrtime([time]) - returns [seconds, nanoseconds]
void setup_hrtime(JSContextRef ctx, JSObjectRef process_obj);

} // namespace process
} // namespace js
} // namespace jpm

#endif // USE_JAVASCRIPTCORE
#endif // JPM_JS_PROCESS_EVENTS_H