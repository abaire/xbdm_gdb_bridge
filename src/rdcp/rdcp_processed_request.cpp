#include "rdcp_processed_request.h"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string_regex.hpp>
#include <boost/log/trivial.hpp>
#include <boost/regex.hpp>
#include <boost/tokenizer.hpp>

static std::vector<std::vector<char>> SplitMultiline(
    const std::vector<char> &data) {
  std::vector<std::vector<char>> ret;
  if (data.empty()) {
    return ret;
  }

  char delimiter[RDCPResponse::kTerminatorLen] = {0};
  memcpy(delimiter, RDCPResponse::kTerminator, RDCPResponse::kTerminatorLen);

  auto delimiter_regex = boost::regex(delimiter);
  boost::algorithm::split_regex(ret, data, delimiter_regex);

  return ret;
}

RDCPMultilineResponse::RDCPMultilineResponse(const std::vector<char> &data) {
  lines = SplitMultiline(data);
}

RDCPMapResponse::RDCPMapResponse(const std::vector<char> &data) {
  if (data.empty()) {
    return;
  }

  std::string sanitized_data(data.data(), data.size());
  boost::algorithm::replace_all(sanitized_data, RDCPResponse::kTerminator, " ");

  boost::escaped_list_separator<char> separator('\\', ' ', '\"');
  typedef boost::tokenizer<boost::escaped_list_separator<char>> SpaceTokenizer;
  SpaceTokenizer keyvals(sanitized_data, separator);
  for (auto it = keyvals.begin(); it != keyvals.end(); ++it) {
    const std::string &token = *it;
    // See if this is a key=value pair or just a key[=true].
    auto delimiter = token.find('=');
    if (delimiter == std::string::npos) {
      map[token] = "";
      valueless_keys.insert(token);
      continue;
    }

    if (token[delimiter + 1] == '\"') {
      // Insert the unescaped contents of the string.
      std::string value = token.substr(delimiter + 2, token.size() - 1);
      boost::replace_all(value, "\\\"", "\"");
      map[token.substr(0, delimiter)] = value;
    } else {
      map[token.substr(0, delimiter)] = token.substr(delimiter + 1);
    }
  }
}

bool RDCPMapResponse::HasKey(const std::string &key) const {
  return map.find(key) != map.end();
}

std::string RDCPMapResponse::GetString(const std::string &key,
                                       const std::string &default_value) const {
  auto it = map.find(key);
  if (it == map.end()) {
    return default_value;
  }

  return it->second;
}

int32_t RDCPMapResponse::ParseInteger(const std::vector<uint8_t> &data) {
  std::string value(reinterpret_cast<const char *>(data.data()), data.size());
  return ParseInteger(value);
}

int32_t RDCPMapResponse::ParseInteger(const std::vector<char> &data) {
  std::string value(data.data(), data.size());
  return ParseInteger(value);
}

int32_t RDCPMapResponse::ParseInteger(const std::string &value) {
  int base = 10;
  if (value.size() > 2 && (value[1] == 'x' || value[1] == 'X')) {
    base = 16;
  }

  return static_cast<int32_t>(strtol(value.c_str(), nullptr, base));
}

boost::optional<int32_t> RDCPMapResponse::GetOptionalDWORD(
    const std::string &key) const {
  auto it = map.find(key);
  if (it == map.end()) {
    return boost::none;
  }
  return ParseInteger(it->second);
}

int32_t RDCPMapResponse::GetDWORD(const std::string &key,
                                  int32_t default_value) const {
  auto it = map.find(key);
  if (it == map.end()) {
    return default_value;
  }

  return ParseInteger(it->second);
}

uint32_t RDCPMapResponse::GetUInt32(const std::string &key, uint32_t default_value) const {
  auto it = map.find(key);
  if (it == map.end()) {
    return default_value;
  }

  return ParseInteger(it->second);
}

int64_t RDCPMapResponse::GetQWORD(const std::string &low_key,
                                  const std::string &high_key,
                                  int64_t default_value) const {
  auto it = map.find(low_key);
  if (it == map.end()) {
    return default_value;
  }

  int64_t low = ParseInteger(it->second);

  it = map.find(high_key);
  if (it == map.end()) {
    BOOST_LOG_TRIVIAL(warning)
        << "Found QWORD low key " << low_key << " but missing high key "
        << high_key << std::endl;
    return default_value;
  }

  int64_t high = ParseInteger(it->second);

  return low + (high << 32);
}

RDCPMultiMapResponse::RDCPMultiMapResponse(const std::vector<char> &data) {
  auto lines = SplitMultiline(data);
  for (auto &line : lines) {
    maps.emplace_back(RDCPMapResponse(line));
  }
}
