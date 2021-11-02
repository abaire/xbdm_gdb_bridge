#ifndef XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_REQUEST_H_
#define XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_REQUEST_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class RDCPResponse;

class RDCPRequest {
 public:
  explicit RDCPRequest(std::string command) : command_(std::move(command)) {}
  RDCPRequest(std::string command, std::vector<uint8_t> data) : command_(std::move(command)), data_(std::move(data)) {}

  explicit operator std::vector<uint8_t>() const;

  void Complete(const std::shared_ptr<RDCPResponse> &response);

 private:
  std::string command_;
  std::vector<uint8_t> data_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_REQUEST_H_
