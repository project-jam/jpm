#ifndef JPM_PACKAGE_INFO_H
#define JPM_PACKAGE_INFO_H

#include <string>
#include <vector>
#include <map>
#include "package/package_spec.h"

namespace jpm {

// Represents the resolved information for a specific package version
struct PackageInfo {
    std::string name;
    std::string resolved_version;
    std::string tarball_url;
    std::map<std::string, std::string> dependencies; // name -> version_requirement string
    // std::map<std::string, std::string> dev_dependencies;
    // ... other fields like description, license, etc.

    // Could be populated from parsed JSON
};

} // namespace jpm

#endif // JPM_PACKAGE_INFO_H
