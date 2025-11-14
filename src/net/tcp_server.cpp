#include "tcp_server.h"

#include <sys/socket.h>
#include <unistd.h>

#include <cassert>

#include "net/ip_address.h"
#include "util/logging.h"

bool TCPServer::Listen(const IPAddress& address) {
  const std::lock_guard<std::recursive_mutex> lock(socket_lock_);
  address_ = address;

  socket_ = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket_ < 0) {
    return false;
  }

  const struct sockaddr_in& addr = address.Address();
  struct sockaddr_in bind_addr{};
  socklen_t bind_addr_len = sizeof(bind_addr);

  int enabled = 1;
  if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &enabled,
                 sizeof(enabled))) {
    char buffer[256];
    strerror_r(errno, buffer, 256);
    LOG(warning) << "Failed to set reuseaddr " << errno << " - " << buffer;
  }

  if (bind(socket_, reinterpret_cast<struct sockaddr const*>(&addr),
           sizeof(addr))) {
    char buffer[256];
    strerror_r(errno, buffer, 256);
    LOG(error) << "Bind failed " << errno << " - " << buffer;
    goto close_and_fail;
  }

  if (getsockname(socket_, reinterpret_cast<struct sockaddr*>(&bind_addr),
                  &bind_addr_len) < 0) {
    char buffer[256];
    strerror_r(errno, buffer, 256);
    LOG(error) << "getsockname failed " << errno << " - " << buffer;
    goto close_and_fail;
  }
  address_ = IPAddress(bind_addr);

  if (listen(socket_, 1)) {
    char buffer[256];
    strerror_r(errno, buffer, 256);
    LOG(error) << "listen failed " << errno << " - " << buffer;
    goto close_and_fail;
  }

  LOG(trace) << "Server listening at " << address_;

  return true;

close_and_fail:
  shutdown(socket_, SHUT_RDWR);
  close(socket_);
  socket_ = -1;
  is_shutdown_ = true;
  return false;
}

void TCPServer::SetConnection(int sock, const IPAddress& address) {
  assert(false);
}

int TCPServer::Select(fd_set& read_fds, fd_set& write_fds, fd_set& except_fds) {
  const std::lock_guard<std::recursive_mutex> lock(socket_lock_);
  if (socket_ < 0) {
    return socket_;
  }

  FD_SET(socket_, &read_fds);
  FD_SET(socket_, &except_fds);
  return socket_;
}

bool TCPServer::Process(const fd_set& read_fds, const fd_set& write_fds,
                        const fd_set& except_fds) {
  const std::lock_guard<std::recursive_mutex> lock(socket_lock_);
  if (socket_ < 0) {
    // If the socket was previously connected and is now shutdown, request
    // deletion.
    return !is_shutdown_;
  }

  if (FD_ISSET(socket_, &except_fds)) {
    LOG(trace) << "Socket exception detected.";
    Close();
    return false;
  }

  if (FD_ISSET(socket_, &read_fds)) {
    struct sockaddr_in bind_addr{};
    socklen_t bind_addr_len = sizeof(bind_addr);

    int accepted_socket =
        accept(socket_, reinterpret_cast<struct sockaddr*>(&bind_addr),
               &bind_addr_len);
    if (accepted_socket < 0) {
      char buffer[256];
      strerror_r(errno, buffer, 256);
      LOG(error) << "accept failed " << errno << " - " << buffer;
    } else {
      auto address = IPAddress(bind_addr);
      OnAccepted(accepted_socket, address);
    }
  }

  return true;
}
