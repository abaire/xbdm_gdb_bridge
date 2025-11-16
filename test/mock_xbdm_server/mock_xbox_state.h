#ifndef XBDM_GDB_BRIDGE_TEST_MOCK_XBDM_SERVER_MOCK_XBOX_STATE_H_
#define XBDM_GDB_BRIDGE_TEST_MOCK_XBDM_SERVER_MOCK_XBOX_STATE_H_

#include <atomic>
#include <boost/algorithm/string.hpp>
#include <functional>
#include <map>
#include <string>

namespace xbdm_gdb_bridge::testing {

// Represents a memory region in the simulated Xbox
struct MemoryRegion {
  uint32_t base_address;
  std::vector<uint8_t> data;
  uint32_t protect{0x00020004};
};

// Represents a thread in the simulated Xbox
struct SimulatedThread {
  uint32_t id;

  bool suspended{false};
  uint32_t priority{9};
  ;
  uint32_t start{0x00060000};
  uint32_t base{0xd0000000};
  uint32_t tls_base{0xd0001000};
  uint32_t limit{0xd0200000};

  union {
    uint64_t timestamp;
    struct {
      uint32_t hi{0x01dc5690};
      uint32_t low{0xaa2345f0};
    };
  } create;

  std::optional<uint32_t> ebp;
  std::optional<uint32_t> esp;
  std::optional<uint32_t> eip;
  std::optional<uint32_t> eflags;
  std::optional<uint32_t> eax;
  std::optional<uint32_t> ebx;
  std::optional<uint32_t> ecx;
  std::optional<uint32_t> edx;
  std::optional<uint32_t> edi;
  std::optional<uint32_t> esi;
  std::optional<uint32_t> cr0_npx_state;

 public:
  void SetRegister(const std::string& reg_name, uint32_t value) {}

  void ClearRegister(const std::string& reg_name) {}

 private:
  std::optional<uint32_t>& GetRegister(const std::string& reg_name) {
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
};

// Represents a breakpoint
struct Breakpoint {
  uint32_t address;
  bool enabled = true;
  bool hardware = false;  // false = software breakpoint
};

// Represents a loaded module/XBE
struct Module {
  std::string name;
  uint32_t base_address;
  uint32_t size;
  uint32_t timestamp;
  uint32_t checksum;
};

struct MockXboxState {
  std::string xbox_name = "XBOX-TEST";
  std::string xbox_version = "1.0.5838.1";

  std::atomic<bool> execution_running{false};

  // Simulate non-debugable processes.
  bool is_debugable_{true};

  std::map<uint32_t, MemoryRegion> memory_regions;

  std::map<uint32_t, SimulatedThread> threads;
  uint32_t next_thread_id = 1;
  uint32_t current_thread_id = 0;

  std::map<uint32_t, Breakpoint> breakpoints;

  std::map<std::string, Module> modules;
};

}  // namespace xbdm_gdb_bridge::testing

#endif  // XBDM_GDB_BRIDGE_TEST_MOCK_XBDM_SERVER_MOCK_XBOX_STATE_H_
