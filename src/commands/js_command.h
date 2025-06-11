#ifndef JPM_JS_COMMAND_H
#define JPM_JS_COMMAND_H

#include <string>
#include <vector>

namespace jpm {

class JSCommand {
public:
    JSCommand();
    void execute(const std::vector<std::string>& args);

private:
    void execute_js_file(const std::string& file_path);
};

} // namespace jpm

#endif // JPM_JS_COMMAND_H
