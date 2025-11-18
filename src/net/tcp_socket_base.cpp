#include "tcp_socket_base.h"

#include <unistd.h>

void TCPSocketBase::SetConnection(int sock, const IPAddress& address) {
  const std::lock_guard lock(socket_lock_);
  socket_ = sock;
  address_ = address;
}

void TCPSocketBase::Close() {
  SignalingBase::Close();
  is_shutdown_ = true;
  if (socket_ < 0) {
    return;
  }
  const std::lock_guard lock(socket_lock_);
  shutdown(socket_, SHUT_RDWR);
  close(socket_);
  socket_ = -1;
}
