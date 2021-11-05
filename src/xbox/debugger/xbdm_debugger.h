#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_DEBUGGER_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_DEBUGGER_H_

#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "memory_region.h"
#include "rdcp/types/execution_state.h"
#include "rdcp/types/module.h"
#include "rdcp/types/section.h"
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
  explicit XBDMDebugger(std::shared_ptr<XBDMContext> context);

  bool Attach();
  void Shutdown();

  bool DebugXBE(const std::string &path, bool wait_forever = false);
  bool DebugXBE(const std::string &path, const std::string &command_line,
                bool wait_forever = false);

  [[nodiscard]] std::list<std::shared_ptr<Thread>> Threads();
  [[nodiscard]] std::list<std::shared_ptr<Module>> Modules();
  [[nodiscard]] std::list<std::shared_ptr<Section>> Sections();

  [[nodiscard]] int ActiveThreadID() const {
    auto thread = ActiveThread();
    if (thread) {
      return thread->thread_id;
    }
    return -1;
  }

  [[nodiscard]] std::shared_ptr<Thread> ActiveThread() const {
    if (active_thread_index_ < 0 || active_thread_index_ > threads_.size()) {
      return {};
    }

    return *std::next(threads_.begin(), active_thread_index_);
  }

  //! Returns an arbitrary thread ID, preferring the active thread.
  [[nodiscard]] int AnyThreadID() const {
    if (threads_.empty()) {
      return -1;
    }

    auto active_thread = ActiveThread();
    if (active_thread) {
      return active_thread->thread_id;
    }

    return threads_.front()->thread_id;
  }

  [[nodiscard]] std::shared_ptr<Thread> GetThread(int thread_id);

  bool SetActiveThread(int thread_id);

  [[nodiscard]] const std::list<std::shared_ptr<Module>> &Modules() const {
    return modules_;
  }

  [[nodiscard]] const std::list<std::shared_ptr<Section>> &Sections() const {
    return sections_;
  }

  bool ContinueAll(bool no_break_on_exception = false);
  bool HaltAll();

  bool FetchThreads();
  bool FetchModules();
  bool RestartAndAttach(int flags = Reboot::kStop);

  bool StepFunction();

 private:
  [[nodiscard]] bool Stop() const;
  [[nodiscard]] bool Go() const;
  [[nodiscard]] bool BreakAtStart() const;
  bool SetDebugger(bool enabled);
  bool RestartAndReconnect(uint32_t reboot_flags);

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

  int active_thread_index_{-1};

  std::recursive_mutex threads_lock_;
  std::list<std::shared_ptr<Thread>> threads_;

  std::recursive_mutex modules_lock_;
  std::list<std::shared_ptr<Module>> modules_;

  std::recursive_mutex sections_lock_;
  std::list<std::shared_ptr<Section>> sections_;

  std::map<int, std::string> debugstr_accumulator_;

  bool target_not_debuggable_{false};
  int notification_handler_id_{0};
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_DEBUGGER_H_