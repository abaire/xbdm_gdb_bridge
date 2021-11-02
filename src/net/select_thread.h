#ifndef XBDM_GDB_BRIDGE_SELECTTHREAD_H
#define XBDM_GDB_BRIDGE_SELECTTHREAD_H

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "net/tcp_socket_base.h"

class SelectThread {
 public:
  void Start();
  void Stop();

  [[nodiscard]] bool IsRunning() const { return running_; }

  void AddConnection(const std::shared_ptr<TCPSocketBase>& conn);

 private:
  void ThreadMain();
  static void ThreadMainBootstrap(SelectThread* instance);

 protected:
  std::atomic<bool> running_{false};
  std::thread thread_;

  std::mutex connection_lock_;
  std::vector<std::shared_ptr<TCPSocketBase>> connections_;
};

#endif  // XBDM_GDB_BRIDGE_SELECTTHREAD_H
