#include "rdcp_response_processors.h"

#include <algorithm>
#include <boost/algorithm/string_regex.hpp>
#include <boost/regex.hpp>

#include "util/logging.h"
#include "util/parsing.h"

static std::vector<std::vector<char>> SplitMultiline(
    const std::vector<char>& data) {
  std::vector<std::vector<char>> ret;
  if (data.empty()) {
    return ret;
  }

  char delimiter[RDCPResponse::kTerminatorLen + 1] = {0};
  memcpy(delimiter, RDCPResponse::kTerminator, RDCPResponse::kTerminatorLen);

  auto delimiter_regex = boost::regex(delimiter);
  boost::algorithm::split_regex(ret, data, delimiter_regex);

  return ret;
}

RDCPMultilineResponse::RDCPMultilineResponse(const std::vector<char>& data) {
  lines = SplitMultiline(data);
}

RDCPMapResponse::RDCPMapResponse(const std::vector<char>& data)
    : RDCPMapResponse(data.begin(), data.end()) {}

RDCPMapResponse::RDCPMapResponse(const std::string& data)
    : RDCPMapResponse(data.begin(), data.end()) {}

bool RDCPMapResponse::HasKey(const std::string& key) const {
  auto insensitive_key = boost::algorithm::to_lower_copy(key);
  auto it = map.find(insensitive_key);
  return map.find(insensitive_key) != map.end();
}

std::string RDCPMapResponse::GetString(const std::string& key,
                                       const std::string& default_value) const {
  auto insensitive_key = boost::algorithm::to_lower_copy(key);
  auto it = map.find(insensitive_key);
  if (it == map.end()) {
    return default_value;
  }

  return it->second;
}

std::optional<int32_t> RDCPMapResponse::GetOptionalDWORD(
    const std::string& key) const {
  auto insensitive_key = boost::algorithm::to_lower_copy(key);
  auto it = map.find(insensitive_key);
  if (it == map.end()) {
    return {};
  }
  return ParseUint32(it->second);
}

int32_t RDCPMapResponse::GetDWORD(const std::string& key,
                                  int32_t default_value) const {
  auto insensitive_key = boost::algorithm::to_lower_copy(key);
  auto it = map.find(insensitive_key);
  if (it == map.end()) {
    return default_value;
  }

  return static_cast<int32_t>(ParseUint32(it->second));
}

uint32_t RDCPMapResponse::GetUInt32(const std::string& key,
                                    uint32_t default_value) const {
  auto insensitive_key = boost::algorithm::to_lower_copy(key);
  auto it = map.find(insensitive_key);
  if (it == map.end()) {
    return default_value;
  }

  return ParseUint32(it->second);
}

int64_t RDCPMapResponse::GetQWORD(const std::string& low_key,
                                  const std::string& high_key,
                                  int64_t default_value) const {
  auto insensitive_key = boost::algorithm::to_lower_copy(low_key);
  auto it = map.find(insensitive_key);
  if (it == map.end()) {
    return default_value;
  }

  auto low = ParseUint32(it->second);

  insensitive_key = boost::algorithm::to_lower_copy(high_key);
  it = map.find(insensitive_key);
  if (it == map.end()) {
    LOG_XBDM(warning) << "Found QWORD low key " << low_key
                      << " but missing high key " << high_key;
    return default_value;
  }

  auto high = static_cast<int64_t>(ParseInt32(it->second));
  return (high << 32) + low;
}

RDCPMultiMapResponse::RDCPMultiMapResponse(const std::vector<char>& data) {
  auto lines = SplitMultiline(data);
  for (auto& line : lines) {
    maps.emplace_back(line);
  }
}

std::ostream& operator<<(std::ostream& os, const RDCPMapResponse& r) {
  for (const auto& it : r.map) {
    os << it.first << " = " << it.second << " ; ";
  }
  return os;
}
