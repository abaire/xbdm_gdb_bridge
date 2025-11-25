#ifndef XBDM_GDB_BRIDGE_TEST_MOCK_XBDM_SERVER_MOCK_XBOX_STATE_H_
#define XBDM_GDB_BRIDGE_TEST_MOCK_XBDM_SERVER_MOCK_XBOX_STATE_H_

#include <atomic>
#include <boost/algorithm/string.hpp>
#include <functional>
#include <map>
#include <string>

#include "rdcp/types/execution_state.h"

namespace xbdm_gdb_bridge::testing {

// Represents a memory region in the simulated Xbox
struct MemoryRegion {
  uint32_t base_address;
  uint32_t size;
  std::vector<uint8_t> data;
  uint32_t protect{0x00020004};
};

// Represents a thread in the simulated Xbox
struct SimulatedThread {
  uint32_t id;

  bool created{false};
  // Threads may be stopped by XBDM.
  bool stopped{false};
  std::string stop_reason;

  // Threads may be suspended by the OS or via XBDM "suspend" and resumed via
  // "resume".
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
  void SetRegister(const std::string& reg_name, uint32_t value);
  void ClearRegister(const std::string& reg_name);
  bool ContainsAddress(uint32_t address) const;

  void Reset();

 private:
  std::optional<uint32_t>& GetRegister(const std::string& reg_name);
};

struct Breakpoint {
  enum class Type {
    READ,
    WRITE,
    EXECUTE,
  };

  uint32_t address{0};
  Type type{Type::EXECUTE};
};

//! Represents a loaded module/XBE.
struct Module {
  std::string name;
  uint32_t base_address;
  uint32_t size;
  uint32_t timestamp;
  uint32_t checksum;
};

//! Represents a section within an XBE.
struct XbeSection {
  std::string name;
  uint32_t base_address;
  uint32_t size;
  uint32_t index;
  uint32_t flags;
};

struct LoadOnBootInfo {
  std::string name;
  std::string path;
  std::string command_line;
  bool persistent;
};

struct BootActions {
  bool wait_for_debugger{false};
  bool halt{false};
  bool break_at_first_thread{false};
};

struct StopEvents {
  bool first_chance_exception{true};
  bool create_thread{false};
  bool debug_str{false};
  bool stack_trace{false};

  void SetAll() {
    first_chance_exception = true;
    create_thread = true;
    debug_str = true;
    stack_trace = true;
  }

  void ClearAll() {
    first_chance_exception = false;
    create_thread = false;
    debug_str = false;
    stack_trace = false;
  }
};

struct MockXboxState {
  enum class TitleExecutionPhase {
    BOOTING,
    START,
    LOAD_MODULES,
    LOAD_SECTIONS,
    START_FIRST_THREAD,
    START_THREADS,
    RUNNING,
  };

  void ResetThreadStates();
  [[nodiscard]] bool IsStartingUp() const {
    return execution_phase != TitleExecutionPhase::RUNNING;
  }

  /**
   * Reads a block of memory from the simulated Xbox state.
   *
   * If the requested range overlaps with unmapped memory, those bytes
   * will be set to 0x00 in the result.
   *
   * @param buffer The vector to be populated with fetched data.
   * @param address The starting virtual address to read from.
   * @param length The number of bytes to read.
   * @param fill Value to be written into `buffer` when reading from unmapped
   *             data.
   * @return false if the full range was not mapped
   */
  bool ReadVirtualMemory(std::vector<uint8_t>& buffer, uint32_t address,
                         uint32_t length, uint8_t fill = 0xCC);

  /**
   * Writes a block of memory to the simulated Xbox state.
   *
   * If the requested range overlaps with unmapped memory, any initial portion
   * will be written and then false will be returned.
   *
   * @param address Address to write to
   * @param data Data to be written
   * @return false if the full buffer was not written
   */
  bool WriteVirtualMemory(uint32_t address, const std::vector<uint8_t>& data);

  std::string xbox_name = "XBOX-TEST";
  std::string xbox_version = "1.0.5838.1";

  TitleExecutionPhase execution_phase{TitleExecutionPhase::BOOTING};

  LoadOnBootInfo load_on_boot_info;
  BootActions boot_actions;

  std::atomic<bool> awaiting_debugger_{false};
  std::atomic<ExecutionState> execution_state{S_REBOOTING};

  // Simulate non-debugable processes.
  bool is_debugable_{true};

  StopEvents stop_events;

  std::map<uint32_t, MemoryRegion> memory_regions;

  std::map<uint32_t, SimulatedThread> threads;
  uint32_t next_thread_id = 1;
  uint32_t current_thread_id = 0;

  std::map<uint32_t, Breakpoint> breakpoints;

  std::map<std::string, Module> modules;
  std::map<std::string, XbeSection> xbe_sections;
};

}  // namespace xbdm_gdb_bridge::testing

#endif  // XBDM_GDB_BRIDGE_TEST_MOCK_XBDM_SERVER_MOCK_XBOX_STATE_H_
