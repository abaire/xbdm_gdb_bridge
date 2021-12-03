#include "xbdm_notification.h"

#include <cassert>
#include <cstring>
#include <ostream>
#include <regex>

#include "rdcp/rdcp_response_processors.h"
#include "util/logging.h"
#include "util/parsing.h"

static bool StartsWith(const char *buffer, long buffer_len, const char *prefix,
                       long prefix_len);

std::shared_ptr<XBDMNotification> ParseXBDMNotification(const char *buffer,
                                                        long buffer_len) {
#define SETIF(prefix, type)                                         \
  if (StartsWith(buffer, buffer_len, prefix, sizeof(prefix) - 1)) { \
    return std::make_shared<type>(buffer + sizeof(prefix) - 1,      \
                                  buffer + buffer_len);             \
  }

  SETIF("vx!", NotificationVX)
  SETIF("debugstr ", NotificationDebugStr)
  SETIF("modload ", NotificationModuleLoaded)
  SETIF("sectload ", NotificationSectionLoaded)
  SETIF("sectunload ", NotificationSectionUnloaded)
  SETIF("create ", NotificationThreadCreated)
  SETIF("terminate ", NotificationThreadTerminated)
  SETIF("execution ", NotificationExecutionStateChanged)
  SETIF("break ", NotificationBreakpoint)
  SETIF("data ", NotificationWatchpoint)
  SETIF("singlestep ", NotificationSingleStep)
  SETIF("exception ", NotificationException)

#undef SETIF
  std::string notification(buffer, buffer_len);
  LOG(error) << "Uknown notification " << notification;
  assert(false && "Unknown notification type");
  return {};
}

static bool StartsWith(const char *buffer, long buffer_len, const char *prefix,
                       long prefix_len) {
  if (prefix_len > buffer_len) {
    return false;
  }

  return !memcmp(buffer, prefix, prefix_len);
}

NotificationVX::NotificationVX(const char *buffer_start,
                               const char *buffer_end) {
  message.assign(buffer_start, buffer_end);
}

std::ostream &NotificationVX::WriteStream(std::ostream &os) const {
  os << "VX message: " << message;
  return os;
}

// "thread=4 lf string=Test string with newline"
static std::regex notification_regex(
    R"(thread=(\d+)\s+(lf|cr|crlf)?\s*string=(.*))");

NotificationDebugStr::NotificationDebugStr(const char *buffer_start,
                                           const char *buffer_end) {
  std::string buffer(buffer_start, buffer_end);
  std::smatch match;
  if (!std::regex_match(buffer, match, notification_regex)) {
    LOG(error) << "Regex match failed on notification buffer '" << buffer
               << "'";
    thread_id = -1;
    text = "";
    termination = "\n";
    is_terminated = true;
    return;
  }

  thread_id = ParseInt32(match[1]);
  text = match[3];

  if (match[2] == "lf") {
    termination = "\n";
  } else if (match[2] == "cr") {
    termination = "\r";
  } else if (match[2] == "crlf") {
    termination = "\r\n";
  }
  is_terminated = !termination.empty();
}

std::ostream &NotificationDebugStr::WriteStream(std::ostream &os) const {
  os << "DebugStr: thread_id: " << thread_id << " text: " << text
     << termination;
  return os;
}

NotificationModuleLoaded::NotificationModuleLoaded(const char *buffer_start,
                                                   const char *buffer_end)
    : module(RDCPMapResponse(buffer_start, buffer_end)) {}

std::ostream &NotificationModuleLoaded::WriteStream(std::ostream &os) const {
  os << "ModuleLoaded: " << module;
  return os;
}

NotificationSectionLoaded::NotificationSectionLoaded(const char *buffer_start,
                                                     const char *buffer_end)
    : section(RDCPMapResponse(buffer_start, buffer_end)) {}

std::ostream &NotificationSectionLoaded::WriteStream(std::ostream &os) const {
  os << "SectionLoaded: " << section;
  return os;
}

NotificationSectionUnloaded::NotificationSectionUnloaded(
    const char *buffer_start, const char *buffer_end)
    : section(RDCPMapResponse(buffer_start, buffer_end)) {}

std::ostream &NotificationSectionUnloaded::WriteStream(std::ostream &os) const {
  return os << "SectionUnloaded: " << section;
}

NotificationThreadCreated::NotificationThreadCreated(const char *buffer_start,
                                                     const char *buffer_end) {
  RDCPMapResponse parsed(buffer_start, buffer_end);
  thread_id = parsed.GetDWORD("thread");
}

std::ostream &NotificationThreadCreated::WriteStream(std::ostream &os) const {
  os << "Thread created: " << thread_id;
  return os;
}

NotificationThreadTerminated::NotificationThreadTerminated(
    const char *buffer_start, const char *buffer_end) {
  RDCPMapResponse parsed(buffer_start, buffer_end);
  thread_id = parsed.GetDWORD("thread");
}

std::ostream &NotificationThreadTerminated::WriteStream(
    std::ostream &os) const {
  os << "Thread terminated: " << thread_id;
  return os;
}

NotificationExecutionStateChanged::NotificationExecutionStateChanged(
    const char *buffer_start, const char *buffer_end) {
  RDCPMapResponse parsed(buffer_start, buffer_end);
  if (parsed.HasKey("stopped")) {
    state = S_STOPPED;
  } else if (parsed.HasKey("started")) {
    state = S_STARTED;
  } else if (parsed.HasKey("rebooting")) {
    state = S_REBOOTING;
  } else if (parsed.HasKey("pending")) {
    state = S_PENDING;
  } else {
    state = S_INVALID;
  }
}

std::ostream &NotificationExecutionStateChanged::WriteStream(
    std::ostream &os) const {
  os << "Execution state changed: ";
  switch (state) {
    case ExecutionState::S_STOPPED:
      os << "stopped";
      break;
    case ExecutionState::S_STARTED:
      os << "started";
      break;
    case ExecutionState::S_REBOOTING:
      os << "rebooting";
      break;
    case ExecutionState::S_PENDING:
      os << "pending";
      break;
    default:
      os << "INAVLID " << state;
      break;
  }

  return os;
}

NotificationBreakpoint::NotificationBreakpoint(const char *buffer_start,
                                               const char *buffer_end) {
  RDCPMapResponse parsed(buffer_start, buffer_end);
  thread_id = parsed.GetDWORD("thread");
  address = parsed.GetUInt32("addr");
  flags = parsed.valueless_keys;
}

std::ostream &NotificationBreakpoint::WriteStream(std::ostream &os) const {
  os << "Break thread_id: " << thread_id << " address: 0x" << std::setw(8)
     << std::setfill('0') << std::hex << address;
  if (!flags.empty()) {
    os << " flags:";
    for (auto &f : flags) {
      os << " " << f;
    }
  }
  return os;
}

NotificationWatchpoint::NotificationWatchpoint(const char *buffer_start,
                                               const char *buffer_end) {
  RDCPMapResponse parsed(buffer_start, buffer_end);
  thread_id = parsed.GetDWORD("thread");
  accessed_address = parsed.GetUInt32("addr");
  if (parsed.HasKey("read")) {
    type = AT_READ;
    address = parsed.GetUInt32("read");
  } else if (parsed.HasKey("write")) {
    type = AT_WRITE;
    address = parsed.GetUInt32("write");
  } else if (parsed.HasKey("execute")) {
    type = AT_EXECUTE;
    address = parsed.GetUInt32("execute");
  } else {
    type = AT_INVALID;
    address = 0;
  }

  flags = parsed.valueless_keys;
  if (flags.find("stop") != flags.end()) {
    should_break = true;
    flags.erase("stop");
  }
}

std::ostream &NotificationWatchpoint::WriteStream(std::ostream &os) const {
  os << "Watchpoint type: " << type << " thread_id: " << thread_id
     << " address: 0x" << std::setw(8) << std::setfill('0') << std::hex
     << address << " accessed_address: 0x" << accessed_address;

  if (!flags.empty()) {
    os << " flags:";
    for (auto &f : flags) {
      os << " " << f;
    }
  }
  return os;
}

NotificationSingleStep::NotificationSingleStep(const char *buffer_start,
                                               const char *buffer_end) {
  RDCPMapResponse parsed(buffer_start, buffer_end);
  thread_id = parsed.GetDWORD("thread");
  address = parsed.GetUInt32("addr");
}

std::ostream &NotificationSingleStep::WriteStream(std::ostream &os) const {
  os << "SingleStep thread_id: " << thread_id << " address: 0x" << std::hex
     << std::setw(8) << std::setfill('0') << address;
  return os;
}

NotificationException::NotificationException(const char *buffer_start,
                                             const char *buffer_end) {
  RDCPMapResponse parsed(buffer_start, buffer_end);
  code = parsed.GetDWORD("code");
  thread_id = parsed.GetDWORD("thread");
  address = parsed.GetUInt32("address");
  read = parsed.GetUInt32("read");
  flags = parsed.valueless_keys;
}

std::ostream &NotificationException::WriteStream(std::ostream &os) const {
  os << "Exception: code: 0x" << std::setw(8) << std::setfill('0') << std::hex
     << code << " thread_id: " << std::dec << thread_id << " address: 0x"
     << std::hex << address << " read: 0x" << read;

  if (!flags.empty()) {
    os << " flags:";
    for (auto &f : flags) {
      os << " " << f;
    }
  }
  return os;
}
