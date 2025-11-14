#ifndef XBDM_GDB_BRIDGE_SRC_RDCP_XBDM_STOP_REASONS_H_
#define XBDM_GDB_BRIDGE_SRC_RDCP_XBDM_STOP_REASONS_H_

#include <signal.h>

#include <ostream>
#include <utility>

#include "rdcp/rdcp_processed_request.h"

enum StopReasonType {
  SRT_UNKNOWN,
  SRT_DEBUGSTR,
  SRT_ASSERTION,
  SRT_BREAKPOINT,
  SRT_SINGLE_STEP,
  SRT_WATCHPOINT,
  SRT_EXECUTION_STATE_CHANGED,
  SRT_EXCEPTION,
  SRT_THREAD_CREATED,
  SRT_THREAD_TERMINATED,
  SRT_MODULE_LOADED,
  SRT_SECTION_LOADED,
  SRT_SECTION_UNLOADED,
  SRT_RIP,
  SRT_RIP_STOP,
};

struct StopReasonBase_ {
  explicit StopReasonBase_(StopReasonType type, int signal)
      : type(type), signal(signal) {}

  friend std::ostream& operator<<(std::ostream& os,
                                  const StopReasonBase_& base) {
    return base.WriteStream(os);
  }

  StopReasonType type;
  int signal;

 protected:
  virtual std::ostream& WriteStream(std::ostream& os) const = 0;
};

struct StopReasonTrapBase_ : StopReasonBase_ {
  explicit StopReasonTrapBase_(StopReasonType type)
      : StopReasonBase_(type, SIGTRAP) {}
};

struct StopReasonUnknown : StopReasonTrapBase_ {
  StopReasonUnknown() : StopReasonTrapBase_(SRT_UNKNOWN) {}
  std::ostream& WriteStream(std::ostream& os) const override {
    return os << "Unknown reason";
  }
};

struct StopReasonThreadContextBase_ : StopReasonTrapBase_ {
  explicit StopReasonThreadContextBase_(StopReasonType type, std::string name,
                                        RDCPMapResponse& parsed)
      : StopReasonTrapBase_(type), name(std::move(name)) {
    thread_id = parsed.GetDWORD("thread");
  }

  std::ostream& WriteStream(std::ostream& os) const override {
    return os << name << " on thread " << thread_id;
  }

  std::string name;
  int thread_id;
};

struct StopReasonDebugstr : StopReasonThreadContextBase_ {
  explicit StopReasonDebugstr(RDCPMapResponse& parsed)
      : StopReasonThreadContextBase_(SRT_DEBUGSTR, "debugstr", parsed) {}
};

struct StopReasonAssertion : StopReasonThreadContextBase_ {
  explicit StopReasonAssertion(RDCPMapResponse& parsed)
      : StopReasonThreadContextBase_(SRT_ASSERTION, "assert prompt", parsed) {}
};

struct StopReasonThreadAndAddressBase_ : StopReasonTrapBase_ {
  explicit StopReasonThreadAndAddressBase_(StopReasonType type,
                                           std::string name,
                                           RDCPMapResponse& parsed)
      : StopReasonTrapBase_(type), name(std::move(name)) {
    thread_id = parsed.GetDWORD("thread");
    address = parsed.GetUInt32("Address");
  }

  std::ostream& WriteStream(std::ostream& os) const override {
    return os << name << " on thread " << thread_id << " at 0x" << std::hex
              << std::setfill('0') << std::setw(8) << address;
  }

  std::string name;
  int thread_id;
  uint32_t address;
};

struct StopReasonBreakpoint : StopReasonThreadAndAddressBase_ {
  explicit StopReasonBreakpoint(RDCPMapResponse& parsed)
      : StopReasonThreadAndAddressBase_(SRT_BREAKPOINT, "breakpoint", parsed) {}
};

struct StopReasonSingleStep : StopReasonThreadAndAddressBase_ {
  explicit StopReasonSingleStep(RDCPMapResponse& parsed)
      : StopReasonThreadAndAddressBase_(SRT_SINGLE_STEP, "single step",
                                        parsed) {}
};

struct StopReasonDataBreakpoint : StopReasonTrapBase_ {
  enum AccessType {
    AT_UNKNOWN = -1,
    AT_READ,
    AT_WRITE,
    AT_EXECUTE,
  };
  explicit StopReasonDataBreakpoint(RDCPMapResponse& parsed)
      : StopReasonTrapBase_(SRT_WATCHPOINT) {
    thread_id = parsed.GetDWORD("thread");
    address = parsed.GetUInt32("addr");

    auto value = parsed.GetOptionalDWORD("read");
    if (value.has_value()) {
      access_type = AT_READ;
      access_address = value.value();
      return;
    }

    value = parsed.GetOptionalDWORD("write");
    if (value.has_value()) {
      access_type = AT_WRITE;
      access_address = value.value();
      return;
    }
    value = parsed.GetOptionalDWORD("execute");
    if (value.has_value()) {
      access_type = AT_EXECUTE;
      access_address = value.value();
      return;
    }

    access_type = AT_UNKNOWN;
    access_address = 0;
  }

  std::ostream& WriteStream(std::ostream& os) const override {
    std::string access;
    switch (access_type) {
      case AT_READ:
        access = "read";
        break;

      case AT_WRITE:
        access = "write";
        break;

      case AT_EXECUTE:
        access = "execute";
        break;

      default:
        access = "unknown";
        break;
    }

    return os << "Watch breakpoint on thread " << thread_id << " at 0x"
              << std::hex << std::setfill('0') << std::setw(8) << address << " "
              << access << "@0x" << access_address;
  }

  int thread_id;
  uint32_t address;
  AccessType access_type;
  uint32_t access_address;
};

struct StopReasonExecutionStateChange : StopReasonTrapBase_ {
  enum State {
    UNKNOWN = -1,
    STOPPED,
    STARTED,
    REBOOTING,
    PENDING,
  };
  explicit StopReasonExecutionStateChange(RDCPMapResponse& parsed)
      : StopReasonTrapBase_(SRT_EXECUTION_STATE_CHANGED) {
    if (parsed.HasKey("stopped")) {
      state = STOPPED;
      state_name = "stopped";
    } else if (parsed.HasKey("started")) {
      state = STARTED;
      state_name = "started";
    } else if (parsed.HasKey("rebooting")) {
      state = REBOOTING;
      state_name = "rebooting";
    } else if (parsed.HasKey("pending")) {
      state = PENDING;
      state_name = "pending";
    } else {
      state = UNKNOWN;
      state_name = "unknown";
    }
  }

  std::ostream& WriteStream(std::ostream& os) const override {
    return os << "execution state changed to " << state_name;
  }

  State state;
  std::string state_name;
};

struct StopReasonException : StopReasonTrapBase_ {
  enum Type {
    UNKNOWN = 0,
    GENERAL,
    ACCESS_VIOLATION_READ,
    ACCESS_VIOLATION_WRITE,
  };

  explicit StopReasonException(RDCPMapResponse& parsed)
      : StopReasonTrapBase_(SRT_EXCEPTION) {
    exception = parsed.GetUInt32("code");
    thread_id = parsed.GetDWORD("thread");
    address = parsed.GetUInt32("Address");
    is_first_chance_exception = parsed.HasKey("first");
    is_non_continuable = parsed.HasKey("noncont");

    if (parsed.HasKey("read")) {
      type = ACCESS_VIOLATION_READ;
      access_violation_address = parsed.GetUInt32("read");
    } else if (parsed.HasKey("write")) {
      type = ACCESS_VIOLATION_WRITE;
      access_violation_address = parsed.GetUInt32("write");
    } else {
      type = GENERAL;
      nparams = parsed.GetDWORD("nparams");
      params = parsed.GetDWORD("params");
    }
  }

  std::ostream& WriteStream(std::ostream& os) const override {
    os << "exception on thread " << thread_id << " at 0x" << std::hex
       << std::setfill('0') << std::setw(8) << address;
    switch (type) {
      case ACCESS_VIOLATION_READ:
        os << std::hex << std::setfill('0') << std::setw(8)
           << " read violation at 0x" << access_violation_address;
        break;

      case ACCESS_VIOLATION_WRITE:
        os << std::hex << std::setfill('0') << std::setw(8)
           << " write violation at 0x" << access_violation_address;
        break;

      case GENERAL:
        os << " nparams: " << nparams << " params: 0x" << std::hex
           << std::setfill('0') << std::setw(8) << params;
        break;

      case UNKNOWN:
        break;
    }
    return os;
  }

  uint32_t exception;
  int thread_id;
  uint32_t address;
  bool is_first_chance_exception;
  bool is_non_continuable;
  Type type;
  uint32_t access_violation_address;
  int32_t nparams;
  int32_t params;
};

struct StopReasonCreateThread : StopReasonTrapBase_ {
  explicit StopReasonCreateThread(RDCPMapResponse& parsed)
      : StopReasonTrapBase_(SRT_THREAD_CREATED) {
    thread_id = parsed.GetDWORD("thread");
    start_address = parsed.GetDWORD("start");
  }

  std::ostream& WriteStream(std::ostream& os) const override {
    return os << "create thread " << thread_id << " start Address 0x"
              << std::hex << std::setfill('0') << std::setw(8) << start_address;
  }

  int thread_id;
  uint32_t start_address;
};

struct StopReasonTerminateThread : StopReasonThreadContextBase_ {
  explicit StopReasonTerminateThread(RDCPMapResponse& parsed)
      : StopReasonThreadContextBase_(SRT_THREAD_TERMINATED, "terminate thread",
                                     parsed) {}
};

struct StopReasonModuleLoad : StopReasonTrapBase_ {
  explicit StopReasonModuleLoad(RDCPMapResponse& parsed)
      : StopReasonTrapBase_(SRT_MODULE_LOADED) {
    name = parsed.GetString("name");
    base_address = parsed.GetUInt32("base");
    size = parsed.GetUInt32("size");
    checksum = parsed.GetUInt32("check");
    timestamp = parsed.GetUInt32("timestamp");
    is_tls = parsed.HasKey("tls");
    is_xbe = parsed.HasKey("xbe");
  }

  std::ostream& WriteStream(std::ostream& os) const override {
    return os << "Module load: name: " << name << " base_address: 0x"
              << std::hex << std::setfill('0') << std::setw(8) << base_address
              << " size: " << std::dec << size << " checksum: 0x" << std::hex
              << checksum << " timestamp: 0x" << timestamp
              << " is_tls: " << is_tls << " is_xbe: " << is_xbe;
  }

  std::string name;
  uint32_t base_address;
  uint32_t size;
  uint32_t checksum;
  uint32_t timestamp;
  bool is_tls;
  bool is_xbe;
};

struct StopReasonSectionActionBase_ : StopReasonTrapBase_ {
  explicit StopReasonSectionActionBase_(StopReasonType type, std::string action,
                                        RDCPMapResponse& parsed)
      : StopReasonTrapBase_(type), action(std::move(action)) {
    name = parsed.GetString("name");
    base_address = parsed.GetUInt32("base");
    size = parsed.GetUInt32("size");
    index = parsed.GetUInt32("index");
    flags = parsed.GetUInt32("flags");
  }

  std::ostream& WriteStream(std::ostream& os) const override {
    return os << action << ": name: " << name << " base_address: 0x" << std::hex
              << std::setfill('0') << std::setw(8) << base_address
              << " size: " << std::dec << size << " index: " << index
              << " flags: 0x" << std::hex << flags;
  }

  std::string action;
  std::string name;
  uint32_t base_address;
  uint32_t size;
  uint32_t index;
  uint32_t flags;
};

struct StopReasonSectionLoad : StopReasonSectionActionBase_ {
  explicit StopReasonSectionLoad(RDCPMapResponse& parsed)
      : StopReasonSectionActionBase_(SRT_SECTION_LOADED, "section load",
                                     parsed) {}
};

struct StopReasonSectionUnload : StopReasonSectionActionBase_ {
  explicit StopReasonSectionUnload(RDCPMapResponse& parsed)
      : StopReasonSectionActionBase_(SRT_SECTION_UNLOADED, "section unload",
                                     parsed) {}
};

struct StopReasonRIPBase_ : StopReasonBase_ {
  explicit StopReasonRIPBase_(StopReasonType type, std::string name,
                              RDCPMapResponse& parsed)
      : StopReasonBase_(type, SIGABRT), name(std::move(name)) {
    thread_id = parsed.GetDWORD("thread");
    message = parsed.GetString("message");
  }

  std::ostream& WriteStream(std::ostream& os) const override {
    os << name << " on thread " << thread_id;
    if (!message.empty()) {
      os << " \"" << message << "\"";
    }
    return os;
  }

  std::string name;
  int thread_id;
  std::string message;
};

struct StopReasonRIP : StopReasonRIPBase_ {
  explicit StopReasonRIP(RDCPMapResponse& parsed)
      : StopReasonRIPBase_(SRT_RIP, "RIP", parsed) {}
};

struct StopReasonRIPStop : StopReasonRIPBase_ {
  explicit StopReasonRIPStop(RDCPMapResponse& parsed)
      : StopReasonRIPBase_(SRT_RIP_STOP, "RIPStop", parsed) {}
};

#endif  // XBDM_GDB_BRIDGE_SRC_RDCP_XBDM_STOP_REASONS_H_
