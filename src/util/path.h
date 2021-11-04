#ifndef XBDM_GDB_BRIDGE_PATH_H
#define XBDM_GDB_BRIDGE_PATH_H

#include <string>

bool SplitXBEPath(const std::string &path, std::string &dir, std::string &xbe);

#endif  // XBDM_GDB_BRIDGE_PATH_H
