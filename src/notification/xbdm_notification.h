#ifndef XBDM_GDB_BRIDGE_XBDM_NOTIFICATION_H
#define XBDM_GDB_BRIDGE_XBDM_NOTIFICATION_H

#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "rdcp/types/execution_state.h"
#include "rdcp/types/module.h"
#include "rdcp/types/section.h"

enum NotificationType {
  NT_INVALID,
  NT_VX,
  NT_DEBUGSTR,
  NT_MODULE_LOADED,
  NT_SECTION_LOADED,
  NT_SECTION_UNLOADED,
  NT_THREAD_CREATED,
  NT_THREAD_TERMINATED,
  NT_EXECUTION_STATE_CHANGED,
  NT_BREAKPOINT,
  NT_WATCHPOINT,
  NT_SINGLE_STEP,
  NT_EXCEPTION,
};

struct XBDMNotification {
  [[nodiscard]] virtual NotificationType Type() const = 0;

  friend std::ostream &operator<<(std::ostream &os,
                                  const XBDMNotification &base) {
    return base.WriteStream(os);
  }

 protected:
  virtual std::ostream &WriteStream(std::ostream &os) const = 0;
};

struct NotificationVX : XBDMNotification {
  NotificationVX(const char *buffer_start, const char *buffer_end);
  [[nodiscard]] NotificationType Type() const override { return NT_VX; }
  std::ostream &WriteStream(std::ostream &os) const override;
  std::string message;
};

struct NotificationDebugStr : XBDMNotification {
  NotificationDebugStr(const char *buffer_start, const char *buffer_end);
  [[nodiscard]] NotificationType Type() const override { return NT_DEBUGSTR; }
  std::ostream &WriteStream(std::ostream &os) const override;
  int thread_id;
  std::string text;
  bool is_terminated;
};

struct NotificationModuleLoaded : XBDMNotification {
  NotificationModuleLoaded(const char *buffer_start, const char *buffer_end);
  [[nodiscard]] NotificationType Type() const override {
    return NT_MODULE_LOADED;
  }
  std::ostream &WriteStream(std::ostream &os) const override;
  Module module;
};

struct NotificationSectionLoaded : XBDMNotification {
  NotificationSectionLoaded(const char *buffer_start, const char *buffer_end);
  [[nodiscard]] NotificationType Type() const override {
    return NT_SECTION_LOADED;
  }
  std::ostream &WriteStream(std::ostream &os) const override;
  Section section;
};

struct NotificationSectionUnloaded : XBDMNotification {
  NotificationSectionUnloaded(const char *buffer_start, const char *buffer_end);
  [[nodiscard]] NotificationType Type() const override {
    return NT_SECTION_UNLOADED;
  }
  std::ostream &WriteStream(std::ostream &os) const override;
  Section section;
};

struct NotificationThreadCreated : XBDMNotification {
  NotificationThreadCreated(const char *buffer_start, const char *buffer_end);
  [[nodiscard]] NotificationType Type() const override {
    return NT_THREAD_CREATED;
  }
  std::ostream &WriteStream(std::ostream &os) const override;
  int thread_id;
};

struct NotificationThreadTerminated : XBDMNotification {
  NotificationThreadTerminated(const char *buffer_start,
                               const char *buffer_end);
  [[nodiscard]] NotificationType Type() const override {
    return NT_THREAD_TERMINATED;
  }
  std::ostream &WriteStream(std::ostream &os) const override;
  int thread_id;
};

struct NotificationExecutionStateChanged : XBDMNotification {
  NotificationExecutionStateChanged(const char *buffer_start,
                                    const char *buffer_end);
  [[nodiscard]] NotificationType Type() const override {
    return NT_EXECUTION_STATE_CHANGED;
  }
  std::ostream &WriteStream(std::ostream &os) const override;

  ExecutionState state;
};

struct NotificationBreakpoint : XBDMNotification {
  NotificationBreakpoint(const char *buffer_start, const char *buffer_end);
  [[nodiscard]] NotificationType Type() const override { return NT_BREAKPOINT; }
  std::ostream &WriteStream(std::ostream &os) const override;
  int thread_id;
  uint32_t address;
  std::set<std::string> flags;
};

struct NotificationWatchpoint : XBDMNotification {
  enum AccessType {
    AT_INVALID = -1,
    AT_READ,
    AT_WRITE,
    AT_EXECUTE,
  };
  NotificationWatchpoint(const char *buffer_start, const char *buffer_end);
  [[nodiscard]] NotificationType Type() const override { return NT_WATCHPOINT; }
  std::ostream &WriteStream(std::ostream &os) const override;
  AccessType type;
  int thread_id;
  uint32_t address;
  uint32_t accessed_address;
  std::set<std::string> flags;
};

struct NotificationSingleStep : XBDMNotification {
  NotificationSingleStep(const char *buffer_start, const char *buffer_end);
  [[nodiscard]] NotificationType Type() const override {
    return NT_SINGLE_STEP;
  }
  std::ostream &WriteStream(std::ostream &os) const override;
  int thread_id;
  uint32_t address;
};

struct NotificationException : XBDMNotification {
  NotificationException(const char *buffer_start, const char *buffer_end);
  [[nodiscard]] NotificationType Type() const override { return NT_EXCEPTION; }
  std::ostream &WriteStream(std::ostream &os) const override;
  uint32_t code;
  int thread_id;
  uint32_t address;
  uint32_t read;
  std::set<std::string> flags;
};

std::shared_ptr<XBDMNotification> ParseXBDMNotification(const char *message,
                                                        long message_len);

#endif  // XBDM_GDB_BRIDGE_XBDM_NOTIFICATION_H
