#include "mock_xbdm_client_transport.h"

#include "util/logging.h"

namespace xbdm_gdb_bridge::testing {

constexpr const char kTagMockServer[] = "MockXBDM";
#define LOG_SERVER(lvl) \
  LOG_TAGGED(lvl, xbdm_gdb_bridge::testing::kTagMockServer)

ClientTransport::ClientTransport(int sock, const IPAddress& address,
                                 BytesReceivedHandler bytes_received_handler)
    : TCPConnection("MockXBDMClient", sock, address),
      bytes_received_handler_(std::move(bytes_received_handler)) {}

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

std::shared_ptr<TCPConnection> ClientTransport::CreateNotificationConnection(
    const IPAddress& address) {
  int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0) {
    return nullptr;
  }

  const struct sockaddr_in& addr = address.Address();
  if (connect(sock, reinterpret_cast<struct sockaddr const*>(&addr),
              sizeof(addr))) {
    LOG_SERVER(error) << "notification channel connect failed " << errno;
    close(sock);
    sock = -1;
    return nullptr;
  }

  notification_connection_ =
      std::make_shared<TCPConnection>(name_ + "_Notif", sock, address);
  return notification_connection_;
}

void ClientTransport::CloseNotificationConnection() {
  if (notification_connection_) {
    notification_connection_->Close();
    notification_connection_.reset();
  }
}

}  // namespace xbdm_gdb_bridge::testing
