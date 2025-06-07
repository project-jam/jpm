#include "package/dependency_resolver.h"
#include "jpm_config.h"
#include <iostream>
#include <future>
#include <thread>
#include <vector>
#include <algorithm>

namespace jpm {

DependencyResolver::DependencyResolver() {
    if (g_verbose_output) {
        std::cout << "DependencyResolver initialized." << std::endl;
    }
}

ResolutionResult DependencyResolver::resolve(const PackageSpec& initial_package_spec) {
    if (g_verbose_output) {
        std::cout << "Top-level resolve initiated for: " << initial_package_spec.to_string() << std::endl;
    }
    ResolutionResult result;
    result.requested_package = initial_package_spec;

    std::map<std::string, PackageInfo> packages_to_install_map;
    std::set<std::string> visited_on_path;

    if (resolve_recursive(initial_package_spec, packages_to_install_map, visited_on_path, result.error_message)) {
        result.success = true;
        for (const auto& pair : packages_to_install_map) {
            result.packages_to_install.push_back(pair.second);
        }
        if (g_verbose_output) {
            std::cout << "Successfully resolved all dependencies for: " << initial_package_spec.to_string() << std::endl;
        }
    } else {
        result.success = false;
        if (result.error_message.empty() && !result.success) {
            result.error_message = "Unknown error during resolution for " + initial_package_spec.to_string();
        }
        std::cerr << "Resolution failed. Error: " << result.error_message << std::endl;
    }
    return result;
}

bool DependencyResolver::resolve_recursive(
    const PackageSpec& current_spec,
    std::map<std::string, PackageInfo>& shared_packages_to_install_map,
    std::set<std::string> visited_on_current_path,
    std::string& shared_error_accumulator) {

    std::string current_spec_id = current_spec.name + "@" + current_spec.version_requirement;
    if (g_verbose_output) {
        std::cout << "[Thread " << std::this_thread::get_id() << "] resolve_recursive for: " << current_spec_id << std::endl;
    }

    if (visited_on_current_path.count(current_spec_id)) {
        if (g_verbose_output) {
            std::cout << "[Thread " << std::this_thread::get_id() << "] Cycle detected for " << current_spec_id << " on current path. Path: [ ";
            for(const auto& p : visited_on_current_path) { std::cout << p << " -> "; }
            std::cout << current_spec_id << " ]" << std::endl;
        }
        return true;
    }
    visited_on_current_path.insert(current_spec_id);

    PackageInfo package_info = fetch_and_parse_package_info(current_spec);

    if (package_info.resolved_version.empty() || package_info.tarball_url.empty()) {
        std::string error_msg = "Could not retrieve valid package info for " + current_spec_id;
        if (g_verbose_output) {
            std::cout << "[Thread " << std::this_thread::get_id() << "] " << error_msg << std::endl;
        }
        {
            std::lock_guard<std::mutex> lock(resolver_mutex_);
            if (!shared_error_accumulator.empty()) shared_error_accumulator += "; ";
            shared_error_accumulator += error_msg;
        }
        return false;
    }

    std::string resolved_package_key = package_info.name + "@" + package_info.resolved_version;

    {
        std::lock_guard<std::mutex> lock(resolver_mutex_);
        if (shared_packages_to_install_map.count(resolved_package_key)) {
            if (g_verbose_output) {
                std::cout << "[Thread " << std::this_thread::get_id() << "] Package " << resolved_package_key
                          << " (from " << current_spec_id << ") already resolved globally. Skipping dependencies." << std::endl;
            }
            return true;
        }
        shared_packages_to_install_map[resolved_package_key] = package_info;
        if (g_verbose_output) {
            std::cout << "[Thread " << std::this_thread::get_id() << "] Added to global install map: " << resolved_package_key
                      << " (from " << current_spec_id << ")" << std::endl;
        }
    }

    if (!package_info.dependencies.empty()) {
        std::vector<std::future<bool>> dependency_futures;
        if (g_verbose_output) {
            std::cout << "[Thread " << std::this_thread::get_id() << "] Queueing " << package_info.dependencies.size()
                      << " dependencies for " << resolved_package_key << std::endl;
        }

        for (const auto& dep_pair : package_info.dependencies) {
            PackageSpec spec_for_async(dep_pair.first, dep_pair.second);

            dependency_futures.push_back(
                std::async(std::launch::async,
                    [this, captured_spec = spec_for_async, path_copy = visited_on_current_path,
                     &map_ref = shared_packages_to_install_map, &err_ref = shared_error_accumulator]() mutable {
                        return this->resolve_recursive(
                            captured_spec,
                            map_ref,
                            path_copy,
                            err_ref
                        );
                    }
                )
            );
        }

        bool all_deps_resolved = true;
        for (size_t i = 0; i < dependency_futures.size(); ++i) {
            if (!dependency_futures[i].get()) {
                all_deps_resolved = false;
            }
        }

        if (!all_deps_resolved) {
            if (g_verbose_output) {
                std::cout << "[Thread " << std::this_thread::get_id() << "] Failed to resolve one or more dependencies for " << resolved_package_key << std::endl;
            }
            return false;
        }
    }

    if (g_verbose_output) {
        std::cout << "[Thread " << std::this_thread::get_id() << "] Successfully resolved branch for: " << current_spec_id
                  << " -> " << resolved_package_key << std::endl;
    }
    return true;
}

PackageInfo DependencyResolver::fetch_and_parse_package_info(const PackageSpec& spec) {
    std::string version_to_fetch = spec.version_requirement;
    if (version_to_fetch.empty()) {
        version_to_fetch = "latest";
    }
    if (version_to_fetch != "latest" && (version_to_fetch.find('^') != std::string::npos ||
                                       version_to_fetch.find('~') != std::string::npos ||
                                       version_to_fetch.find('x') != std::string::npos ||
                                       version_to_fetch.find('*') != std::string::npos ||
                                       version_to_fetch.find('>') != std::string::npos ||
                                       version_to_fetch.find('<') != std::string::npos)) {
        if (g_verbose_output) {
            std::cout << "[Thread " << std::this_thread::get_id() << "] Detected version range \"" << spec.version_requirement
                      << "\" for " << spec.name << ". Defaulting to 'latest' for now (simplification)." << std::endl;
        }
        version_to_fetch = "latest";
    }

    std::string registry_url = "https://registry.npmjs.org/" + spec.name + "/" + version_to_fetch;
    if (g_verbose_output) {
        std::cout << "[Thread " << std::this_thread::get_id() << "] Fetching package metadata from: " << registry_url << std::endl;
    }

    std::string cache_key = spec.name + "@" + version_to_fetch;
    {
        std::lock_guard<std::mutex> lock(resolver_mutex_);
        if (package_cache_.count(cache_key)) {
            if (g_verbose_output) {
                std::cout << "[Thread " << std::this_thread::get_id() << "] Cache hit for " << cache_key << std::endl;
            }
            return package_cache_[cache_key];
        }
    }

    std::optional<std::string> response_opt = http_client_.get(registry_url);

    if (!response_opt) {
        std::cerr << "[Thread " << std::this_thread::get_id() << "] HTTP client failed to fetch package data for " << spec.to_string() << " from " << registry_url << std::endl;
        return PackageInfo();
    }

    if (g_verbose_output) {
        std::cout << "[Thread " << std::this_thread::get_id() << "] HTTP response for " << spec.to_string() << ": " << (*response_opt).substr(0, 200) << "..." << std::endl;
    }

    JsonData data = JsonParser::try_parse(*response_opt);

    if (data.is_null() || data.is_discarded()) {
        std::cerr << "[Thread " << std::this_thread::get_id() << "] Failed to parse JSON response for " << spec.to_string() << ". Response was: " << (*response_opt).substr(0, 200) << "..." << std::endl;
        return PackageInfo();
    }

    if (data.contains("error") && data["error"].is_string()) {
        std::cerr << "[Thread " << std::this_thread::get_id() << "] Registry error for " << spec.to_string() << ": " << data["error"].get<std::string>() << std::endl;
        return PackageInfo();
    }

    PackageInfo info;
    info.name = spec.name;
    if (data.contains("version") && data["version"].is_string()) {
        info.resolved_version = data["version"].get<std::string>();
    }
    if (data.contains("dist") && data["dist"].is_object() &&
        data["dist"].contains("tarball") && data["dist"]["tarball"].is_string()) {
        info.tarball_url = data["dist"]["tarball"].get<std::string>();
    }

    if (data.contains("dependencies") && data["dependencies"].is_object()) {
        for (auto& [dep_name, dep_ver_req_json] : data["dependencies"].items()) {
            if (dep_ver_req_json.is_string()) {
                info.dependencies[dep_name] = dep_ver_req_json.get<std::string>();
            }
        }
    }

    if (info.resolved_version.empty() || info.tarball_url.empty()){
        std::cerr << "[Thread " << std::this_thread::get_id() << "] Could not extract all required fields (version, tarball URL) for " << spec.to_string()
                  << " from JSON. Name: '" << info.name << "', Resolved: '" << info.resolved_version
                  << "', Tarball: '" << info.tarball_url << "'." << std::endl;
    } else {
        if (g_verbose_output) {
            std::cout << "[Thread " << std::this_thread::get_id() << "] Successfully fetched and parsed info for " << spec.name
                      << " (requested " << version_to_fetch
                      << ") -> resolved to " << info.name << "@" << info.resolved_version << std::endl;
        }
        {
            std::lock_guard<std::mutex> lock(resolver_mutex_);
            package_cache_[cache_key] = info;
        }
    }
    return info;
}

} // namespace jpm
