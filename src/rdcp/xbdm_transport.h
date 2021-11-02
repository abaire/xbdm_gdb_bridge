#ifndef XBDM_GDB_BRIDGE_SRC_RDCP_XBDM_TRANSPORT_H_
#define XBDM_GDB_BRIDGE_SRC_RDCP_XBDM_TRANSPORT_H_

#include <deque>
#include <memory>
#include <mutex>

#include "net/tcp_connection.h"
#include "rdcp/rdcp_request.h"

class XBDMTransport : public TCPConnection {
 public:
  enum ConnectionState { INIT = 0, CONNECTED, AWAITING_RESPONSE };

 public:
  explicit XBDMTransport(std::string name) : TCPConnection(std::move(name)) {}

  bool Connect(const IPAddress& address);
  void Close() override;

  [[nodiscard]] ConnectionState State() const { return state_; }
  [[nodiscard]] bool CanProcessCommands() const { return state_ >= CONNECTED; }

  void SetConnected();

  void Send(const std::shared_ptr<RDCPRequest>& request);

 protected:
  void OnBytesRead() override;

 private:
  void WriteNextRequest();

 private:
  ConnectionState state_{ConnectionState::INIT};

  std::mutex request_queue_lock_;
  std::deque<std::shared_ptr<RDCPRequest>> request_queue_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_RDCP_XBDM_TRANSPORT_H_
