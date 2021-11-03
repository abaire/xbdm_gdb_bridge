#include "select_thread.h"

#include <sys/socket.h>

#include <boost/log/trivial.hpp>
#include <chrono>

static int kMaxWaitMilliseconds = 100;

void SelectThread::ThreadMainBootstrap(SelectThread *instance) {
  instance->ThreadMain();
}

void SelectThread::ThreadMain() {
  fd_set recv_fds;
  fd_set send_fds;
  fd_set except_fds;

  static constexpr int kMinSleepMilliseconds = 10;
  struct timeval timeout = {.tv_sec = 0,
                            .tv_usec = 1000 * kMaxWaitMilliseconds};

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
      std::this_thread::sleep_for(std::chrono::milliseconds(kMinSleepMilliseconds));
      continue;
    }

    int fds = select(max_fd + 1, &recv_fds, &send_fds, &except_fds, &timeout);
    if (!fds) {
      continue;
    }
    if (fds < 0) {
      BOOST_LOG_TRIVIAL(error) << "select failed " << errno;
      // TODO: Determine if this would ever be recoverable.
      std::this_thread::sleep_for(std::chrono::milliseconds(kMinSleepMilliseconds));
      continue;
    }

    {
      const std::lock_guard<std::recursive_mutex> lock(connection_lock_);
      auto it = connections_.begin();
      auto it_end = connections_.end();

      while (it != it_end) {
        bool keep = (*it)->Process(recv_fds, send_fds, except_fds);
        if (!keep) {
          it = connections_.erase(it);
        } else {
          ++it;
        }
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

void SelectThread::AddConnection(const std::shared_ptr<TCPSocketBase> &conn) {
  const std::lock_guard<std::recursive_mutex> lock(connection_lock_);
  connections_.push_back(conn);
}
