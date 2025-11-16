#ifndef XBDM_GDB_BRIDGE_RDCP_RESPONSE_PROCESSORS_H
#define XBDM_GDB_BRIDGE_RDCP_RESPONSE_PROCESSORS_H

#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "rdcp_response.h"

struct RDCPMultilineResponse {
  explicit RDCPMultilineResponse(const std::vector<char>& data);

  std::vector<std::vector<char>> lines;
};

struct RDCPMapResponse {
  explicit RDCPMapResponse(const std::vector<char>& data);
  explicit RDCPMapResponse(const std::string& data);

  template <typename ConstIterator>
  RDCPMapResponse(ConstIterator start, ConstIterator end) {
    if (start == end) {
      return;
    }

    std::string sanitized_data(start, end);
    boost::algorithm::replace_all(sanitized_data, RDCPResponse::kTerminator,
                                  " ");

    boost::escaped_list_separator<char> separator(0, ' ', '\"');
    typedef boost::tokenizer<boost::escaped_list_separator<char>>
        SpaceTokenizer;
    SpaceTokenizer keyvals(sanitized_data, separator);
    for (auto it = keyvals.begin(); it != keyvals.end(); ++it) {
      const std::string& token = *it;
      // See if this is a key=value pair or just a key[=true].
      auto delimiter = token.find('=');
      if (delimiter == std::string::npos) {
        auto key = boost::algorithm::to_lower_copy(token);
        map[key] = "";
        valueless_keys.insert(key);
        continue;
      }

      if (token[delimiter + 1] == '\"') {
        // Insert the unescaped contents of the string.
        std::string value = token.substr(delimiter + 2, token.size() - 1);
        boost::replace_all(value, "\\\"", "\"");
        auto key = boost::algorithm::to_lower_copy(token.substr(0, delimiter));
        map[key] = value;
      } else {
        auto key = boost::algorithm::to_lower_copy(token.substr(0, delimiter));
        map[key] = token.substr(delimiter + 1);
      }
    }
  }

  [[nodiscard]] bool HasKey(const std::string& key) const;

  template <typename T, typename... Ts>
  using are_same_type = std::conjunction<std::is_same<T, Ts>...>;

  template <typename T>
  [[nodiscard]] bool HasAnyOf(const T& key) const {
    return map.find(key) != map.end();
  }

  template <typename T, typename... Ts, typename = are_same_type<T, Ts...>>
  [[nodiscard]] bool HasAnyOf(const T& key, const Ts&... rest) const {
    if (map.find(key) != map.end()) {
      return true;
    }

    return HasAnyOf(rest...);
  }

  [[nodiscard]] std::string GetString(const std::string& key) const {
    return GetString(key, "");
  }
  [[nodiscard]] std::string GetString(const std::string& key,
                                      const std::string& default_value) const;

  [[nodiscard]] std::optional<int32_t> GetOptionalDWORD(
      const std::string& key) const;

  [[nodiscard]] int32_t GetDWORD(const std::string& key) const {
    return GetDWORD(key, 0);
  }
  [[nodiscard]] int32_t GetDWORD(const std::string& key,
                                 int32_t default_value) const;

  [[nodiscard]] uint32_t GetUInt32(const std::string& key) const {
    return GetUInt32(key, 0);
  }
  [[nodiscard]] uint32_t GetUInt32(const std::string& key,
                                   uint32_t default_value) const;

  [[nodiscard]] int64_t GetQWORD(const std::string& low_key,
                                 const std::string& high_key) const {
    return GetQWORD(low_key, high_key, 0);
  }
  [[nodiscard]] int64_t GetQWORD(const std::string& low_key,
                                 const std::string& high_key,
                                 int64_t default_value) const;

  std::map<std::string, std::string> map;
  std::set<std::string> valueless_keys;
};

std::ostream& operator<<(std::ostream& os, const RDCPMapResponse& r);

struct RDCPMultiMapResponse {
  explicit RDCPMultiMapResponse(const std::vector<char>& data);

  std::vector<RDCPMapResponse> maps;
};

#endif  // XBDM_GDB_BRIDGE_RDCP_RESPONSE_PROCESSORS_H
