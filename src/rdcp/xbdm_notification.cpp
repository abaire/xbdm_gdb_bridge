#include "xbdm_notification.h"

#include <cstring>

static bool StartsWith(const char *buffer, long buffer_len, const char *prefix, long prefix_len);

XBDMNotification::XBDMNotification(const char *buffer, long buffer_len) {
#define SETIF(prefix, type) \
  if (StartsWith(buffer, buffer_len, prefix, sizeof(prefix) - 1)) { \
    type_ = type;           \
    data_.assign(buffer + sizeof(prefix) - 1, buffer + buffer_len); \
  return;                   \
  }

  SETIF("vx!", NotificationType::VX)
  SETIF("debugstr ", NotificationType::DEBUGSTR)
  SETIF("modload ", NotificationType::MODLOAD)
  SETIF("sectload ", NotificationType::SECTLOAD)
  SETIF("create ", NotificationType::CREATE_THREAD)
  SETIF("terminate ", NotificationType::TERMINATE_THREAD)
  SETIF("execution ", NotificationType::EXECUTION_STATE_CHANGE)
  SETIF("break ", NotificationType::BREAKPOINT)
  SETIF("data ", NotificationType::WATCHPOINT)
  SETIF("singlestep ", NotificationType::SINGLE_STEP)
  SETIF("exception ", NotificationType::EXCEPTION)

  type_ = NotificationType::INVALID;
#undef SETIF
}

static bool StartsWith(const char *buffer, long buffer_len, const char *prefix, long prefix_len) {
  if (prefix_len > buffer_len) {
    return false;
  }

  return !memcmp(buffer, prefix, prefix_len);
}