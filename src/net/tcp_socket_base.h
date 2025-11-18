#ifndef XBDM_GDB_BRIDGE_TCP_SOCKET_BASE_H
#define XBDM_GDB_BRIDGE_TCP_SOCKET_BASE_H

#include <mutex>
#include <utility>

#include "ip_address.h"
#include "signaling_base.h"

class TCPSocketBase : public SignalingBase {
 public:
  explicit TCPSocketBase(std::string name, int sock = -1)
      : SignalingBase(std::move(name)), socket_(sock) {}
  TCPSocketBase(std::string name, int sock, IPAddress address)
      : SignalingBase(std::move(name)),
        socket_(sock),
        address_(std::move(address)) {}

  virtual void SetConnection(int sock, const IPAddress& address);
  [[nodiscard]] bool IsConnected() const { return socket_ >= 0; }
  [[nodiscard]] const IPAddress& Address() const { return address_; }

  void Close() override;

  std::ostream& Print(std::ostream& os) const override {
    return os << "TCPSocketBase[" << name_ << " - " << address_ << "]";
  }

 protected:
  std::recursive_mutex socket_lock_;
  int socket_{-1};
  IPAddress address_;
};

#endif  // XBDM_GDB_BRIDGE_TCP_SOCKET_BASE_H
