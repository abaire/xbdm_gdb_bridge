#ifndef XBDM_GDB_BRIDGE_TCPSERVER_H
#define XBDM_GDB_BRIDGE_TCPSERVER_H

#include "tcp_socket_base.h"

class TCPServer : public TCPSocketBase {
 public:
  bool Listen(const Address &address);
  void SetConnection(int sock, const Address &address) override;

  void Select(fd_set &read_fds, fd_set &write_fds, fd_set &except_fds) override;
  void Process(const fd_set &read_fds, const fd_set &write_fds, const fd_set &except_fds) override;

 protected:
  virtual void OnAccepted(int sock, Address &&address) = 0;
};

#endif  // XBDM_GDB_BRIDGE_TCPSERVER_H
