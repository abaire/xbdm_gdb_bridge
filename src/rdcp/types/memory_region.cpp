#include "memory_region.h"

#include <utility>

#include "rdcp/rdcp_processed_request.h"

MemoryRegion::MemoryRegion(uint32_t start, uint32_t size, uint32_t protect,
                           std::set<std::string> flags)
    : start(start), size(size), protect(protect), flags(std::move(flags)) {
  end = start + size;
}

MemoryRegion::MemoryRegion(const RDCPMapResponse& parsed) {
  start = parsed.GetUInt32("base");
  size = parsed.GetUInt32("size");
  protect = parsed.GetUInt32("protect");
  flags = parsed.valueless_keys;
  end = start + size;
}

bool MemoryRegion::Contains(uint32_t address, uint32_t length) const {
  return address >= start && (address + length) < end;
}

bool MemoryRegion::IsWritable() const {
  if (protect & PAGE_READWRITE) {
    return true;
  }
  if (protect & PAGE_WRITECOPY) {
    return true;
  }
  if (protect & PAGE_EXECUTE) {
    return true;
  }
  if (protect & PAGE_EXECUTE_READ) {
    return true;
  }
  if (protect & PAGE_EXECUTE_READWRITE) {
    return true;
  }
  if (protect & PAGE_EXECUTE_WRITECOPY) {
    return true;
  }

  return false;
}