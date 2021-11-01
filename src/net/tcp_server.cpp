#include "tcp_server.h"

#include <boost/log/trivial.hpp>
#include <cassert>
#include <sys/socket.h>
#include <unistd.h>

bool TCPServer::Listen(const Address& address) {
  const std::lock_guard<std::mutex> lock(socket_lock_);
  address_ = address;

  socket_ = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket_ < 0) {
    return false;
  }

  const struct sockaddr_in &addr = address.address();
  struct sockaddr_in bind_addr{};
  socklen_t bind_addr_len = sizeof(bind_addr);

  if (bind(socket_, reinterpret_cast<struct sockaddr const *>(&addr),
           sizeof(addr))) {
    goto close_and_fail;
  }

  if (getsockname(socket_, reinterpret_cast<struct sockaddr *>(&bind_addr), &bind_addr_len) < 0) {
    BOOST_LOG_TRIVIAL(error) << "getsockname failed" << errno << std::endl;
    goto close_and_fail;
  }
  address_ = Address(bind_addr);

  if (listen(socket_, 1)) {
    BOOST_LOG_TRIVIAL(error) << "listen failed" << errno << std::endl;
    goto close_and_fail;
  }

  return true;

close_and_fail:
  shutdown(socket_, SHUT_RDWR);
  close(socket_);
  socket_ = -1;
  return false;
}

void TCPServer::SetConnection(int sock, const Address& address) {
  assert(false);
}

int TCPServer::Select(fd_set &read_fds, fd_set &write_fds,
                          fd_set &except_fds) {
  const std::lock_guard<std::mutex> lock(socket_lock_);
  if (socket_ < 0) {
    return socket_;
  }

  FD_SET(socket_, &read_fds);
  FD_SET(socket_, &except_fds);
  return socket_;
}

bool TCPServer::Process(const fd_set &read_fds, const fd_set &write_fds,
                           const fd_set &except_fds) {
  const std::lock_guard<std::mutex> lock(socket_lock_);
  if (socket_ < 0) {
    return false;
  }

  if (FD_ISSET(socket_, &except_fds)) {
    BOOST_LOG_TRIVIAL(trace) << "Socket exception detected." << std::endl;
    Close();
    return false;
  }

  if (FD_ISSET(socket_, &read_fds)) {
    struct sockaddr_in bind_addr{};
    socklen_t bind_addr_len = sizeof(bind_addr);

    int accepted_socket = accept(socket_, reinterpret_cast<struct sockaddr *>(&bind_addr), &bind_addr_len);
    if (accepted_socket < 0) {
      BOOST_LOG_TRIVIAL(error) << "accept failed" << errno << std::endl;
    } else {
      OnAccepted(accepted_socket, Address(bind_addr));
    }
  }

  return true;
}
