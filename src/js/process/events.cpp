#include "js/process/events.h"

#ifdef USE_JAVASCRIPTCORE
#include "jpm_config.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <cstring>

namespace jpm {
namespace js {
namespace process {

struct EventListener {
    JSContextRef ctx;
    JSObjectRef callback;
};

struct NextTickItem {
    JSContextRef ctx;
    JSObjectRef callback;
    std::vector<JSValueRef> args;
};

static std::queue<NextTickItem> next_tick_queue;
static std::mutex next_tick_mutex;
static bool next_tick_processing = false;

ProcessEventEmitter& ProcessEventEmitter::getInstance() {
    static ProcessEventEmitter instance;
    return instance;
}

void ProcessEventEmitter::on(const std::string& event_name, JSContextRef ctx, JSObjectRef callback) {
    listeners[event_name].push_back({ctx, callback});
    if (g_verbose_output) {
        std::cout << "Added listener for event: " << event_name << std::endl;
    }
}

void ProcessEventEmitter::removeListener(const std::string& event_name, JSObjectRef callback) {
    auto& event_listeners = listeners[event_name];
    event_listeners.erase(
        std::remove_if(event_listeners.begin(), event_listeners.end(),
                       [callback](const EventListener& listener) {
                           return listener.callback == callback;
                       }),
        event_listeners.end());
}

void ProcessEventEmitter::emit(const std::string& event_name, JSContextRef ctx,
                               const std::vector<JSValueRef>& args) {
    if (g_verbose_output) {
        std::cout << "Emitting event: " << event_name << std::endl;
    }

    auto it = listeners.find(event_name);
    if (it != listeners.end()) {
        for (const auto& listener : it->second) {
            JSValueRef exception = nullptr;
            JSObjectCallAsFunction(listener.ctx, listener.callback, nullptr,
                                   args.size(), args.data(), &exception);

            if (exception) {
                JSStringRef error_str = JSValueToStringCopy(ctx, exception, nullptr);
                size_t len = JSStringGetMaximumUTF8CStringSize(error_str);
                std::string error_msg(len, '\0');
                JSStringGetUTF8CString(error_str, &error_msg[0], len);
                error_msg.resize(strlen(error_msg.c_str()));
                std::cerr << "Error in event handler for '" << event_name
                          << "': " << error_msg << std::endl;
                JSStringRelease(error_str);
            }
        }
    }
}

bool ProcessEventEmitter::hasListeners(const std::string& event_name) const {
    auto it = listeners.find(event_name);
    return it != listeners.end() && !it->second.empty();
}

static void process_next_tick_queue() {
    while (true) {
        NextTickItem item;
        {
            std::lock_guard<std::mutex> lock(next_tick_mutex);
            if (next_tick_queue.empty()) {
                next_tick_processing = false;
                break;
            }
            item = next_tick_queue.front();
            next_tick_queue.pop();
        }

        JSValueRef exception = nullptr;
        JSObjectCallAsFunction(item.ctx, item.callback, nullptr,
                               item.args.size(), item.args.data(), &exception);

        if (exception) {
            JSStringRef error_str = JSValueToStringCopy(item.ctx, exception, nullptr);
            size_t len = JSStringGetMaximumUTF8CStringSize(error_str);
            std::string error_msg(len, '\0');
            JSStringGetUTF8CString(error_str, &error_msg[0], len);
            error_msg.resize(strlen(error_msg.c_str()));
            std::cerr << "Error in nextTick callback: " << error_msg << std::endl;
            JSStringRelease(error_str);
        }
    }
}

void setup_events(JSContextRef ctx, JSObjectRef process_obj) {
    JSStringRef on_name = JSStringCreateWithUTF8CString("on");
    JSObjectRef on_func = JSObjectMakeFunctionWithCallback(ctx, on_name,
        [](JSContextRef ctx_inner, JSObjectRef, JSObjectRef,
           size_t argc, const JSValueRef args[], JSValueRef*) -> JSValueRef {
            if (argc < 2 || !JSValueIsString(ctx_inner, args[0]) || !JSValueIsObject(ctx_inner, args[1]))
                return JSValueMakeUndefined(ctx_inner);

            JSStringRef event_str = JSValueToStringCopy(ctx_inner, args[0], nullptr);
            size_t len = JSStringGetMaximumUTF8CStringSize(event_str);
            std::string event_name(len, '\0');
            JSStringGetUTF8CString(event_str, &event_name[0], len);
            event_name.resize(strlen(event_name.c_str()));
            JSStringRelease(event_str);

            ProcessEventEmitter::getInstance().on(event_name, ctx_inner, (JSObjectRef)args[1]);
            return JSValueMakeUndefined(ctx_inner);
        });
    JSObjectSetProperty(ctx, process_obj, on_name, on_func, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(on_name);

    JSStringRef tick_name = JSStringCreateWithUTF8CString("nextTick");
    JSObjectRef tick_func = JSObjectMakeFunctionWithCallback(ctx, tick_name,
        [](JSContextRef ctx_inner, JSObjectRef, JSObjectRef,
           size_t argc, const JSValueRef args[], JSValueRef*) -> JSValueRef {
            if (argc < 1 || !JSValueIsObject(ctx_inner, args[0]))
                return JSValueMakeUndefined(ctx_inner);

            std::vector<JSValueRef> callback_args;
            if (argc > 1)
                callback_args.assign(args + 1, args + argc);

            {
                std::lock_guard<std::mutex> lock(next_tick_mutex);
                next_tick_queue.push({ctx_inner, (JSObjectRef)args[0], callback_args});
                if (!next_tick_processing) {
                    next_tick_processing = true;
                    std::thread(process_next_tick_queue).detach();
                }
            }

            return JSValueMakeUndefined(ctx_inner);
        });
    JSObjectSetProperty(ctx, process_obj, tick_name, tick_func, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(tick_name);

    if (g_verbose_output)
        std::cout << "Setup process events functionality" << std::endl;
}

void setup_hrtime(JSContextRef ctx, JSObjectRef process_obj) {
    JSStringRef hrtime_name = JSStringCreateWithUTF8CString("hrtime");
    JSObjectRef hrtime_func = JSObjectMakeFunctionWithCallback(ctx, hrtime_name,
        [](JSContextRef ctx_inner, JSObjectRef, JSObjectRef,
           size_t argc, const JSValueRef args[], JSValueRef*) -> JSValueRef {
            using namespace std::chrono;
            static const auto start_time = high_resolution_clock::now();

            auto now = high_resolution_clock::now();
            auto duration = now - start_time;

            if (argc > 0 && JSValueIsObject(ctx_inner, args[0])) {
                JSObjectRef time_array = (JSObjectRef)args[0];
                JSValueRef sec_ref = JSObjectGetPropertyAtIndex(ctx_inner, time_array, 0, nullptr);
                JSValueRef nano_ref = JSObjectGetPropertyAtIndex(ctx_inner, time_array, 1, nullptr);

                if (JSValueIsNumber(ctx_inner, sec_ref) && JSValueIsNumber(ctx_inner, nano_ref)) {
                    int64_t prev_sec = static_cast<int64_t>(JSValueToNumber(ctx_inner, sec_ref, nullptr));
                    int64_t prev_ns = static_cast<int64_t>(JSValueToNumber(ctx_inner, nano_ref, nullptr));
                    duration -= seconds(prev_sec) + nanoseconds(prev_ns);
                }
            }

            auto secs = duration_cast<seconds>(duration);
            auto nanos = duration_cast<nanoseconds>(duration - secs);

            JSValueRef values[2];
            values[0] = JSValueMakeNumber(ctx_inner, secs.count());
            values[1] = JSValueMakeNumber(ctx_inner, nanos.count());

            return JSObjectMakeArray(ctx_inner, 2, values, nullptr);
        });
    JSObjectSetProperty(ctx, process_obj, hrtime_name, hrtime_func, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(hrtime_name);

    if (g_verbose_output)
        std::cout << "Setup process hrtime functionality" << std::endl;
}

} // namespace process
} // namespace js
} // namespace jpm

#endif // USE_JAVASCRIPTCORE
