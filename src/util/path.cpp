#include "path.h"

#include <boost/algorithm/string/predicate.hpp>

static constexpr const char* kDefaultXBE = "default.xbe";

bool SplitXBEPath(const std::string& path, std::string& dir, std::string& xbe) {
  if (path.empty()) {
    return false;
  }

  dir.clear();
  xbe.clear();

  bool has_xbe = boost::algorithm::ends_with(path, ".xbe");

  auto last_slash = path.rfind('\\');
  if (last_slash == std::string::npos) {
    if (has_xbe) {
      dir = "\\";
      xbe = path;
      return true;
    }

    dir = path + "\\";
    xbe = kDefaultXBE;
    return true;
  }

  if (!has_xbe) {
    if (last_slash == path.size() - 1) {
      dir = path;
    } else {
      dir = path + "\\";
    }
    xbe = kDefaultXBE;

    return true;
  }

  dir = path.substr(0, last_slash + 1);
  xbe = path.substr(last_slash + 1);

  return true;
}