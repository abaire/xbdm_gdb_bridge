#include "mock_xbox_state.h"

namespace xbdm_gdb_bridge::testing {

void SimulatedThread::SetRegister(const std::string& reg_name, uint32_t value) {
  auto& reg = GetRegister(reg_name);

  reg = value;
}

void SimulatedThread::ClearRegister(const std::string& reg_name) {
  auto& reg = GetRegister(reg_name);
  reg.reset();
}

bool SimulatedThread::ContainsAddress(uint32_t address) const {
  return address >= base && address < (base + limit);
}

std::optional<uint32_t>& SimulatedThread::GetRegister(
    const std::string& reg_name) {
  auto name = boost::algorithm::to_lower_copy(reg_name);
  if (name == "ebp") {
    return ebp;
  }
  if (name == "esp") {
    return esp;
  }
  if (name == "eip") {
    return eip;
  }
  if (name == "eflags") {
    return eflags;
  }
  if (name == "eax") {
    return eax;
  }
  if (name == "ebx") {
    return ebx;
  }
  if (name == "ecx") {
    return ecx;
  }
  if (name == "edx") {
    return edx;
  }
  if (name == "edi") {
    return edi;
  }
  if (name == "esi") {
    return esi;
  }
  if (name == "cr0_npx_state") {
    return cr0_npx_state;
  }

  assert(!"Invalid register name");
}

void SimulatedThread::Reset() {
  eip = start;

  ebp = tls_base;
  esp = ebp;

  eax = 0;
  ebx = 1;
  ecx = 2;
  edx = 3;

  edi = 0xF00D;
  esi = 0xFEED;

  eflags = 0xFFFFFFFF;
  cr0_npx_state = 0x00CAFE00;
}

void MockXboxState::ResetThreadStates() {
  for (auto& entry : threads) {
    auto& thread = entry.second;
    thread.Reset();
  }
}

bool MockXboxState::ReadVirtualMemory(std::vector<uint8_t>& buffer,
                                      uint32_t address, uint32_t length,
                                      uint8_t fill) {
  buffer.assign(length, fill);

  if (length == 0 || memory_regions.empty()) {
    return false;
  }

  uint32_t current_cursor = address;
  uint32_t request_end = address + length;

  auto it = memory_regions.upper_bound(address);
  if (it != memory_regions.begin()) {
    --it;
  }

  for (; it != memory_regions.end(); ++it) {
    const MemoryRegion& region = it->second;
    uint32_t region_end = region.base_address + region.size;

    // Optimization: If region starts after the request ends, we are done
    // checking this region However, if we haven't finished satisfying the
    // request, this means we hit a hole at the end.
    if (region.base_address >= request_end) {
      break;
    }

    // Skip regions that are entirely before our current cursor
    if (region_end <= current_cursor) {
      continue;
    }

    // Check for a hole between the current cursor and this region's start
    if (region.base_address > current_cursor) {
      return false;
    }

    // Calculate the overlap for the current step
    // We know region.base_address <= current_cursor
    uint32_t chunk_end = std::min(request_end, region_end);
    size_t copy_size = chunk_end - current_cursor;

    size_t src_offset = current_cursor - region.base_address;
    size_t dst_offset = current_cursor - address;

    // Only copy if the region actually has data backing this range
    if (src_offset < region.data.size()) {
      size_t available_bytes = region.data.size() - src_offset;
      size_t actual_copy = std::min((size_t)copy_size, available_bytes);

      std::memcpy(buffer.data() + dst_offset, region.data.data() + src_offset,
                  actual_copy);
    }

    // Advance cursor
    current_cursor += copy_size;

    if (current_cursor == request_end) {
      return true;
    }
  }

  // If we exited the loop, check if we finished the request
  return (current_cursor == request_end);
}

bool MockXboxState::WriteVirtualMemory(uint32_t address,
                                       const std::vector<uint8_t>& data) {
  auto bytes_remaining = data.size();
  if (!bytes_remaining) {
    return true;
  }
  auto start = data.begin();

  auto it = memory_regions.upper_bound(address);
  if (it != memory_regions.begin()) {
    --it;
  }

  for (; it != memory_regions.end(); ++it) {
    auto& region = it->second;
    if (address >= region.base_address &&
        address < region.base_address + region.size) {
      uint32_t offset = address - region.base_address;
      uint32_t bytes_to_write = std::min(static_cast<uint32_t>(bytes_remaining),
                                         region.size - offset);
      std::copy_n(start, bytes_to_write, region.data.begin() + offset);
      bytes_remaining -= bytes_to_write;
      if (!bytes_remaining) {
        break;
      }

      start += bytes_to_write;
      address += bytes_to_write;
    }
  }

  return !bytes_remaining;
}

}  // namespace xbdm_gdb_bridge::testing
