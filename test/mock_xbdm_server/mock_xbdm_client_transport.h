#ifndef XBDM_GDB_BRIDGE_TEST_MOCK_XBDM_SERVER_MOCK_XBDM_CLIENT_TRANSPORT_H_
#define XBDM_GDB_BRIDGE_TEST_MOCK_XBDM_SERVER_MOCK_XBDM_CLIENT_TRANSPORT_H_

#include <functional>
#include <memory>

#include "net/ip_address.h"
#include "net/tcp_connection.h"

namespace xbdm_gdb_bridge::testing {

class ClientTransport : public TCPConnection {
 public:
  typedef std::function<void(ClientTransport&)> BytesReceivedHandler;

 public:
  ClientTransport(int sock, const IPAddress& address,
                  BytesReceivedHandler bytes_received_handler);

  void Close() override;

  /**
   * Connects to a notification server at the given address.
   */
  std::shared_ptr<TCPConnection> CreateNotificationConnection(
      const IPAddress& address);

  /**
   * Closes and releases the associated notification connection, if one exists.
   */
  void CloseNotificationConnection();

  [[nodiscard]] std::shared_ptr<TCPConnection> GetNotificationConnection()
      const {
    return notification_connection_;
  }

  std::recursive_mutex& ReadLock() { return read_lock_; }

  std::vector<uint8_t>& ReadBuffer() { return read_buffer_; }

 protected:
  void OnBytesRead() override;

 private:
  std::shared_ptr<TCPConnection> notification_connection_;
  BytesReceivedHandler bytes_received_handler_;
};

}  // namespace xbdm_gdb_bridge::testing

#endif  // XBDM_GDB_BRIDGE_TEST_MOCK_XBDM_SERVER_MOCK_XBDM_CLIENT_TRANSPORT_H_
