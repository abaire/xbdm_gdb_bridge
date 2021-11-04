#include "module.h"

#include <utility>

Module::Module(std::string name, uint32_t baseAddress, uint32_t size,
               uint32_t checksum, uint32_t timestamp, bool isTls, bool isXbe)
    : name(std::move(name)),
      base_address(baseAddress),
      size(size),
      checksum(checksum),
      timestamp(timestamp),
      is_tls(isTls),
      is_xbe(isXbe) {}

std::ostream &operator<<(std::ostream &os, const Module &m) {
  return os << "Module " << m.name << " base_address: 0x" << std::hex
            << std::setfill('0') << std::setw(8) << m.base_address
            << " size: " << std::dec << m.size << " checksum: 0x" << std::hex
            << m.checksum << " timestamp: 0x" << m.timestamp
            << " is_tls: " << m.is_tls << " is_xbe: " << m.is_xbe;
}
