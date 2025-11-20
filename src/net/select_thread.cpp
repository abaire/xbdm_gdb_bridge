#include "select_thread.h"

#include <sys/socket.h>

#include <chrono>
#include <future>
#include <unordered_set>
#include <utility>

#include "util/logging.h"
#include "util/thread_debug_util.h"
#include "util/timer.h"

static constexpr int kQuiescenseTimeoutMicroseconds = 10000;

SelectThread::SelectThread(std::string debug_name)
    : select_signaller_(std::make_shared<TaskConnection>("SelectSignaller")),
      debug_name_(std::move(debug_name)) {}

void SelectThread::ThreadMainBootstrap(SelectThread* instance) {
  debug::SetCurrentThreadName(instance->debug_name_);
  instance->ThreadMain();
}

namespace {

void FutureTimeToTimevalTimeout(
    struct timeval& timeout,
    const std::chrono::steady_clock::time_point& timestamp) {
  auto now = std::chrono::steady_clock::now();
  auto remaining = timestamp - now;
  if (remaining.count() <= 0) {
    // Time has already expired, so don't block. Use a zero-timeout.
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return;
  }

  auto secs = std::chrono::duration_cast<std::chrono::seconds>(remaining);
  auto usecs =
      std::chrono::duration_cast<std::chrono::microseconds>(remaining - secs);
  timeout.tv_sec = secs.count();
  timeout.tv_usec = usecs.count();
}

}  // namespace

void SelectThread::ThreadMain() {
  fd_set recv_fds;
  fd_set send_fds;
  fd_set except_fds;

  static constexpr int kMinSleepMilliseconds = 1;

  while (running_) {
    FD_ZERO(&recv_fds);
    FD_ZERO(&send_fds);
    FD_ZERO(&except_fds);

    int max_fd = -1;
    std::optional<std::chrono::steady_clock::time_point> soonest_scheduled =
        std::nullopt;

    ApplyAndEraseIf([&max_fd, &soonest_scheduled, &recv_fds, &send_fds,
                     &except_fds](const auto& entry) {
      int conn_max_fd = entry->Select(recv_fds, send_fds, except_fds);
      if (conn_max_fd < 0) {
        return entry->IsShutdown();
      }

      max_fd = std::max(max_fd, conn_max_fd);

      auto conn_event_time = entry->GetNextEventTime();
      if (conn_event_time) {
        if (!soonest_scheduled || *conn_event_time < *soonest_scheduled) {
          soonest_scheduled = conn_event_time;
        }
      }

      return false;
    });

    if (max_fd < 0) {
      WaitMilliseconds(kMinSleepMilliseconds);
      assert(!"There should always be at least the signaller");
      continue;
    }

    struct timeval timeout{0};
    struct timeval* timeout_ptr = nullptr;

    bool has_fences = false;
    {
      std::lock_guard lock(fence_mutex_);
      has_fences = !pending_fences_.empty();
    }

    if (has_fences) {
      timeout.tv_usec = kQuiescenseTimeoutMicroseconds;
      timeout_ptr = &timeout;
    } else if (soonest_scheduled) {
      FutureTimeToTimevalTimeout(timeout, *soonest_scheduled);
      timeout_ptr = &timeout;
    }

    int fds =
        select(max_fd + 1, &recv_fds, &send_fds, &except_fds, timeout_ptr);

    if (fds < 0) {
      char buffer[256];
      strerror_r(errno, buffer, 256);
      LOG(error) << debug_name_ << " select failed " << errno << " - "
                 << buffer;
      if (errno != EBADF) {
        LOG(error) << debug_name_ << "select failed " << errno << " - "
                   << buffer;
      }
      // TODO: Determine if this would ever be recoverable.
      WaitMilliseconds(kMinSleepMilliseconds);
      continue;
    }

    ApplyAndEraseIf([&recv_fds, &send_fds, &except_fds](const auto& entry) {
      return !entry->Process(recv_fds, send_fds, except_fds);
    });

    if (has_fences && !fds) {
      std::lock_guard lock(fence_mutex_);
      for (auto& fence : pending_fences_) {
        fence.set_value();
      }
      pending_fences_.clear();
    }
  }
}

void SelectThread::AwaitQuiescence() {
  std::promise<void> fence;
  auto future = fence.get_future();
  {
    std::lock_guard lock(fence_mutex_);
    pending_fences_.push_back(std::move(fence));
  }
  select_signaller_->SignalProcessingNeeded();
  future.wait();
}

void SelectThread::ApplyAndEraseIf(
    const std::function<bool(const std::shared_ptr<SelectableBase>&)>& func) {
  {
    std::list<std::shared_ptr<SelectableBase>> entries_to_service;
    {
      const std::lock_guard lock(selectables_lock_);
      entries_to_service = selectables_;
    }

    std::unordered_set<std::shared_ptr<SelectableBase>> entries_to_remove;
    for (auto& entry : entries_to_service) {
      if (!entry || func(entry)) {
        entries_to_remove.insert(entry);
      }
    }

    if (!entries_to_remove.empty()) {
      const std::lock_guard lock(selectables_lock_);
      std::erase_if(selectables_, [&entries_to_remove](const auto& item) {
        return !item || entries_to_remove.count(item);
      });
    }
  }
}

void SelectThread::Start() {
  running_ = true;
  AddConnection(select_signaller_);
  thread_ = std::thread(ThreadMainBootstrap, this);
}

void SelectThread::Stop() {
  select_signaller_->Post([this] { running_ = false; });
  if (thread_.joinable()) {
    thread_.join();
  }
}

void SelectThread::AddConnection(const std::shared_ptr<SelectableBase>& conn) {
  assert(conn && "Invalid connection passed to AddConnection");
  {
    const std::lock_guard lock(selectables_lock_);
    selectables_.emplace_back(conn);
  }

  std::string conn_name = conn->Name();

  select_signaller_->Post([this, conn_name] {});
}

void SelectThread::AddConnection(const std::shared_ptr<SelectableBase>& conn,
                                 std::function<void()> on_close) {
  const std::lock_guard lock(selectables_lock_);
  AddConnection(conn);
  close_callbacks_[conn] = std::move(on_close);
}
