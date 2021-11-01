#ifndef XBDM_GDB_BRIDGE_SHELL_H
#define XBDM_GDB_BRIDGE_SHELL_H

#include "rdcp/xbdm_notification_transport.h"
#include "rdcp/xbdm_transport.h"

class Shell {
 private:
  XBDMTransport xbdm_transport_;
  XBDMNotificationTransport xbdm_notification_transport_;
};

#endif  // XBDM_GDB_BRIDGE_SHELL_H
