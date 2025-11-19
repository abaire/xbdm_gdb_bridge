#include "parsing.h"

static constexpr char kCommandDelimiter[] = "&&";

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

namespace command_line_command_tokenizer {
std::vector<std::vector<std::string>> SplitCommands(
    const std::vector<std::string>& additional_commands) {
  std::vector<std::vector<std::string>> ret;

  if (additional_commands.empty()) {
    return ret;
  }

  std::vector<std::string> cmd;
  for (auto& elem : additional_commands) {
    if (elem == kCommandDelimiter) {
      ret.push_back(cmd);
      cmd.clear();
      continue;
    }

    cmd.push_back(elem);
  }
  ret.push_back(cmd);

  return std::move(ret);
}
}  // namespace command_line_command_tokenizer
