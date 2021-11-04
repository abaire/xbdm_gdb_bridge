#include "memory_region.h"

bool MemoryRegion::Contains(uint32_t address, uint32_t length) const {
  return address >= start && (address + length) < end;
}
