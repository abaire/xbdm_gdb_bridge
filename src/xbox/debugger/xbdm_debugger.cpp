#include "xbdm_debugger.h"

#include <boost/log/trivial.hpp>

#include "util/path.h"
#include "xbox/xbdm_context.h"

XBDMDebugger::XBDMDebugger(std::shared_ptr<XBDMContext> context)
    : xbdm_context_(std::move(context)) {}

bool XBDMDebugger::Attach() { return false; }

void XBDMDebugger::Shutdown() {}

bool XBDMDebugger::DebugXBE(const std::string& path) {
  return DebugXBE(path, "");
}

bool XBDMDebugger::DebugXBE(const std::string& path,
                            const std::string& command_line) {
  uint32_t flags = 0;

  std::string xbe_dir;
  std::string xbe_name;
  if (!SplitXBEPath(path, xbe_dir, xbe_name)) {
    BOOST_LOG_TRIVIAL(error) << "Invalid XBE path '" << path << "'";
    return false;
  }

  auto request =
      std::make_shared<LoadOnBootTitle>(xbe_name, xbe_dir, command_line);

  return false;
}
