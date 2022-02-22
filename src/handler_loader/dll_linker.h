#ifndef XBDM_GDB_BRIDGE_DLL_LINKER_H
#define XBDM_GDB_BRIDGE_DLL_LINKER_H

#include <cstdint>
#include <memory>
#include <optional>

class XBDMDebugger;

std::optional<uint32_t> GetExportAddress(
    const std::shared_ptr<XBDMDebugger>& debugger, uint32_t ordinal,
    uint32_t image_base);

#endif  // XBDM_GDB_BRIDGE_DLL_LINKER_H
