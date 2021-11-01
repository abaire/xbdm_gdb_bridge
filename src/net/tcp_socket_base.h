#ifndef XBDM_GDB_BRIDGE_TCP_SOCKET_BASE_H
#define XBDM_GDB_BRIDGE_TCP_SOCKET_BASE_H

#include <mutex>

#include "address.h"

class TCPSocketBase {
 public:
  explicit TCPSocketBase(std::string name, int sock = -1)
  : name_(std::move(name)), socket_(sock) {}
  TCPSocketBase(std::string name, int sock, Address address)
  : name_(std::move(name)), socket_(sock), address_(std::move(address)) {}

  virtual void SetConnection(int sock, const Address &address);
  [[nodiscard]] bool IsConnected() const { return socket_ >= 0; }

  virtual void Close();

  virtual void Select(fd_set &read_fds, fd_set &write_fds, fd_set &except_fds) = 0;
  virtual void Process(const fd_set &read_fds, const fd_set &write_fds,
               const fd_set &except_fds) = 0;

 protected:
  std::string name_;

  std::mutex socket_lock_;
  int socket_{-1};
  Address address_;
};

#endif  // XBDM_GDB_BRIDGE_TCP_SOCKET_BASE_H