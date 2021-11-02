#ifndef XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_REQUEST_H_
#define XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_REQUEST_H_

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

class RDCPResponse;

class RDCPRequest {
 public:
  explicit RDCPRequest(std::string command) : command_(std::move(command)) {}
  RDCPRequest(std::string command, std::vector<uint8_t> data) : command_(std::move(command)), data_(std::move(data)) {}

  explicit operator std::vector<uint8_t>() const;

  virtual void Complete(const std::shared_ptr<RDCPResponse> &response) = 0;

  void SetData(const std::vector<uint8_t> &data) {
    data_ = data;
  }

  void SetData(const std::string &data) {
    data_.clear();
    data_.assign(data.begin(), data.end());
  }

  void SetData(const char *data) {
    data_.clear();
    if (data) {
      data_.assign(data, data + strlen(data));
    }
  }

 protected:
  std::string command_;
  std::vector<uint8_t> data_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_REQUEST_H_
