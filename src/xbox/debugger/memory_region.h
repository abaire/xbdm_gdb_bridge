#ifndef XBDM_GDB_BRIDGE_MEMORY_REGION_H
#define XBDM_GDB_BRIDGE_MEMORY_REGION_H

#include <cinttypes>
#include <string>

#include "rdcp/xbdm_requests.h"

struct MemoryRegion {
  explicit MemoryRegion(const WalkMem::Region &region) {
    start = region.base;
    size = region.size;
    protect = region.protect;
    flags = region.flags;

    end = start + size;
  }

  bool Contains(uint32_t start, uint32_t size = 1) const;

  uint32_t start;
  uint32_t end;
  uint32_t size;
  uint32_t protect;
  std::set<std::string> flags;
};

#endif  // XBDM_GDB_BRIDGE_MEMORY_REGION_H
