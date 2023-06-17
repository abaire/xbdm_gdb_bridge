#include "select_thread.h"

#include <sys/socket.h>

#include <chrono>

#include "util/logging.h"
#include "util/timer.h"

void SelectThread::ThreadMainBootstrap(SelectThread *instance) {
  instance->ThreadMain();
}

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

    {
      const std::lock_guard<std::recursive_mutex> lock(connection_lock_);
      for (auto &connection : connections_) {
        int conn_max_fd = connection->Select(recv_fds, send_fds, except_fds);
        if (conn_max_fd < 0) {
          continue;
        }
        max_fd = std::max(max_fd, conn_max_fd);
      }
    }

    if (max_fd < 0) {
      WaitMilliseconds(kMinSleepMilliseconds);
      continue;
    }

    struct timeval timeout = {.tv_sec = 0, .tv_usec = 10};
    int fds = select(max_fd + 1, &recv_fds, &send_fds, &except_fds, &timeout);
    if (!fds) {
      continue;
    }
    if (fds < 0) {
      LOG(error) << "select failed " << errno;
      // TODO: Determine if this would ever be recoverable.
      WaitMilliseconds(kMinSleepMilliseconds);
      continue;
    }

    {
      const std::lock_guard<std::recursive_mutex> lock(connection_lock_);
      auto it = connections_.begin();
      while (it != connections_.end()) {
        if (!*it) {
          it = connections_.erase(it);
          continue;
        }

        if (!(*it)->Process(recv_fds, send_fds, except_fds)) {
          it = connections_.erase(it);
          continue;
        }

        ++it;
      }
    }
  }
}

void SelectThread::Start() {
  running_ = true;
  thread_ = std::thread(ThreadMainBootstrap, this);
}

void SelectThread::Stop() {
  running_ = false;
  if (thread_.joinable()) {
    thread_.join();
  }
}

void SelectThread::AddConnection(std::shared_ptr<TCPSocketBase> conn) {
  const std::lock_guard<std::recursive_mutex> lock(connection_lock_);
  connections_.emplace_back(conn);
}
