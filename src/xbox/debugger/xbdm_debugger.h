#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_DEBUGGER_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_DEBUGGER_H_

#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "debugger_expression_parser.h"
#include "rdcp/types/execution_state.h"
#include "rdcp/types/memory_region.h"
#include "rdcp/types/module.h"
#include "rdcp/types/section.h"
#include "rdcp/xbdm_requests.h"
#include "thread.h"

class XBDMContext;
class XBDMNotification;
struct NotificationVX;
struct NotificationDebugStr;
struct NotificationModuleLoaded;
struct NotificationSectionLoaded;
struct NotificationSectionUnloaded;
struct NotificationThreadCreated;
struct NotificationThreadTerminated;
struct NotificationExecutionStateChanged;
struct NotificationBreakpoint;
struct NotificationWatchpoint;
struct NotificationSingleStep;
struct NotificationException;

class XBDMDebugger {
 public:
  static constexpr uint32_t kDefaultHaltAllMaxWaitMilliseconds = 250;
  static constexpr uint32_t kAttachSafeStateMaxWaitMilliseconds = 250;

  enum class BreakpointType {
    BREAKPOINT,
    READ_WATCH,
    WRITE_WATCH,
    EXECUTE_WATCH,
  };

 public:
  explicit XBDMDebugger(std::shared_ptr<XBDMContext> context);

  bool Attach();
  [[nodiscard]] inline bool IsAttached() const { return is_attached_; }
  void Shutdown();

  bool DebugXBE(const std::string& path, bool wait_forever = false,
                bool break_at_start = true);
  bool DebugXBE(const std::string& path, const std::string& command_line,
                bool wait_forever = false, bool break_at_start = true);

  [[nodiscard]] std::list<std::shared_ptr<Thread>> Threads();
  [[nodiscard]] std::list<std::shared_ptr<Module>> Modules();
  [[nodiscard]] std::list<std::shared_ptr<Section>> Sections();

  [[nodiscard]] std::shared_ptr<Module> GetModule(
      const std::string& module_name) const;

  std::vector<uint32_t> GetThreadIDs();

  [[nodiscard]] std::optional<uint32_t> ActiveThreadID();

  [[nodiscard]] std::shared_ptr<Thread> ActiveThread();

  //! Returns an arbitrary thread ID, preferring the active thread.
  [[nodiscard]] std::optional<uint32_t> AnyThreadID();

  [[nodiscard]] std::shared_ptr<Thread> GetAnyThread();

  [[nodiscard]] std::shared_ptr<Thread> GetThread(uint32_t thread_id);

  [[nodiscard]] std::shared_ptr<Thread> GetFirstStoppedThread();

  bool SetActiveThread(uint32_t thread_id);

  [[nodiscard]] const std::list<std::shared_ptr<Module>>& Modules() const {
    return modules_;
  }

  [[nodiscard]] const std::list<std::shared_ptr<Section>>& Sections() const {
    return sections_;
  }

  bool ContinueAll(bool no_break_on_exception = false);
  bool ContinueThread(uint32_t thread_id, bool no_break_on_exception = false);
  bool HaltAll(
      uint32_t optimistic_max_wait = kDefaultHaltAllMaxWaitMilliseconds);
  //! Halts the active thread.
  bool Halt();

  [[nodiscard]] bool Stop() const;
  [[nodiscard]] bool Go() const;

  bool FetchThreads();
  bool FetchModules();
  bool FetchMemoryMap();
  bool RestartAndAttach(int flags = Reboot::kStop);

  bool StepInstruction();
  bool StepFunction();

  std::optional<std::vector<uint8_t>> GetMemory(uint32_t address,
                                                uint32_t length);
  std::optional<uint32_t> GetDWORD(uint32_t address);
  bool SetMemory(uint32_t address, const std::vector<uint8_t>& data);

  void SetDisplayExpandedBreakpointOutput(bool enable) {
    print_thread_info_on_break_ = enable;
  };

  void SetBreakpointCondition(BreakpointType breakpoint_type, uint32_t address,
                              const std::string& condition);
  void RemoveBreakpointCondition(BreakpointType breakpoint_type,
                                 uint32_t address);
  std::optional<std::string> FindBreakpointCondition(
      BreakpointType breakpoint_type, uint32_t address) const;

  bool AddBreakpoint(uint32_t address);
  bool AddReadWatch(uint32_t address, uint32_t length);
  bool AddWriteWatch(uint32_t address, uint32_t length);
  bool RemoveBreakpoint(uint32_t address);
  bool RemoveReadWatch(uint32_t address, uint32_t length);
  bool RemoveWriteWatch(uint32_t address, uint32_t length);

  bool ValidateMemoryAccess(uint32_t address, uint32_t length,
                            bool is_write = false);

  //! Waits up to max_wait_milliseconds for the target to be in one of the given
  //! ExecutionStates.
  bool WaitForStateIn(const std::set<ExecutionState>& target_states,
                      uint32_t max_wait_milliseconds);

  //! Waits up to max_wait_milliseconds for the target to be in a state other
  //! than one of the ExecutionStates.
  bool WaitForStateNotIn(const std::set<ExecutionState>& banned_states,
                         uint32_t max_wait_milliseconds);

  ExecutionState CurrentKnownState();

  DebuggerExpressionParser::MemoryReader CreateMemoryReader();

 private:
  [[nodiscard]] bool BreakAtStart() const;
  bool SetDebugger(bool enabled);
  bool RestartAndReconnect(uint32_t reboot_flags);

  //! Waits up to max_wait_milliseconds for the target to enter the given
  //! ExecutionState.
  bool WaitForState(ExecutionState s, uint32_t max_wait_milliseconds);

  void OnNotification(const std::shared_ptr<XBDMNotification>&);

  static void OnVX(const std::shared_ptr<NotificationVX>&);
  void OnDebugStr(const std::shared_ptr<NotificationDebugStr>&);
  void OnModuleLoaded(const std::shared_ptr<NotificationModuleLoaded>&);
  void OnSectionLoaded(const std::shared_ptr<NotificationSectionLoaded>&);
  void OnSectionUnloaded(const std::shared_ptr<NotificationSectionUnloaded>&);
  void OnThreadCreated(const std::shared_ptr<NotificationThreadCreated>&);
  void OnThreadTerminated(const std::shared_ptr<NotificationThreadTerminated>&);
  void OnExecutionStateChanged(
      const std::shared_ptr<NotificationExecutionStateChanged>&);
  void OnBreakpoint(const std::shared_ptr<NotificationBreakpoint>&);
  void OnWatchpoint(const std::shared_ptr<NotificationWatchpoint>&);
  void OnSingleStep(const std::shared_ptr<NotificationSingleStep>&);
  void OnException(const std::shared_ptr<NotificationException>&);

  /**
   * Called by the various On<x> handlers for breakpoints/watches/steps/fce's
   */
  void PerformAfterStopActions(const std::shared_ptr<Thread>& active_thread);

 private:
  bool is_attached_{false};
  std::shared_ptr<XBDMContext> context_;

  mutable std::mutex state_lock_;
  std::condition_variable state_condition_variable_;
  ExecutionState state_{S_INVALID};

  std::optional<uint32_t> active_thread_id_;

  mutable std::recursive_mutex threads_lock_;
  std::list<std::shared_ptr<Thread>> threads_;

  mutable std::recursive_mutex modules_lock_;
  std::list<std::shared_ptr<Module>> modules_;

  mutable std::recursive_mutex sections_lock_;
  std::list<std::shared_ptr<Section>> sections_;

  mutable std::recursive_mutex memory_regions_lock_;
  std::list<std::shared_ptr<MemoryRegion>> memory_regions_;

  std::map<int, std::string> debugstr_accumulator_;

  mutable std::recursive_mutex conditions_lock_;
  // Maps <BreakpointType, Address> to strings defining IF conditions.
  std::map<std::pair<BreakpointType, uint32_t>, std::string>
      breakpoint_conditions_;

  bool target_not_debuggable_{false};
  int notification_handler_id_{0};

  bool print_thread_info_on_break_{true};
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_DEBUGGER_H_
