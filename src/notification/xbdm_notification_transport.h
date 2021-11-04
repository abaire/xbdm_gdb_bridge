#ifndef XBDM_GDB_BRIDGE_XBDMNOTIFICATIONTRANSPORT_H
#define XBDM_GDB_BRIDGE_XBDMNOTIFICATIONTRANSPORT_H

#include <functional>
#include <memory>

#include "net/tcp_connection.h"
#include "xbdm_notification.h"

class XBDMNotificationTransport : public TCPConnection {
 public:
  typedef std::function<void(std::shared_ptr<XBDMNotification>)>
      NotificationHandler;

 public:
  explicit XBDMNotificationTransport(std::string name, int sock,
                                     const IPAddress &address,
                                     NotificationHandler handler);

  [[nodiscard]] bool IsHelloReceived() const { return hello_received_; }

 protected:
  void OnBytesRead() override;
  void HandleNotification(const char *message, long message_len);

 private:
  NotificationHandler notification_handler_;
  bool hello_received_;
};

#endif  // XBDM_GDB_BRIDGE_XBDMNOTIFICATIONTRANSPORT_H
