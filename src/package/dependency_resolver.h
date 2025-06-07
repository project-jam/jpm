#ifndef JPM_DEPENDENCY_RESOLVER_H
#define JPM_DEPENDENCY_RESOLVER_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include "package/package_spec.h"
#include "package/package_info.h"
#include "network/http_client.h"
#include "parsing/json_parser.h"
#include <mutex>

namespace jpm {

struct ResolutionResult {
    PackageSpec requested_package;
    std::vector<PackageInfo> packages_to_install;
    bool success = false;
    std::string error_message;
};

class DependencyResolver {
public:
    DependencyResolver();

    ResolutionResult resolve(const PackageSpec& initial_package_spec);

private:
    HttpClient http_client_;
    std::mutex resolver_mutex_;
    std::unordered_map<std::string, PackageInfo> package_cache_;

    bool resolve_recursive(
        const PackageSpec& current_spec,
        std::map<std::string, PackageInfo>& packages_to_install_map,
        std::set<std::string> visited_on_current_path,
        std::string& error_accumulator
    );

    PackageInfo fetch_and_parse_package_info(const PackageSpec& spec);
};

} // namespace jpm

#endif // JPM_DEPENDENCY_RESOLVER_H
