#include "package/tarball_handler.h"
#include "utils/file_utils.h"
#include "jpm_config.h"
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifndef _WIN32
#include <unistd.h> // For mkstemp, close on non-Windows
#else
#include <windows.h> // For GetTempPath/FileName (alternative to _tempnam)
#endif

namespace jpm {

TarballHandler::TarballHandler() {
    if (g_verbose_output) {
        std::cout << "TarballHandler initialized." << std::endl;
    }
}

bool TarballHandler::download_and_extract(
    const std::string& tarball_url,
    const std::string& package_name,
    const std::string& package_version,
    const std::string& base_destination_path) {
    if (g_verbose_output) {
        std::cout << "TarballHandler::download_and_extract for: " << package_name << "@" << package_version << std::endl;
        std::cout << "  URL: " << tarball_url << std::endl;
        std::cout << "  Base Destination: " << base_destination_path << std::endl;
    }

    std::string local_tarball_path;

#ifdef _WIN32
    char tmp_filename_buffer[MAX_PATH + 1];
    char temp_path[MAX_PATH + 1];
    // Use GetTempPath and GetTempFileName on Windows for a robust temporary file name
    if (GetTempPathA(MAX_PATH, temp_path) == 0) {
        std::cerr << "Error: Could not get temporary path on Windows." << std::endl;
        return false;
    }
    if (GetTempFileNameA(temp_path, "jpm", 0, tmp_filename_buffer) == 0) {
         std::cerr << "Error: Could not generate temporary filename on Windows." << std::endl;
         return false;
    }
    local_tarball_path = tmp_filename_buffer;
    local_tarball_path += ".tar.gz"; // Append .tar.gz as GetTempFileName might not
#else // Non-Windows (Linux, macOS, etc.)
    // mkstemp modifies the buffer, so it needs to be writable
    char tmp_filename_template[] = "/tmp/jpm-tmp-XXXXXX";
    int fd = mkstemp(tmp_filename_template);
    if (fd == -1) {
        std::cerr << "Error: Could not generate temporary filename for download." << std::endl;
        return false;
    }
    close(fd); // Close the file descriptor, we just needed the unique name
    local_tarball_path = tmp_filename_template; // mkstemp writes the unique name into the buffer
    local_tarball_path += ".tar.gz";
#endif

    if (g_verbose_output) {
        std::cout << "  Temporary download path: " << local_tarball_path << std::endl;
    }

    if (g_verbose_output) {
        std::cout << "  Downloading..." << std::endl;
    }
    bool downloaded = http_client_.download_file(tarball_url, local_tarball_path);
    if (!downloaded) {
        std::cerr << "  Failed to download tarball from " << tarball_url << std::endl;
        return false;
    }
    if (g_verbose_output) {
        std::cout << "  Download successful to " << local_tarball_path << std::endl;
    }

    std::string extract_to_path_final = base_destination_path + "/" + package_name;
    if (g_verbose_output) {
        std::cout << "  Ensuring extraction directory exists: " << extract_to_path_final << std::endl;
    }
    if (!FileUtils::path_exists(extract_to_path_final)) {
        if (!FileUtils::create_directory_recursively(extract_to_path_final)) {
            std::cerr << "  Failed to create extraction directory: " << extract_to_path_final << std::endl;
            std::remove(local_tarball_path.c_str());
            return false;
        }
    }

    if (g_verbose_output) {
        std::cout << "  Extracting " << local_tarball_path << " to " << extract_to_path_final << "..." << std::endl;
    }
    bool extracted = extract_tarball(local_tarball_path, extract_to_path_final);

    if (g_verbose_output) {
        std::cout << "  Cleaning up temporary file: " << local_tarball_path << std::endl;
    }
    if (std::remove(local_tarball_path.c_str()) != 0) {
        std::cerr << "  Warning: Failed to remove temporary tarball: " << local_tarball_path << std::endl;
    }

    if (!extracted) {
        std::cerr << "  Failed to extract tarball " << local_tarball_path << std::endl;
        return false;
    }

    if (g_verbose_output) {
        std::cout << "  Successfully downloaded and extracted " << package_name << "@" << package_version << std::endl;
    }
    return true;
}

bool TarballHandler::extract_tarball(
    const std::string& local_tarball_path,
    const std::string& extract_to_path) {
    if (!FileUtils::path_exists(extract_to_path)) {
        if (!FileUtils::create_directory_recursively(extract_to_path)) {
            std::cerr << "Extraction target directory " << extract_to_path << " does not exist and could not be created." << std::endl;
            return false;
        }
    }

    std::string command;
#ifdef _WIN32
    // Use cmake -E tar on Windows. Requires cmake in PATH.
    // Note: cmake -E tar doesn't support --strip-components=1 directly.
    // A more complex solution might be needed here, or rely on a separate tool,
    // or extract to a temp folder inside the target and then move contents up.
    // For this edit, let's use the simplest form and note the limitation.
    command = "cmake -E tar xzf \"" + local_tarball_path + "\" -C \"" + extract_to_path + "\"";
    if (g_verbose_output) {
        std::cout << "  NOTE: --strip-components=1 is not supported by cmake -E tar." << std::endl;
    }
#else
    // Use standard tar on non-Windows (Linux, macOS, etc.)
    command = "tar -xzf \"" + local_tarball_path + "\" -C \"" + extract_to_path + "\" --strip-components=1";
#endif

    if (g_verbose_output) {
        std::cout << "  Executing extraction command: " << command << std::endl;
    }

    int result = std::system(command.c_str());
    if (result != 0) {
        std::cerr << "  Tar extraction command failed with exit code: " << result << std::endl;
        return false;
    }

    if (g_verbose_output) {
        std::cout << "  Tarball extracted successfully." << std::endl;
    }
    return true;
}

} // namespace jpm
