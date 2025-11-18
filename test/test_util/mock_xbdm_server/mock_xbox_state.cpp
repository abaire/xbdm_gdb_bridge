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

}  // namespace xbdm_gdb_bridge::testing
