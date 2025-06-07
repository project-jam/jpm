#ifndef JPM_TARBALL_HANDLER_H
#define JPM_TARBALL_HANDLER_H

#include <string>
#include "network/http_client.h"

namespace jpm {

class TarballHandler {
public:
    TarballHandler();

    // Downloads and extracts a tarball to a specified directory
    // Returns true on success, false on error.
    bool download_and_extract(
        const std::string& tarball_url,
        const std::string& package_name, // For creating a subdirectory, e.g. node_modules/lodash
        const std::string& package_version, // For versioned paths or cache keys
        const std::string& base_destination_path // e.g., "./node_modules" or a global cache path
    );

private:
    HttpClient http_client_;

    bool extract_tarball(
        const std::string& local_tarball_path,
        const std::string& extract_to_path
    );
};

} // namespace jpm

#endif // JPM_TARBALL_HANDLER_H
