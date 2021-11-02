#ifndef XBDM_GDB_BRIDGE_RDCP_PROCESSED_REQUEST_H
#define XBDM_GDB_BRIDGE_RDCP_PROCESSED_REQUEST_H

#include <map>
#include <string>
#include <vector>

#include "rdcp_response.h"
#include "rdcp_request.h"
#include "rdcp_status_code.h"

struct RDCPProcessedRequest : public RDCPRequest {
 public:
  explicit RDCPProcessedRequest(std::string command) : RDCPRequest(std::move(command)) {}
  RDCPProcessedRequest(std::string command, std::vector<uint8_t> data) : RDCPRequest(std::move(command), std::move(data)) {}

  [[nodiscard]] virtual bool IsOK() const {
    return status == StatusCode::OK;
  }

  void Complete(const std::shared_ptr<RDCPResponse>& response) override {
    status = response->Status();
    message = response->Message();
  }

 public:
  StatusCode status{INVALID};
  std::string message;
};

struct RDCPMultilineResponse {
  explicit RDCPMultilineResponse(const std::vector<char> &data);

  std::vector<std::vector<char>> lines;
};

struct RDCPMapResponse {
  explicit RDCPMapResponse(const std::vector<char> &data);

  [[nodiscard]] bool HasKey(const std::string &key) const;

  [[nodiscard]] std::string GetString(const std::string &key) const { return GetString(key, ""); }
  [[nodiscard]] std::string GetString(const std::string &key, const std::string &default_value) const;

  [[nodiscard]] int32_t GetDWORD(const std::string &key) const { return GetDWORD(key, 0); }
  [[nodiscard]] int32_t GetDWORD(const std::string &key, int32_t default_value) const;

  [[nodiscard]] int64_t GetQWORD(const std::string &key) const { return GetQWORD(key, 0); }
  [[nodiscard]] int64_t GetQWORD(const std::string &key, int64_t default_value) const;

  std::map<std::string, std::string> map;
};

struct RDCPMultiMapResponse {
  explicit RDCPMultiMapResponse(const std::vector<char> &data);

  std::vector<RDCPMapResponse> maps;
};

#endif  // XBDM_GDB_BRIDGE_RDCP_PROCESSED_REQUEST_H
