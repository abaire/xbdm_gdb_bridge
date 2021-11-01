#ifndef XBDM_GDB_BRIDGE_SRC_RDCP_XBDM_TRANSPORT_H_
#define XBDM_GDB_BRIDGE_SRC_RDCP_XBDM_TRANSPORT_H_

#include <mutex>

#include "net/ip_transport.h"
#include "rdcp/rdcp_request.h"

class XBDMTransport : public IPTransport {
 public:
  enum ConnectionState { INIT = 0, CONNECTED, AWAITING_RESPONSE };

 public:
  void Close() override;

  [[nodiscard]] ConnectionState State() const { return state_; }
  [[nodiscard]] bool CanProcessCommands() const { return state_ >= CONNECTED; }

  void SetConnected();

  void Send(const RDCPRequest &request);

 protected:
  void OnBytesRead() override;

 private:
  void WriteNextRequest();

 private:
  ConnectionState state_;

  std::mutex request_queue_lock_;
  std::deque<RDCPRequest> request_queue_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_RDCP_XBDM_TRANSPORT_H_
