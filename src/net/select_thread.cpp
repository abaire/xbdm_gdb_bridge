#include "select_thread.h"

#include <boost/log/trivial.hpp>
#include <chrono>
#include <sys/socket.h>

static long kMaxWaitMilliseconds = 100;

void SelectThread::ThreadMainBootstrap(SelectThread* instance) {
  instance->ThreadMain();
}

void SelectThread::ThreadMain() {
  fd_set recv_fds;
  fd_set send_fds;
  fd_set except_fds;

  struct timeval timeout = {.tv_sec = 0, .tv_usec = 1000 * kMaxWaitMilliseconds};

  while (running_) {
    FD_ZERO(&recv_fds);
    FD_ZERO(&send_fds);
    FD_ZERO(&except_fds);
    int max_fd = -1;

    {
      const std::lock_guard<std::mutex> lock(connection_lock_);
      for (auto &connection : connections_) {
        int conn_max_fd = connection->Select(recv_fds, send_fds, except_fds);
        max_fd = std::max(max_fd, conn_max_fd);
      }
    }

    int fds = select(max_fd + 1, &recv_fds, &send_fds, &except_fds, &timeout);
    if (fds <= 0) {
      BOOST_LOG_TRIVIAL(error) << "select failed " << errno << std::endl;
      // TODO: Determine if this would ever be recoverable.
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    {
      const std::lock_guard<std::mutex> lock(connection_lock_);
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
  thread_ = std::thread(ThreadMainBootstrap, this);
}

void SelectThread::Stop() {
  running_ = false;
  if (thread_.joinable()) {
    thread_.join();
  }
}

void SelectThread::AddConnection(const std::shared_ptr<TCPSocketBase> &conn) {
  const std::lock_guard<std::mutex> lock(connection_lock_);
  connections_.push_back(conn);
}
