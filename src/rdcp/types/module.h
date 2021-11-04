#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_MODULE_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_MODULE_H_

#include <cinttypes>
#include <ostream>
#include <string>

#include "rdcp/rdcp_processed_request.h"

struct Module {
  Module() = default;
  Module(std::string name, uint32_t baseAddress, uint32_t size,
         uint32_t checksum, uint32_t timestamp, bool isTls, bool isXbe);

  explicit Module(const RDCPMapResponse &parsed) {
    name = parsed.GetString("name");
    base_address = parsed.GetUInt32("base");
    size = parsed.GetUInt32("size");
    checksum = parsed.GetUInt32("check");
    timestamp = parsed.GetUInt32("timestamp");
    is_tls = parsed.HasKey("tls");
    is_xbe = parsed.HasKey("xbe");
  }

  friend std::ostream &operator<<(std::ostream &os, const Module &loaded);

  std::string name;
  uint32_t base_address{0};
  uint32_t size{0};
  uint32_t checksum{0};
  uint32_t timestamp{0};
  bool is_tls{false};
  bool is_xbe{false};
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_MODULE_H_
