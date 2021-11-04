#include "module.h"

std::ostream &operator<<(std::ostream &os, const Module &m) {
  return os << "Module " << m.name << " base_address: 0x" << std::hex
            << std::setfill('0') << std::setw(8) << m.base_address
            << " size: " << std::dec << m.size << " checksum: 0x" << std::hex
            << m.checksum << " timestamp: 0x" << m.timestamp
            << " is_tls: " << m.is_tls << " is_xbe: " << m.is_xbe;
}
