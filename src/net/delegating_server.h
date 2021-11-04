#ifndef XBDM_GDB_BRIDGE_SRC_NET_DELEGATING_SERVER_H_
#define XBDM_GDB_BRIDGE_SRC_NET_DELEGATING_SERVER_H_

#include <functional>
#include <utility>

#include "net/tcp_server.h"

class DelegatingServer : public TCPServer {
 public:
  typedef std::function<void(int, IPAddress &)> ConnectionAcceptedHandler;

 public:
  explicit DelegatingServer(std::string name,
                            ConnectionAcceptedHandler connection_accepted)
      : TCPServer(std::move(name)),
        connection_accepted_(std::move(connection_accepted)) {}

 protected:
  void OnAccepted(int sock, IPAddress &address) override {
    connection_accepted_(sock, address);
  }

 private:
  ConnectionAcceptedHandler connection_accepted_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_NET_DELEGATING_SERVER_H_
