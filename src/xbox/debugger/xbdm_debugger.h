#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_DEBUGGER_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_DEBUGGER_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include "memory_region.h"
#include "rdcp/types/module.h"
#include "rdcp/types/section.h"
#include "thread.h"

class XBDMContext;
class XBDMNotification;
struct NotificationVX;
struct NotificationDebugStr;
struct NotificationModuleLoaded;
struct NotificationSectionLoaded;
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

  bool DebugXBE(const std::string &path);
  bool DebugXBE(const std::string &path, const std::string &command_line);

  [[nodiscard]] const std::list<std::shared_ptr<Thread>> &Threads() const {
    return threads_;
  }

  [[nodiscard]] int ActiveThreadID() const { return active_thread_id_; }

  [[nodiscard]] std::shared_ptr<Thread> ActiveThread() const {
    if (active_thread_id_ < 0 || active_thread_id_ > threads_.size()) {
      return {};
    }

    return *std::next(threads_.begin(), active_thread_id_);
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

  bool FetchThreads();
  bool RestartAndAttach(int flags = Reboot::kStop);

 private:
  [[nodiscard]] bool Stop() const;
  [[nodiscard]] bool Go() const;
  bool SetDebugger(bool enabled);
  bool RestartAndReconnect(int reboot_flags);

  void OnNotification(const std::shared_ptr<XBDMNotification> &);

  static void OnVX(const std::shared_ptr<NotificationVX> &);
  void OnDebugStr(const std::shared_ptr<NotificationDebugStr> &);
  void OnModuleLoaded(const std::shared_ptr<NotificationModuleLoaded> &);
  void OnSectionLoaded(const std::shared_ptr<NotificationSectionLoaded> &);
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

  int active_thread_id_{-1};
  std::recursive_mutex thread_lock_;
  std::list<std::shared_ptr<Thread>> threads_;
  std::list<std::shared_ptr<Module>> modules_;
  std::list<std::shared_ptr<Section>> sections_;

  std::map<int, std::string> debugstr_accumulator_;

  bool target_not_debuggable_{false};
  int notification_handler_id_{0};
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_DEBUGGER_H_
