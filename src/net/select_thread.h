#ifndef XBDM_GDB_BRIDGE_SELECTTHREAD_H
#define XBDM_GDB_BRIDGE_SELECTTHREAD_H

#include <atomic>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#include "net/tcp_socket_base.h"

class SelectThread {
 public:
  void Start();
  void Stop();

  [[nodiscard]] bool IsRunning() const { return running_; }

  void AddConnection(const std::shared_ptr<TCPSocketBase>& conn);
  //! Registers the given connection along with a callback function to be
  //! invoked when the connection is closed.
  void AddConnection(const std::shared_ptr<TCPSocketBase>& conn,
                     std::function<void()> on_close);

 private:
  void ThreadMain();
  static void ThreadMainBootstrap(SelectThread* instance);

 protected:
  std::atomic<bool> running_{false};
  std::thread thread_;

  std::recursive_mutex connection_lock_;
  std::list<std::shared_ptr<TCPSocketBase>> connections_;

  std::map<std::shared_ptr<TCPSocketBase>, std::function<void()>>
      close_callbacks_;
};

#endif  // XBDM_GDB_BRIDGE_SELECTTHREAD_H
