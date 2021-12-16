#ifndef XBDM_GDB_BRIDGE_MEMORY_REGION_H
#define XBDM_GDB_BRIDGE_MEMORY_REGION_H

#include <cinttypes>
#include <set>
#include <string>

class RDCPMapResponse;

struct MemoryRegion {
  enum ProtectionFlags {
    PAGE_NOACCESS = 0x01,
    PAGE_READONLY = 0x02,
    PAGE_READWRITE = 0x04,
    PAGE_WRITECOPY = 0x08,
    PAGE_EXECUTE = 0x10,
    PAGE_EXECUTE_READ = 0x20,
    PAGE_EXECUTE_READWRITE = 0x40,
    PAGE_EXECUTE_WRITECOPY = 0x80,
    PAGE_GUARD = 0x100,
    PAGE_NOCACHE = 0x200,
    PAGE_WRITECOMBINE = 0x400,
    PAGE_OLD_VIDEO = 0x800,
    MEM_COMMIT = 0x1000,
    MEM_RESERVE = 0x2000,
    MEM_DECOMMIT = 0x4000,
    MEM_RELEASE = 0x8000,
    MEM_FREE = 0x10000,
    MEM_PRIVATE = 0x20000,
    MEM_MAPPED = 0x40000,
    MEM_RESET = 0x80000,
    MEM_TOP_DOWN = 0x100000,
    MEM_NOZERO = 0x800000,
    MEM_LARGE_PAGES = 0x20000000,
    MEM_4MB_PAGES = 0x80000000,
  };

  MemoryRegion() = default;
  MemoryRegion(uint32_t start, uint32_t size, uint32_t protect,
               std::set<std::string> flags);
  explicit MemoryRegion(const RDCPMapResponse &parsed);

  [[nodiscard]] bool Contains(uint32_t start, uint32_t size = 1) const;
  [[nodiscard]] bool IsWritable() const;

  uint32_t start;  // The first address within this MemoryRegion.
  uint32_t end;    // The first address beyond this MemoryRegion.
  uint32_t size;
  uint32_t protect;
  std::set<std::string> flags;
};

#endif  // XBDM_GDB_BRIDGE_MEMORY_REGION_H
