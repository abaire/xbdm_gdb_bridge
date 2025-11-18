#include "task_connection.h"

#include <cassert>

TaskConnection::TaskConnection(const std::string& name) : SignalingBase(name) {}

void TaskConnection::Post(std::function<void()> task) {
  const std::lock_guard lock(queue_mutex_);
  task_queue_.push_back(std::move(task));

  SignalProcessingNeeded();
}

bool TaskConnection::Process(const fd_set& read_fds, const fd_set& write_fds,
                             const fd_set& except_fds) {
  if (!SignalingBase::Process(read_fds, write_fds, except_fds)) {
    return false;
  }

  auto now = std::chrono::steady_clock::now();
  std::list<std::function<void()>> tasks_to_run;
  {
    const std::lock_guard lock(queue_mutex_);
    if (!task_queue_.empty()) {
      tasks_to_run.swap(task_queue_);
    }

    std::erase_if(delayed_tasks_,
                  [&tasks_to_run,
                   &now](const std::pair<std::chrono::steady_clock::time_point,
                                         std::function<void()>>& it) {
                    if (it.first <= now) {
                      tasks_to_run.push_back(std::move(it.second));
                      return true;
                    }
                    return false;
                  });
  }

  for (auto& task : tasks_to_run) {
    task();
  }
  return true;
}

std::optional<std::chrono::steady_clock::time_point>
TaskConnection::GetNextEventTime() {
  const std::lock_guard lock(queue_mutex_);
  if (delayed_tasks_.empty()) {
    return std::nullopt;
  }

  return delayed_tasks_.begin()->first;
}
