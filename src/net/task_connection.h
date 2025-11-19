#ifndef XBDM_GDB_BRIDGE_TASK_CONNECTION_H
#define XBDM_GDB_BRIDGE_TASK_CONNECTION_H

#include <functional>
#include <list>
#include <map>
#include <mutex>

#include "net/signaling_base.h"

/**
 * A Selectable that manages a task queue to be executed in the managing
 * SelectThread.
 */
class TaskConnection : public SignalingBase {
 public:
  explicit TaskConnection(const std::string& name);

  void Post(std::function<void()> task);

  /**
   * @brief Posts a task to be run after a specified delay.
   * @tparam Rep
   * @tparam Period
   * @param delay The delay (e.g., 500ms).
   * @param task The function to execute.
   */
  template <typename Rep, typename Period>
  void PostDelayed(std::chrono::duration<Rep, Period> delay,
                   std::function<void()> task);

  bool Process(const fd_set& read_fds, const fd_set& write_fds,
               const fd_set& except_fds) override;

  std::optional<std::chrono::steady_clock::time_point> GetNextEventTime()
      override;

 private:
  std::mutex queue_mutex_;
  std::list<std::function<void()>> task_queue_;

  std::multimap<std::chrono::steady_clock::time_point, std::function<void()>>
      delayed_tasks_;

  int pipe_fds_[2]{-1, -1};
};

template <typename Rep, typename Period>
void TaskConnection::PostDelayed(std::chrono::duration<Rep, Period> delay,
                                 std::function<void()> task) {
  auto expires = std::chrono::steady_clock::now() + delay;
  {
    const std::lock_guard lock(queue_mutex_);
    delayed_tasks_.emplace(expires, std::move(task));
  }

  SignalProcessingNeeded();
}

#endif  // XBDM_GDB_BRIDGE_TASK_CONNECTION_H
