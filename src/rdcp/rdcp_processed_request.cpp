#include "rdcp_processed_request.h"

#include <algorithm>
#include <boost/regex.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string_regex.hpp>

static std::vector<std::vector<char>> SplitMultiline(const std::vector<char> &data) {
  std::vector<std::vector<char>> ret;

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

  std::string sanitized_data(data.begin(), data.end());
  boost::algorithm::replace_all(sanitized_data, RDCPResponse::kTerminator, " ");

  boost::escaped_list_separator<char> separator('\\', ' ', '\"');
  typedef boost::tokenizer<boost::escaped_list_separator<char>> SpaceTokenizer;
  SpaceTokenizer keyvals(sanitized_data, separator);
  for (auto it = keyvals.begin(); it != keyvals.end(); ++it) {
    // See if this is a key=value pair or just a key[=true].
    auto delimiter = it->find("=");
    if (delimiter == std::string::npos) {
      map[*it] = "";
      continue;
    }

    if (it->at(delimiter + 1) == '\"') {
      // Insert the unescaped contents of the string.
      std::string value = it->substr(delimiter + 2, it->size() - 1);
      boost::replace_all(value, "\\\"", "\"");
      map[it->substr(0, delimiter)] = value;
    } else {
      map[it->substr(0, delimiter)] = it->substr(delimiter + 1);
    }
  }
}

std::string RDCPMapResponse::GetString(const std::string &key,
                                       const std::string &default_value) {
  auto it = map.find(key);
  if (it == map.end()) {
    return default_value;
  }

  return it->second;
}

int32_t RDCPMapResponse::GetDWORD(const std::string &key, int base,
                                  int32_t default_value) {
  auto it = map.find(key);
  if (it == map.end()) {
    return default_value;
  }

  return static_cast<int32_t>(strtol(it->second.c_str(), nullptr, base));
}

int64_t RDCPMapResponse::GetQWORD(const std::string &key, int base,
                                  int64_t default_value) {
  auto it = map.find(key);
  if (it == map.end()) {
    return default_value;
  }

  return strtoll(it->second.c_str(), nullptr, base);
}

RDCPMultiMapResponse::RDCPMultiMapResponse(const std::vector<char> &data) {
  auto lines = SplitMultiline(data);
  for (auto &line : lines) {
    maps.emplace_back(RDCPMapResponse(line));
  }
}
