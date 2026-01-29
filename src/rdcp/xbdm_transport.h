#ifndef XBDM_GDB_BRIDGE_SRC_RDCP_XBDM_TRANSPORT_H_
#define XBDM_GDB_BRIDGE_SRC_RDCP_XBDM_TRANSPORT_H_

#include <deque>
#include <memory>
#include <mutex>

#include "configure.h"
#include "net/tcp_connection.h"

#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
#include "util/timer.h"
#endif

class RDCPRequest;
class RDCPResponse;

class XBDMTransport : public TCPConnection {
 public:
  enum class ConnectionState {
    DISCONNECTED = -1,
    INIT = 0,
    CONNECTED,
    AWAITING_RESPONSE
  };

 public:
  explicit XBDMTransport(std::string name) : TCPConnection(std::move(name)) {}

  bool Connect(const IPAddress& address);
  void Close() override;

  [[nodiscard]] ConnectionState State() const { return state_; }
  [[nodiscard]] bool CanProcessCommands() const {
    return state_ >= ConnectionState::CONNECTED;
  }

  void SetConnected();

  void Send(const std::shared_ptr<RDCPRequest>& request);

  void NotifyRemoved() override {
    state_ = ConnectionState::DISCONNECTED;
    is_shutdown_ = true;
  }

 protected:
  void OnBytesRead() override;
  void HandleInitialConnectResponse(
      const std::shared_ptr<RDCPResponse>& response);

 private:
  void WriteNextRequest();

 private:
  ConnectionState state_{ConnectionState::INIT};

  std::recursive_mutex request_queue_lock_;
  std::deque<std::shared_ptr<RDCPRequest>> request_queue_;

#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
  Timer request_sent_;
#endif
};

#endif  // XBDM_GDB_BRIDGE_SRC_RDCP_XBDM_TRANSPORT_H_
