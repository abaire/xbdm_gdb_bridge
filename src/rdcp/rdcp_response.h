#ifndef XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_RESPONSE_H_
#define XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_RESPONSE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rdcp/rdcp_status_code.h"

class RDCPResponse {
 public:
  //! Indicates that a binary response is expected and that the size of the
  //! binary will be provided as the first 4 bytes in the response.
  static constexpr long kBinaryInt32SizePrefix = -2;
  //! Indicates that a response cannot contain binary data.
  static constexpr long kBinaryNotAllowed = -1;

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
  [[nodiscard]] const std::string &Message() const { return response_message_; }
  [[nodiscard]] const std::vector<char> &Data() const { return data_; }

  static long Parse(std::shared_ptr<RDCPResponse> &response, const char *buffer,
                    size_t buffer_length,
                    long binary_response_size = kBinaryNotAllowed);

 private:
  friend std::ostream &operator<<(std::ostream &, RDCPResponse const &);

 private:
  StatusCode status_;
  std::string response_message_;
  std::vector<char> data_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_RESPONSE_H_
