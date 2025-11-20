#include "config_path.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace config_path {
fs::path GetConfigDirectoryPath(const std::string& app_name) {
  fs::path base_path;

#if defined(__linux__) || defined(__unix__)
  const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
  if (xdg_config) {
    base_path = fs::path(xdg_config);
  } else {
    // Fallback to ~/.config
    const char* home = std::getenv("HOME");
    if (home) {
      base_path = fs::path(home) / ".config";
    } else {
      // Extreme fallback if HOME is missing (rare)
      base_path = fs::current_path();
    }
  }

#elif defined(__APPLE__)

  const char* home = std::getenv("HOME");
  if (home) {
    base_path = fs::path(home) / "Library" / "Application Support";
  } else {
    base_path = fs::current_path();
  }

#else

#error "Unsupported platform"

#endif

  return base_path / app_name;
}

fs::path GetConfigFilePath(const std::string& app_name,
                           const std::string& filename) {
  return GetConfigDirectoryPath(app_name) / filename;
}

}  // namespace config_path
