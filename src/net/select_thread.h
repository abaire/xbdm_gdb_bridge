#ifndef XBDM_GDB_BRIDGE_SELECTTHREAD_H
#define XBDM_GDB_BRIDGE_SELECTTHREAD_H

#include <atomic>
#include <functional>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#include "net/selectable_base.h"
#include "net/task_connection.h"

class SelectThread {
 public:
  SelectThread() : SelectThread("") {};
  explicit SelectThread(std::string debug_name);

  void Start();
  void Stop();

  [[nodiscard]] bool IsRunning() const { return running_; }

  /**
   * Blocks the calling thread until the SelectThread has processed all
   * currently pending events and is quiescent.
   */
  void AwaitQuiescence();

  void AddConnection(const std::shared_ptr<SelectableBase>& conn);
  //! Registers the given connection along with a callback function to be
  //! invoked when the connection is closed.
  void AddConnection(const std::shared_ptr<SelectableBase>& conn,
                     std::function<void()> on_close);

 private:
  void ThreadMain();
  static void ThreadMainBootstrap(SelectThread* instance);

  void ApplyAndEraseIf(
      const std::function<bool(const std::shared_ptr<SelectableBase>&)>& func);

 protected:
  std::atomic<bool> running_{false};
  std::thread thread_;

  mutable std::recursive_mutex selectables_lock_;
  std::list<std::shared_ptr<SelectableBase>> selectables_;

  std::map<std::shared_ptr<SelectableBase>, std::function<void()>>
      close_callbacks_;

 private:
  std::string debug_name_;
  std::shared_ptr<TaskConnection> select_signaller_;

  mutable std::mutex fence_mutex_;
  std::vector<std::promise<void>> pending_fences_;
};

#endif  // XBDM_GDB_BRIDGE_SELECTTHREAD_H
