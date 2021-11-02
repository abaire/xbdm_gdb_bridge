#ifndef XBDM_GDB_BRIDGE_VECTOR_H
#define XBDM_GDB_BRIDGE_VECTOR_H

#include <vector>

std::vector<char> &operator+=(std::vector<char> &dest, const char *str) {
  assert(str);
  dest.insert(dest.end(), str, str + strlen(str));
  return dest;
}

#endif  // XBDM_GDB_BRIDGE_VECTOR_H
