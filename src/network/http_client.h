#ifndef JPM_HTTP_CLIENT_H
#define JPM_HTTP_CLIENT_H

#include <string>
#include <optional> // C++17

namespace jpm {

class HttpClient {
public:
    HttpClient();

    // Returns the body of the response as a string
    // Returns std::nullopt on error
    std::optional<std::string> get(const std::string& url);

    // Downloads a file to the specified path
    // Returns true on success, false on error
    bool download_file(const std::string& url, const std::string& output_path);
};

} // namespace jpm

#endif // JPM_HTTP_CLIENT_H
