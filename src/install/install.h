#ifndef JPM_INSTALL_COMMAND_H // Keeping guard name for now, can be changed
#define JPM_INSTALL_COMMAND_H

#include <string>
#include <vector>
#include "package/dependency_resolver.h" // For DependencyResolver member
#include "package/tarball_handler.h" // For TarballHandler member

namespace jpm {

class InstallCommand {
public:
    InstallCommand();
    // Takes a list of package specifications like "lodash" or "react@17.0.0"
    void execute(const std::vector<std::string>& packages_to_install_args);

private:
    DependencyResolver resolver_;
    TarballHandler tarball_handler_;

};

} // namespace jpm

#endif // JPM_INSTALL_COMMAND_H
