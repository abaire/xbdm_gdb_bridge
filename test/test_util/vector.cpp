#include "vector.h"

#include <cassert>
#include <cstring>

std::vector<char> &operator+=(std::vector<char> &dest, const char *str) {
  assert(str);
  dest.insert(dest.end(), str, str + strlen(str));
  return dest;
}

std::vector<char> &operator+=(std::vector<char> &dest, const std::string &str) {
  dest.insert(dest.end(), str.begin(), str.end());
  return dest;
}

std::vector<char> Serialize(const std::map<std::string, std::string> &map) {
  std::vector<char> buffer;
  for (const auto &it : map) {
    const std::string &value = it.second;
    if (value.empty()) {
      if (!buffer.empty()) {
        buffer += " ";
      }
      buffer += it.first;
      continue;
    }

    buffer += it.first;
    buffer += "=";
    if (value.find(' ') != std::string::npos) {
      buffer += "\"";
      buffer += value;
      buffer += "\"";
      continue;
    }

    buffer += value;
  }

  return buffer;
}
