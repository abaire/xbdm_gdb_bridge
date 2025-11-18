#ifndef XBDM_GDB_BRIDGE_THREAD_DEBUG_UTIL_H
#define XBDM_GDB_BRIDGE_THREAD_DEBUG_UTIL_H

#include <string>

namespace debug {

void SetCurrentThreadName(const std::string& name);

std::string GetCurrentThreadName();

}  // namespace debug

#endif  // XBDM_GDB_BRIDGE_THREAD_DEBUG_UTIL_H
