#include "parsing/json_parser.h"
#include <iostream> // For std::cerr

namespace jpm {

JsonParser::JsonParser() {
    // Constructor - though static methods are used, constructor might be needed for other reasons or future non-static methods.
}

JsonData JsonParser::parse(const std::string& json_string) {
    // nlohmann/json library will throw nlohmann::json::parse_error on failure.
    // This is fine if the caller is prepared to catch it.
    return nlohmann::json::parse(json_string);
}

JsonData JsonParser::try_parse(const std::string& json_string) {
    try {
        return nlohmann::json::parse(json_string);
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return nlohmann::json(); // Return an empty/null json object on error
    }
}

} // namespace jpm
