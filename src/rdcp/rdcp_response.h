#ifndef XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_RESPONSE_H_
#define XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_RESPONSE_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "rdcp/rdcp_status_code.h"

class RDCPResponse {
 public:
  // Function used to determine the full size of a binary response.
  // Returns: false if more binary data is necessary to determine the size
  // uint8_t const *binary_buffer [IN]
  // uint32_t buffer_size [IN]
  // long &binary_size [OUT]
  // uint32_t bytes_consumed [OUT]
  typedef std::function<bool(uint8_t const*, uint32_t, long&, uint32_t&)>
      ReadBinarySizeFunc;

 public:
  static constexpr uint8_t kTerminator[] = {'\r', '\n'};
  static constexpr long kTerminatorLen =
      sizeof(kTerminator) / sizeof(kTerminator[0]);
  static constexpr uint8_t kMultilineTerminator[] = {'\r', '\n', '.', '\r',
                                                     '\n'};
  static constexpr long kMultilineTerminatorLen =
      sizeof(kMultilineTerminator) / sizeof(kMultilineTerminator[0]);

 public:
  RDCPResponse(StatusCode status, std::string message)
      : status_(status), response_message_(std::move(message)) {}
  RDCPResponse(StatusCode status, std::string message, std::vector<char> data)
      : status_(status),
        response_message_(std::move(message)),
        data_(std::move(data)) {}

  [[nodiscard]] StatusCode Status() const { return status_; }
  [[nodiscard]] const std::string& Message() const { return response_message_; }
  [[nodiscard]] const std::vector<char>& Data() const { return data_; }

  static long Parse(std::shared_ptr<RDCPResponse>& response, const char* buffer,
                    size_t buffer_length,
                    const ReadBinarySizeFunc& size_parser);

 private:
  friend std::ostream& operator<<(std::ostream&, RDCPResponse const&);

 private:
  StatusCode status_;
  std::string response_message_;
  std::vector<char> data_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_RESPONSE_H_
