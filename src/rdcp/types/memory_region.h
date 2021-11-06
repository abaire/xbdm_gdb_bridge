#ifndef XBDM_GDB_BRIDGE_MEMORY_REGION_H
#define XBDM_GDB_BRIDGE_MEMORY_REGION_H

#include <cinttypes>
#include <set>
#include <string>

class RDCPMapResponse;

struct MemoryRegion {
  MemoryRegion() = default;
  MemoryRegion(uint32_t start, uint32_t size, uint32_t protect,
               std::set<std::string> flags);
  explicit MemoryRegion(const RDCPMapResponse &parsed);

  bool Contains(uint32_t start, uint32_t size = 1) const;

  uint32_t start;
  uint32_t end;
  uint32_t size;
  uint32_t protect;
  std::set<std::string> flags;
};

#endif  // XBDM_GDB_BRIDGE_MEMORY_REGION_H
