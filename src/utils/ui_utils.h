#ifndef JPM_UI_UTILS_H
#define JPM_UI_UTILS_H

#include <string>
#include <vector>
#include <atomic> // For potential future threaded animation

namespace jpm {
namespace UIUtils {

class ProgressSpinner {
public:
    ProgressSpinner();
    ~ProgressSpinner(); // Ensure spinner is stopped and line is cleared

    void start(const std::string& initial_message);
    void update_message(const std::string& message);
    void tick(); // Manually advance spinner and redraw
    void stop(bool success, const std::string& final_message_override = "");

private:
    void draw();
    void clear_line();
    bool is_tty() const;

    std::string current_message_;
    std::vector<std::string> spinner_chars_;
    size_t spinner_index_;
    std::atomic<bool> active_; // Use atomic for future thread safety
};

} // namespace UIUtils
} // namespace jpm
#endif // JPM_UI_UTILS_H
