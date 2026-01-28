#ifndef XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_REQUEST_H_
#define XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_REQUEST_H_

#include <boost/algorithm/hex.hpp>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "rdcp_response.h"

class RDCPRequest {
 public:
  virtual ~RDCPRequest() = default;
  explicit RDCPRequest(std::string command) : command_(std::move(command)) {}
  RDCPRequest(std::string command, std::vector<uint8_t> data)
      : command_(std::move(command)), data_(std::move(data)) {}

  explicit operator std::vector<uint8_t>() const;

  virtual void Complete(const std::shared_ptr<RDCPResponse>& response) = 0;
  virtual void Abandon() = 0;

  [[nodiscard]] RDCPResponse::ReadBinarySizeFunc BinaryResponseSizeParser()
      const {
    return binary_response_size_parser_;
  }

  [[nodiscard]] virtual const std::vector<uint8_t>* BinaryPayload() {
    return nullptr;
  }

  void SetData(const std::vector<uint8_t>& data) { data_ = data; }

  template <typename T>
  void SetData(const T& data) {
    data_.clear();
    data_.assign(data.begin(), data.end());
  }

  void SetData(const char* data) {
    data_.clear();
    if (data) {
      data_.assign(data, data + strlen(data));
    }
  }

  template <typename T>
  void AppendData(const T& data) {
    data_.insert(data_.end(), data.begin(), data.end());
  }

  void AppendData(const char* data) {
    if (data) {
      data_.insert(data_.end(), data, data + strlen(data));
    }
  }

  void AppendDecimalString(int value) {
    char buf[32] = {0};
    snprintf(buf, 31, "%d", value);
    data_.insert(data_.end(), buf, buf + strlen(buf));
  }

  void AppendDecimalString(unsigned int value) {
    char buf[32] = {0};
    snprintf(buf, 31, "%u", value);
    data_.insert(data_.end(), buf, buf + strlen(buf));
  }

  template <
      typename T,
      std::enable_if_t<std::is_integral<T>::value && sizeof(T) <= 8, int> = 0>
  void AppendHexString(T value) {
    char buf[32] = {0};
    if (sizeof(T) > 4) {
      snprintf(buf, 31, "0q%016llx", (unsigned long long)value);
    } else {
      snprintf(buf, 31, "0x%08x", (unsigned int)value);
    }
    data_.insert(data_.end(), buf, buf + strlen(buf));
  }

  template <typename T>
  void AppendHexBuffer(const std::vector<T>& buffer) {
    boost::algorithm::hex(buffer.begin(), buffer.end(), back_inserter(data_));
  }

 protected:
  friend std::ostream& operator<<(std::ostream&, RDCPRequest const&);

 protected:
  std::string command_;
  std::vector<uint8_t> data_;
  RDCPResponse::ReadBinarySizeFunc binary_response_size_parser_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_REQUEST_H_
