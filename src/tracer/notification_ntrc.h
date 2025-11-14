#ifndef XBDM_GDB_BRIDGE_SRC_TRACER_NOTIFICATION_NTRC_H_
#define XBDM_GDB_BRIDGE_SRC_TRACER_NOTIFICATION_NTRC_H_

#include <string>

#include "notification/xbdm_notification.h"
#include "ntrc_dyndxt.h"

namespace NTRCTracer {

//! Encapsulates information about an NTRC tracer push notification.
struct NotificationNTRC : XBDMNotification {
  NotificationNTRC(const char* buffer_start, const char* buffer_end);

  [[nodiscard]] NotificationType Type() const override { return NT_CUSTOM; }
  [[nodiscard]] std::string NotificationPrefix() const override;

  std::ostream& WriteStream(std::ostream& os) const override;

  //! The parsed content of the message.
  RDCPMapResponse content;
};

}  // namespace NTRCTracer

#endif  // XBDM_GDB_BRIDGE_SRC_TRACER_NOTIFICATION_NTRC_H_
