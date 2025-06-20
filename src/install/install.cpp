#include "install/install.h"
#include "package/package_spec.h"
#include "package/dependency_resolver.h"
#include "utils/file_utils.h"
#include "utils/ui_utils.h"
#include "jpm_config.h"
#include <iostream>
#include <vector>
#include <string>
#include <future>
#include <thread>
#include <algorithm>
#include <chrono> // For timing

namespace jpm {

InstallCommand::InstallCommand() {
    if (g_verbose_output) {
        std::cout << "InstallCommand initialized." << std::endl;
    }
}

void InstallCommand::execute(const std::vector<std::string>& packages_to_install_args) {
    if (packages_to_install_args.empty()) {
        std::cerr << "No packages specified for install command." << std::endl;
        return;
    }

    auto overall_start_time = std::chrono::high_resolution_clock::now();

    if (g_verbose_output) {
        std::cout << "Install command executing for: ";
        for (size_t i = 0; i < packages_to_install_args.size(); ++i) {
            std::cout << packages_to_install_args[i] << (i + 1 < packages_to_install_args.size() ? ", " : "");
        }
        std::cout << std::endl;
    }

    UIUtils::ProgressSpinner spinner;

    // Ensure base dir
    std::string destination_base = "./node_modules";
    if (!jpm::FileUtils::path_exists(destination_base)) {
        if (g_verbose_output) {
            std::cout << "Creating directory: " << destination_base << std::endl;
        }
        if (!jpm::FileUtils::create_directory_recursively(destination_base)) {
            std::cerr << "Failed to create installation directory: " << destination_base << ". Aborting installation." << std::endl;
            return;
        }
    }

    for (const auto& pkg_arg : packages_to_install_args) {
        auto single_pkg_start = std::chrono::high_resolution_clock::now();

        // parse name/version
        std::string package_name = pkg_arg;
        std::string version_requirement = "latest";
        size_t at = pkg_arg.find('@');
        if (at != std::string::npos && at > 0) {
            version_requirement = pkg_arg.substr(at + 1);
            if (version_requirement.empty()) version_requirement = "latest";
            package_name = pkg_arg.substr(0, at);
        }

        spinner.start("Preparing " + package_name + "...");

        PackageSpec spec(package_name, version_requirement);
        if (g_verbose_output) {
            std::cout << "-----------------------------------------------------\n"
                      << "Resolving dependencies for: " << spec.to_string() << std::endl;
        } else {
            spinner.update_message("Resolving " + spec.to_string() + "...");
        }

        // resolution spinner thread
        std::atomic<bool> resolve_done{false};
        std::thread resolve_spinner([&](){
            while (!resolve_done) {
                spinner.tick();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });

        auto resolve_start = std::chrono::high_resolution_clock::now();
        ResolutionResult result = resolver_.resolve(spec);
        resolve_done = true;
        resolve_spinner.join();
        auto resolve_end = std::chrono::high_resolution_clock::now();
        if (g_verbose_output) {
            std::chrono::duration<double> d = resolve_end - resolve_start;
            std::cout << "Resolution took: " << d.count() << "s\n";
        }

        if (!result.success) {
            std::cerr << "Failed to resolve " << spec.to_string() << ". "
                      << (!result.error_message.empty() ? result.error_message : "") << std::endl;
            spinner.stop(false, "Resolution failed for " + spec.to_string());
            continue;
        }

        if (result.packages_to_install.empty()) {
            spinner.stop(true, "Already up-to-date: " + spec.to_string());
        } else {
            if (g_verbose_output) {
                std::cout << "Installing " << result.packages_to_install.size() << " packages for "
                          << spec.to_string() << "...\n";
            } else {
                spinner.update_message("Installing " + spec.to_string() + "...");
            }

            // **Install-phase spinner thread**
            std::atomic<bool> install_done{false};
            std::thread install_spinner([&](){
                while (!install_done) {
                    spinner.tick();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            });

            // launch async downloads
            std::vector<std::future<bool>> futures;
            for (auto& pkg_info : result.packages_to_install) {
                futures.push_back(std::async(std::launch::async, [=]() {
                    return tarball_handler_.download_and_extract(
                        pkg_info.tarball_url,
                        pkg_info.name,
                        pkg_info.resolved_version,
                        destination_base
                    );
                }));
            }

            // wait for all
            bool all_ok = true;
            for (auto& f : futures) {
                if (!f.get()) all_ok = false;
            }

            // stop spinner
            install_done = true;
            install_spinner.join();

            if (all_ok) {
                spinner.stop(true, "Installed " + spec.to_string());
            } else {
                spinner.stop(false, "Installation failed for " + spec.to_string());
            }
        }

        if (g_verbose_output) {
            auto single_pkg_end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> total = single_pkg_end - single_pkg_start;
            std::cout << "Total time for " << spec.to_string() << ": " << total.count() << "s\n"
                      << "-----------------------------------------------------" << std::endl;
        }
    }

    auto overall_end = std::chrono::high_resolution_clock::now();
    if (g_verbose_output) {
        std::chrono::duration<double> tot = overall_end - overall_start_time;
        std::cout << "Total jpm execution time: " << tot.count() << "s" << std::endl;
    }
}

} // namespace jpm
