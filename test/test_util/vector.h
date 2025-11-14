#ifndef XBDM_GDB_BRIDGE_VECTOR_H
#define XBDM_GDB_BRIDGE_VECTOR_H

#include <map>
#include <string>
#include <vector>

std::vector<char>& operator+=(std::vector<char>& dest, const char* str);
std::vector<char>& operator+=(std::vector<char>& dest, const std::string& str);

std::vector<char> Serialize(const std::map<std::string, std::string>& map);

#endif  // XBDM_GDB_BRIDGE_VECTOR_H
