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

void ClientTransport::OnBytesRead() {
  TCPConnection::OnBytesRead();
  bytes_received_handler_(*this);
}

}  // namespace xbdm_gdb_bridge::testing
