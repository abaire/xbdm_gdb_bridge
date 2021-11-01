#ifndef XBDM_GDB_BRIDGE_TCPSERVER_H
#define XBDM_GDB_BRIDGE_TCPSERVER_H

#include <string>

#include "tcp_socket_base.h"

class TCPServer : public TCPSocketBase {
 public:
  explicit TCPServer(std::string name) : TCPSocketBase(std::move(name)) {}
  bool Listen(const Address &address);
  void SetConnection(int sock, const Address &address) override;

  int Select(fd_set &read_fds, fd_set &write_fds, fd_set &except_fds) override;
  bool Process(const fd_set &read_fds, const fd_set &write_fds, const fd_set &except_fds) override;

 protected:
  virtual void OnAccepted(int sock, Address &address) = 0;
};

#endif  // XBDM_GDB_BRIDGE_TCPSERVER_H
