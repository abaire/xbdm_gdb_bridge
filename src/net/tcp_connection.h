#ifndef XBDM_GDB_BRIDGE_SRC_NET_IP_TRANSPORT_H_
#define XBDM_GDB_BRIDGE_SRC_NET_IP_TRANSPORT_H_

#include <sys/socket.h>

#include <cctype>
#include <list>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "tcp_socket_base.h"

class TCPConnection : public TCPSocketBase {
 public:
  explicit TCPConnection(std::string name, int sock = -1) : TCPSocketBase(std::move(name), sock) {}
  TCPConnection(std::string name, int sock, Address address) : TCPSocketBase(std::move(name), sock, std::move(address)) {}

  void ShiftReadBuffer(long shift_bytes);
  size_t BytesAvailable();
  void DropReceiveBuffer();
  void DropSendBuffer();

  void Send(const std::vector<uint8_t> &buffer) {
    Send(buffer.data(), buffer.size());
  }
  void Send(uint8_t const *buffer, size_t len);

  [[nodiscard]] virtual bool HasBufferedData();

  int Select(fd_set &read_fds, fd_set &write_fds, fd_set &except_fds) override;
  bool Process(const fd_set &read_fds, const fd_set &write_fds, const fd_set &except_fds) override;

 protected:
  virtual void OnBytesRead() {}

  virtual void DoReceive();
  virtual void DoSend();

  std::vector<uint8_t>::iterator FirstIndexOf(uint8_t element);
  std::vector<uint8_t>::iterator FirstIndexOf(const std::vector<uint8_t> &pattern);

 protected:
  std::mutex read_lock_;
  std::vector<uint8_t> read_buffer_;
  std::mutex write_lock_;
  std::vector<uint8_t> write_buffer_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_NET_IP_TRANSPORT_H_
