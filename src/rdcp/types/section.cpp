#include "section.h"

std::ostream& operator<<(std::ostream& os, const Section& s) {
  return os << "Section " << s.name << " base_address: 0x" << std::hex
            << std::setfill('0') << std::setw(8) << s.base_address
            << " size: " << std::dec << s.size << " index: " << s.index
            << " flags: 0x" << std::hex << s.flags;
}
