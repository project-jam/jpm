#ifndef JPM_PACKAGE_SPEC_H
#define JPM_PACKAGE_SPEC_H

#include <string>

namespace jpm {

// Represents a request for a package, e.g., "lodash" or "lodash@^1.0.0"
struct PackageSpec {
    std::string name;
    std::string version_requirement; // e.g., "^1.0.0", "1.2.3", "latest", "" (empty means latest)

    // Default constructor
    PackageSpec() : name(""), version_requirement("latest") {}

    PackageSpec(std::string n, std::string v_req = "latest")
        : name(std::move(n)), version_requirement(std::move(v_req)) {}

    // TODO: Add a static factory method to parse "name@version_req" string
    // static PackageSpec from_string(const std::string& spec_string);

    std::string to_string() const {
        if (version_requirement.empty() || version_requirement == "latest") {
            return name;
        }
        return name + "@" + version_requirement;
    }
};

} // namespace jpm

#endif // JPM_PACKAGE_SPEC_H
