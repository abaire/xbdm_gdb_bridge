#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_SECTION_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_SECTION_H_

#include <cinttypes>
#include <string>

#include "rdcp/rdcp_processed_request.h"

struct Section {
  Section(const RDCPMapResponse &parsed) {
    name = parsed.GetString("name");
    base_address = parsed.GetUInt32("base");
    size = parsed.GetUInt32("size");
    index = parsed.GetUInt32("index");
    flags = parsed.GetUInt32("flags");
  }

  std::string action;
  std::string name;
  uint32_t base_address;
  uint32_t size;
  uint32_t index;
  uint32_t flags;
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_SECTION_H_
