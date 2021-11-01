#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_XBOX_INTERFACE_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_XBOX_INTERFACE_H_

#include <deque>
#include <memory>
#include <string>

#include "net/address.h"
#include "net/select_thread.h"
#include "rdcp/xbdm_transport.h"
#include "xbox/notification_server.h"

class XBOXInterface {
  public:
  XBOXInterface(std::string name, Address  xbox_address);

  void Start();
  void Stop();

 private:
  void OnNotificationChannelConnected(int sock, Address &address);
  void OnNotificationReceived(XBDMNotification& notification);

 private:
  std::string name_;
  Address xbox_address_;

  std::shared_ptr<SelectThread> select_thread_;
  std::shared_ptr<XBDMTransport> xbdm_transport_;
  std::shared_ptr<NotificationServer> notification_server_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_XBOX_INTERFACE_H_
