#include "utils/ui_utils.h"
#include "jpm_config.h" // For g_verbose_output
#include <iostream>
#include <chrono>
#include <thread> // For std::this_thread::sleep_for (for simple tick)

#ifdef _WIN32
#include <io.h>   // For _isatty, _fileno on Windows
#include <cstdio> // For stdout, stderr
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h> // For isatty() on non-Windows
#include <cstdio>   // For stdout, stderr fileno on non-Windows
#endif

namespace jpm {
namespace UIUtils {

ProgressSpinner::ProgressSpinner() 
    : spinner_chars_{"⠇", "⠋", "⠙", "⠸", "⠴", "⠦"}, // Alternative: {"|", "/", "-", "\\"}
      spinner_index_(0),
      active_(false) {
}

ProgressSpinner::~ProgressSpinner() {
    if (active_.load()) {
        stop(true, ""); // Default stop if active
    }
}

bool ProgressSpinner::is_tty() const {
    // Check stdout. If redirecting stdout, spinner is less useful.
    // Use ::isatty and fileno which are conditionally defined above
    return ::isatty(fileno(stdout));
}

void ProgressSpinner::start(const std::string& initial_message) {
    if (g_verbose_output || !is_tty()) return;
    current_message_ = initial_message;
    active_ = true;
    spinner_index_ = 0;
    draw();
}

void ProgressSpinner::update_message(const std::string& message) {
    if (!active_.load() || g_verbose_output || !is_tty()) return;
    current_message_ = message;
    draw();
}

void ProgressSpinner::tick() {
    if (!active_.load() || g_verbose_output || !is_tty()) return;
    spinner_index_ = (spinner_index_ + 1) % spinner_chars_.size();
    draw();
    // std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
}

void ProgressSpinner::stop(bool success, const std::string& final_message_override) {
    if (!active_.load()) return; // Only stop if active
    active_ = false;
    if (g_verbose_output || !is_tty()) {
        // If we were in verbose or non-TTY, just print final message if provided
        if (!final_message_override.empty()) {
            std::cout << final_message_override << std::endl;
        }
        return;
    }

    clear_line();
    std::string final_char = success ? "✔" : "✖"; // Or use different symbols
    std::string message_to_print = final_message_override.empty() ? current_message_ : final_message_override;
    
    std::cout << final_char << " " << message_to_print << std::endl;
}

void ProgressSpinner::draw() {
    if (g_verbose_output || !is_tty() || !active_.load()) return;
    clear_line();
    std::cout << spinner_chars_[spinner_index_] << " " << current_message_ << std::flush;
}

void ProgressSpinner::clear_line() {
    if (g_verbose_output || !is_tty()) return;
    // Standard way to clear a line: carriage return, then print spaces, then carriage return again.
    // Assumes a reasonable line length.
    std::cout << "\r" << std::string(80, ' ') << "\r" << std::flush;
}

} // namespace UIUtils
} // namespace jpm
