#ifndef XBDM_GDB_BRIDGE_SRC_NET_IP_TRANSPORT_H_
#define XBDM_GDB_BRIDGE_SRC_NET_IP_TRANSPORT_H_

#include <sys/socket.h>

#include <cctype>
#include <list>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "address.h"

class IPTransport {
 public:
  explicit IPTransport(std::string name, int sock = -1)
      : name_(std::move(name)), socket_(sock) {}
  IPTransport(std::string name, int sock, Address address)
      : name_(std::move(name)), socket_(sock), address_(std::move(address)) {}

  void SetConnection(int sock, const Address &address);
  void ShiftReadBuffer(long shift_bytes);
  size_t BytesAvailable();
  virtual void Close();
  void DropReceiveBuffer();
  void DropSendBuffer();

  void Send(const std::vector<uint8_t> &buffer) {
    Send(buffer.data(), buffer.size());
  }
  void Send(uint8_t const *buffer, size_t len);

  [[nodiscard]] bool IsConnected() const { return socket_ >= 0; }
  [[nodiscard]] virtual bool HasBufferedData();

  void Select(fd_set &read_fds, fd_set &write_fds, fd_set &except_fds);
  void Process(const fd_set &read_fds, const fd_set &write_fds,
               const fd_set &except_fds);

 protected:
  virtual void OnBytesRead() {}

  virtual void DoReceive();
  virtual void DoSend();

  std::vector<uint8_t>::iterator FirstIndexOf(uint8_t element);
  std::vector<uint8_t>::iterator FirstIndexOf(const std::vector<uint8_t> &pattern);

 protected:
  std::string name_;

  std::mutex socket_lock_;
  int socket_{-1};
  Address address_;

  std::mutex read_lock_;
  std::vector<uint8_t> read_buffer_;
  std::mutex write_lock_;
  std::vector<uint8_t> write_buffer_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_NET_IP_TRANSPORT_H_
