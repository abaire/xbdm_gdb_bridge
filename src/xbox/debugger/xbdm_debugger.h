#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_DEBUGGER_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_DEBUGGER_H_

#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>

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

 public:
  explicit XBDMDebugger(std::shared_ptr<XBDMContext> context);

  bool Attach();
  void Shutdown();

  bool DebugXBE(const std::string &path, bool wait_forever = false,
                bool break_at_start = true);
  bool DebugXBE(const std::string &path, const std::string &command_line,
                bool wait_forever = false, bool break_at_start = true);

  [[nodiscard]] std::list<std::shared_ptr<Thread>> Threads();
  [[nodiscard]] std::list<std::shared_ptr<Module>> Modules();
  [[nodiscard]] std::list<std::shared_ptr<Section>> Sections();

  std::shared_ptr<Module> GetModule(const std::string &module_name) const;

  std::vector<int32_t> GetThreadIDs();

  [[nodiscard]] int ActiveThreadID() {
    std::shared_ptr<Thread> thread = ActiveThread();
    if (thread) {
      return thread->thread_id;
    }
    return -1;
  }

  [[nodiscard]] std::shared_ptr<Thread> ActiveThread();

  //! Returns an arbitrary thread ID, preferring the active thread.
  [[nodiscard]] int AnyThreadID() {
    if (threads_.empty()) {
      return -1;
    }

    auto active_thread = ActiveThread();
    if (active_thread) {
      return active_thread->thread_id;
    }

    return threads_.front()->thread_id;
  }

  [[nodiscard]] std::shared_ptr<Thread> GetAnyThread();

  [[nodiscard]] std::shared_ptr<Thread> GetThread(int thread_id);

  [[nodiscard]] std::shared_ptr<Thread> GetFirstStoppedThread();

  bool SetActiveThread(int thread_id);

  [[nodiscard]] const std::list<std::shared_ptr<Module>> &Modules() const {
    return modules_;
  }

  [[nodiscard]] const std::list<std::shared_ptr<Section>> &Sections() const {
    return sections_;
  }

  bool ContinueAll(bool no_break_on_exception = false);
  bool ContinueThread(int thread_id, bool no_break_on_exception = false);
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
  bool SetMemory(uint32_t address, const std::vector<uint8_t> &data);

  bool AddBreakpoint(uint32_t address);
  bool AddReadWatch(uint32_t address, uint32_t length);
  bool AddWriteWatch(uint32_t address, uint32_t length);
  bool RemoveBreakpoint(uint32_t address);
  bool RemoveReadWatch(uint32_t address, uint32_t length);
  bool RemoveWriteWatch(uint32_t address, uint32_t length);

  bool ValidateMemoryAccess(uint32_t address, uint32_t length,
                            bool is_write = false);

  //! Waits up to max_wait_milliseconds for the target to enter one of the given
  //! ExecutionStates.
  bool WaitForStateIn(const std::set<ExecutionState> &target_states,
                      uint32_t max_wait_milliseconds);

 private:
  [[nodiscard]] bool BreakAtStart() const;
  bool SetDebugger(bool enabled);
  bool RestartAndReconnect(uint32_t reboot_flags);

  //! Waits up to max_wait_milliseconds for the target to enter the given
  //! ExecutionState.
  bool WaitForState(ExecutionState s, uint32_t max_wait_milliseconds);

  void OnNotification(const std::shared_ptr<XBDMNotification> &);

  static void OnVX(const std::shared_ptr<NotificationVX> &);
  void OnDebugStr(const std::shared_ptr<NotificationDebugStr> &);
  void OnModuleLoaded(const std::shared_ptr<NotificationModuleLoaded> &);
  void OnSectionLoaded(const std::shared_ptr<NotificationSectionLoaded> &);
  void OnSectionUnloaded(const std::shared_ptr<NotificationSectionUnloaded> &);
  void OnThreadCreated(const std::shared_ptr<NotificationThreadCreated> &);
  void OnThreadTerminated(
      const std::shared_ptr<NotificationThreadTerminated> &);
  void OnExecutionStateChanged(
      const std::shared_ptr<NotificationExecutionStateChanged> &);
  void OnBreakpoint(const std::shared_ptr<NotificationBreakpoint> &);
  void OnWatchpoint(const std::shared_ptr<NotificationWatchpoint> &);
  void OnSingleStep(const std::shared_ptr<NotificationSingleStep> &);
  void OnException(const std::shared_ptr<NotificationException> &);

 private:
  std::shared_ptr<XBDMContext> context_;

  std::mutex state_lock_;
  std::condition_variable state_condition_variable_;
  ExecutionState state_{S_INVALID};

  int active_thread_id_{-1};

  std::recursive_mutex threads_lock_;
  std::list<std::shared_ptr<Thread>> threads_;

  std::recursive_mutex modules_lock_;
  std::list<std::shared_ptr<Module>> modules_;

  std::recursive_mutex sections_lock_;
  std::list<std::shared_ptr<Section>> sections_;

  std::recursive_mutex memory_regions_lock_;
  std::list<std::shared_ptr<MemoryRegion>> memory_regions_;

  std::map<int, std::string> debugstr_accumulator_;

  bool target_not_debuggable_{false};
  int notification_handler_id_{0};
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_DEBUGGER_H_
