#ifndef XBDM_GDB_BRIDGE_PARSING_H
#define XBDM_GDB_BRIDGE_PARSING_H

#include <cinttypes>
#include <string>
#include <vector>

int32_t ParseInt32(const std::vector<uint8_t> &value);
int32_t ParseInt32(const std::vector<char> &data);
int32_t ParseInt32(const std::string &value);

#endif  // XBDM_GDB_BRIDGE_PARSING_H
