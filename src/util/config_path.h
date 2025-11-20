#ifndef XBDM_GDB_BRIDGE_CONFIG_PATH_H
#define XBDM_GDB_BRIDGE_CONFIG_PATH_H

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace config_path {

fs::path GetConfigDirectoryPath(const std::string& app_name);

fs::path GetConfigFilePath(const std::string& app_name,
                           const std::string& filename);

}  // namespace config_path

#endif  // XBDM_GDB_BRIDGE_CONFIG_PATH_H
