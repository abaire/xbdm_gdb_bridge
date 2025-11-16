#include "mock_xbdm_client_transport.h"

namespace xbdm_gdb_bridge::testing {

ClientTransport::ClientTransport(int sock, const IPAddress& address,
                                 BytesReceivedHandler bytes_received_handler)
    : TCPConnection("MockXBDMClient", sock, address),
      bytes_received_handler_(std::move(bytes_received_handler)) {}

std::shared_ptr<TCPConnection> ClientTransport::CreateNotificationConnection(
    int sock, const IPAddress& address) {
  notification_connection_ = std::make_shared<TCPConnection>(
      "MockXBDMClientNotificationChannel", sock, address);
  return notification_connection_;
}

void ClientTransport::Close() {
  if (notification_connection_) {
    notification_connection_->Close();
    notification_connection_.reset();
  }
  TCPSocketBase::Close();
}

void ClientTransport::OnBytesRead() {
  TCPConnection::OnBytesRead();
  bytes_received_handler_(*this);
}

}  // namespace xbdm_gdb_bridge::testing
