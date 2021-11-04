#ifndef XBDM_GDB_BRIDGE_OPTIONAL_H
#define XBDM_GDB_BRIDGE_OPTIONAL_H

#include <optional>

template <class OS, class T>
OS& operator<<(OS& os, std::optional<T> const& v) {
  if (v) {
    os << "Just(" << *v << ')';
  } else {
    os << "Nothing";
  }

  return os;
}

#endif  // XBDM_GDB_BRIDGE_OPTIONAL_H
