#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_DEBUGGER_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_DEBUGGER_H_

#include <memory>
#include <vector>

#include "memory_region.h"
#include "module.h"
#include "section.h"
#include "thread.h"

class XBDMContext;
class XBDMNotification;

class XBDMDebugger {
 public:
  XBDMDebugger(std::shared_ptr<XBDMContext> context);

  bool Attach();
  void Shutdown();

  bool DebugXBE(const std::string &path);
  bool DebugXBE(const std::string &path, const std::string &command_line);

  [[nodiscard]] const std::vector<std::shared_ptr<Thread>> &Threads() const {
    return threads_;
  }

  [[nodiscard]] std::shared_ptr<Thread> ActiveThread() const {
    if (active_thread_ < 0 || active_thread_ > threads_.size()) {
      return {};
    }

    return threads_[active_thread_];
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

  [[nodiscard]] const std::vector<std::shared_ptr<Module>> &Modules() const {
    return modules_;
  }
  [[nodiscard]] const std::vector<std::shared_ptr<Section>> &Sections() const {
    return sections_;
  }

 private:
  void OnNotification(const XBDMNotification &);

 private:
  std::shared_ptr<XBDMContext> context_;

  int active_thread_;
  std::vector<std::shared_ptr<Thread>> threads_;
  std::vector<std::shared_ptr<Module>> modules_;
  std::vector<std::shared_ptr<Section>> sections_;

  bool target_not_debuggable_{false};
  int notification_handler_id_{0};
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_DEBUGGER_H_
