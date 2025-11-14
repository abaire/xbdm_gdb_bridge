#include "parsing.h"

int32_t ParseInt32(const std::vector<uint8_t>& data) {
  std::string value(reinterpret_cast<const char*>(data.data()), data.size());
  return ParseInt32(value);
}

int32_t ParseInt32(const std::vector<char>& data) {
  std::string value(data.data(), data.size());
  return ParseInt32(value);
}

int32_t ParseInt32(const std::string& value) {
  int base = 10;
  if (value.size() > 2 && (value[1] == 'x' || value[1] == 'X')) {
    base = 16;
  }

  return static_cast<int32_t>(strtol(value.c_str(), nullptr, base));
}

uint32_t ParseUint32(const std::vector<uint8_t>& data) {
  std::string value(reinterpret_cast<const char*>(data.data()), data.size());
  return ParseUint32(value);
}

uint32_t ParseUint32(const std::vector<char>& data) {
  std::string value(data.data(), data.size());
  return ParseUint32(value);
}

uint32_t ParseUint32(const std::string& value) {
  int base = 10;
  if (value.size() > 2 && (value[1] == 'x' || value[1] == 'X')) {
    base = 16;
  }

  return static_cast<uint32_t>(strtoul(value.c_str(), nullptr, base));
}
