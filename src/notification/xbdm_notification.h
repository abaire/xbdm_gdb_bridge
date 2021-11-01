#ifndef XBDM_GDB_BRIDGE_XBDM_NOTIFICATION_H
#define XBDM_GDB_BRIDGE_XBDM_NOTIFICATION_H

#include <vector>

class XBDMNotification {
 public:
  enum NotificationType {
    INVALID = 0,
    VX,
    DEBUGSTR,
    MODLOAD,
    SECTLOAD,
    CREATE_THREAD,
    TERMINATE_THREAD,
    EXECUTION_STATE_CHANGE,
    BREAKPOINT,
    WATCHPOINT,
    SINGLE_STEP,
    EXCEPTION,
  };

 public:
  XBDMNotification(const char *buffer, long buffer_len);

  [[nodiscard]] NotificationType Type() const { return type_; }
  [[nodiscard]] const std::vector<char> &Data() const { return data_; }

 private:
  NotificationType type_;
  std::vector<char> data_;
};

#endif  // XBDM_GDB_BRIDGE_XBDM_NOTIFICATION_H
