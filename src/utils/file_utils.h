#ifndef JPM_FILE_UTILS_H
#define JPM_FILE_UTILS_H

#include <string>
#include <vector>

namespace jpm {
namespace FileUtils {

bool create_directory_recursively(const std::string& path);
bool path_exists(const std::string& path);
// Add more utilities as needed: delete_directory, read_file, write_file etc.

} // namespace FileUtils
} // namespace jpm

#endif // JPM_FILE_UTILS_H
