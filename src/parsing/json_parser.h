#ifndef JPM_JSON_PARSER_H
#define JPM_JSON_PARSER_H

#include <string>
#include <nlohmann/json.hpp> // From the nlohmann/json library

namespace jpm {

// Alias for convenience
using JsonData = nlohmann::json;

class JsonParser {
public:
    JsonParser();

    // Parses a JSON string into a JsonData object
    // Throws an exception on parsing error (nlohmann::json::parse_error)
    static JsonData parse(const std::string& json_string);

    // Safely try to parse, returns JsonData or empty JsonData if error
    static JsonData try_parse(const std::string& json_string);
};

} // namespace jpm

#endif // JPM_JSON_PARSER_H
