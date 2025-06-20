#include <iostream>
#include <vector>
#include <string>
#include <algorithm> // For std::remove
#include "install/install.h"
#include "js/js.h" // Include the new JSCommand header
#include "jpm_config.h"      // For g_verbose_output

// Define the global verbosity flag
bool g_verbose_output = false;

int main(int argc, char* argv[]) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    // Check for verbosity flag and remove it from args if present
    auto verbose_it = std::find_if(args.begin(), args.end(), [](const std::string& s) {
        return s == "-v" || s == "--verbose";
    });

    if (verbose_it != args.end()) {
        g_verbose_output = true;
        args.erase(verbose_it);
        // If there was another verbose flag (e.g., -v --verbose), remove it too.
        // This is a simple approach; a proper CLI parser would handle this better.
        auto verbose_it2 = std::find_if(args.begin(), args.end(), [](const std::string& s) {
            return s == "-v" || s == "--verbose";
        });
        if (verbose_it2 != args.end()) {
             args.erase(verbose_it2);
        }
    }

    // Check for version flag and print version if present
    auto version_it = std::find_if(args.begin(), args.end(), [](const std::string& s) {
        return s == "--version";
    });

    if (version_it != args.end()) {
        std::cout << "jpm version " << PROJECT_VERSION << std::endl;
        return 0;
    }

    if (args.empty()) {
        std::cerr << "Usage: jpm [-v|--verbose] <command> [args...]\\n";
        std::cerr << "       jpm [-v|--verbose] <js_file> [args...]\\n";
        std::cerr << "Available commands:\n  install <package_name>[@<version>]...\n  run <js_file>\n"; // Restored run command usage
        return 1;
    }

    std::string command = args[0];
    std::vector<std::string> command_args;
    if (args.size() > 1) {
        command_args.assign(args.begin() + 1, args.end());
    }

    if (command == "install") {
        if (command_args.empty()) {
            std::cerr << "Usage: jpm [-v|--verbose] install <package_name>[@<version>]..." << std::endl;
            std::cerr << "Please specify at least one package to install." << std::endl;
            return 1;
        }
        jpm::InstallCommand install_command;
        install_command.execute(command_args);
    } else if (command == "run") { // Handle the explicit 'run' command
        if (command_args.empty()) {
            std::cerr << "Usage: jpm [-v|--verbose] run <js_file>" << std::endl;
            std::cerr << "Please specify a JavaScript file to run." << std::endl;
            return 1;
        }
#ifdef USE_JAVASCRIPTCORE
        jpm::JSCommand js_command;
        js_command.execute({command_args[0]}); // Pass the first argument as the file path
        // Note: Any args after the file path are currently ignored by JSCommand::execute.
        // This might need adjustment if you need to pass arguments to the JS script.
#else
        std::cerr << "Error: JavaScriptCore support is not enabled in this build." << std::endl;
        return 1;
#endif
    } else if (command.size() >= 3 && command.substr(command.size() - 3) == ".js") {
        // Handle direct .js file execution
#ifdef USE_JAVASCRIPTCORE
        if (g_verbose_output) {
            std::cout << "Detected .js file argument. Running script: " << command << std::endl;
        }
        jpm::JSCommand js_command;
        js_command.execute({command}); // Pass the .js file path as the argument
        // Note: Any args after the file path are currently ignored by JSCommand::execute.
        // This might need adjustment if you need to pass arguments to the JS script.
#else
        std::cerr << "Error: JavaScriptCore support is not enabled in this build." << std::endl;
        return 1;
#endif
    } else {
        // Unknown command
        if (g_verbose_output) {
            std::cout << "jpm (Jam Package Manager) - Verbose Mode" << std::endl;
        } else {
            std::cout << "jpm (Jam Package Manager)" << std::endl;
        }
        std::cerr << "Unknown command or file: " << command << std::endl;
        std::cerr << "Usage: jpm [-v|--verbose] <command> [args...]\\n";
        std::cerr << "       jpm [-v|--verbose] <js_file> [args...]\\n";
        std::cerr << "Available commands:\n  install <package_name>[@<version>]...\n  run <js_file>\n"; // Restored run command usage
        return 1;
    }

    return 0;
}
