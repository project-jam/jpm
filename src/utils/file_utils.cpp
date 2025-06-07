#include "utils/file_utils.h"
#include "jpm_config.h" // For g_verbose_output
#include <iostream> 
#include <sys/stat.h> 
#include <cerrno>     
#include <cstring>    


namespace jpm {
namespace FileUtils {

bool path_exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool create_directory_recursively(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    if (path_exists(path)) {
        struct stat info;
        if (stat(path.c_str(), &info) == 0) {
            if (S_ISDIR(info.st_mode)) {
                return true; // Already exists as a directory
            } else {
                std::cerr << "Error: Path " << path << " exists but is not a directory." << std::endl;
                return false;
            }
        }
        std::cerr << "Error stating existing path " << path << ": " << strerror(errno) << std::endl;
        return false;
    }

    size_t last_slash = 0;
    while ((last_slash = path.find_first_of("/\\", last_slash + 1)) != std::string::npos) {
        std::string current_path_segment = path.substr(0, last_slash);
        if (!current_path_segment.empty() && !path_exists(current_path_segment)) {
            #if defined(_WIN32)
                if (_mkdir(current_path_segment.c_str()) != 0 && errno != EEXIST) {
            #else
                if (mkdir(current_path_segment.c_str(), 0755) != 0 && errno != EEXIST) {
            #endif
                std::cerr << "Error creating directory " << current_path_segment << ": " << strerror(errno) << std::endl;
                return false;
                }
        }
    }

    #if defined(_WIN32)
        if (_mkdir(path.c_str()) != 0 && errno != EEXIST) {
    #else
        if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
    #endif
        std::cerr << "Error creating directory " << path << ": " << strerror(errno) << std::endl;
        return false;
    }

    if (g_verbose_output) {
        std::cout << "Successfully created directory: " << path << std::endl;
    }
    return true;
}

} // namespace FileUtils
} // namespace jpm
