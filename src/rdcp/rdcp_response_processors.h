#ifndef XBDM_GDB_BRIDGE_RDCP_RESPONSE_PROCESSORS_H
#define XBDM_GDB_BRIDGE_RDCP_RESPONSE_PROCESSORS_H

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

struct RDCPMultilineResponse {
  explicit RDCPMultilineResponse(const std::vector<char> &data);

  std::vector<std::vector<char>> lines;
};

struct RDCPMapResponse {
  explicit RDCPMapResponse(const std::vector<char> &data);
  RDCPMapResponse(std::vector<char>::const_iterator start,
                  std::vector<char>::const_iterator end);

  [[nodiscard]] bool HasKey(const std::string &key) const;

  [[nodiscard]] std::string GetString(const std::string &key) const {
    return GetString(key, "");
  }
  [[nodiscard]] std::string GetString(const std::string &key,
                                      const std::string &default_value) const;

  [[nodiscard]] std::optional<int32_t> GetOptionalDWORD(
      const std::string &key) const;

  [[nodiscard]] int32_t GetDWORD(const std::string &key) const {
    return GetDWORD(key, 0);
  }
  [[nodiscard]] int32_t GetDWORD(const std::string &key,
                                 int32_t default_value) const;

  [[nodiscard]] uint32_t GetUInt32(const std::string &key) const {
    return GetUInt32(key, 0);
  }
  [[nodiscard]] uint32_t GetUInt32(const std::string &key,
                                   uint32_t default_value) const;

  [[nodiscard]] int64_t GetQWORD(const std::string &low_key,
                                 const std::string &high_key) const {
    return GetQWORD(low_key, high_key, 0);
  }
  [[nodiscard]] int64_t GetQWORD(const std::string &low_key,
                                 const std::string &high_key,
                                 int64_t default_value) const;

  std::map<std::string, std::string> map;
  std::set<std::string> valueless_keys;
};

struct RDCPMultiMapResponse {
  explicit RDCPMultiMapResponse(const std::vector<char> &data);

  std::vector<RDCPMapResponse> maps;
};

#endif  // XBDM_GDB_BRIDGE_RDCP_RESPONSE_PROCESSORS_H
