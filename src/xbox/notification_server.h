#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_NOTIFICATION_SERVER_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_NOTIFICATION_SERVER_H_

#include <functional>
#include <utility>

#include "net/tcp_server.h"
#include "notification/xbdm_notification_transport.h"

class NotificationServer : public TCPServer {
 public:
  typedef std::function<void(int, Address&)> ConnectionAcceptedHandler;

 public:
  explicit NotificationServer(std::string name,
                              ConnectionAcceptedHandler connection_accepted) : TCPServer(std::move(name)),
        connection_accepted_(std::move(connection_accepted)) {}

 protected:
  void OnAccepted(int sock, Address &address) override {
    connection_accepted_(sock, address);
  }

 private:
  ConnectionAcceptedHandler connection_accepted_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_NOTIFICATION_SERVER_H_
