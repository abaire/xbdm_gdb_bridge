// Adapted from xboxkrnl.exe.def
//    https://github.com/XboxDev/nxdk/blob/master/lib/xboxkrnl/xboxkrnl.exe.def
//
// Copyright (C) 2017 Stefan Schmidt
// This specific file is licensed under the CC0 1.0.
// Look here for details: https://creativecommons.org/publicdomain/zero/1.0/

#ifndef XBDM_GDB_BRIDGE_XBOXKRNL_EXPORTS_H
#define XBDM_GDB_BRIDGE_XBOXKRNL_EXPORTS_H

#include <cstdint>
#include <map>
#include <string>

#include "xboxkrnl_exports.def.h"

extern std::map<std::string, uint32_t> XBOXKRNL_Exports;

#endif  // XBDM_GDB_BRIDGE_XBOXKRNL_EXPORTS_H
