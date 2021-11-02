#ifndef XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_RESPONSE_H_
#define XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_RESPONSE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class RDCPResponse {
 public:
  enum StatusCode : uint32_t {
    INVALID = 0,
    OK = 200,
    OK_CONNECTED = 201,
    OK_MULTILINE_RESPONSE = 202,
    OK_BINARY_RESPONSE = 203,
    OK_SEND_BINARY_DATA = 204,
    OK_CONNECTION_DEDICATED = 205,
    ERR_UNEXPECTED = 400,
    ERR_MAX_CONNECTIONS_EXCEEDED = 401,
    ERR_FILE_NOT_FOUND = 402,
    ERR_NO_SUCH_MODULE = 403,
    ERR_MEMORY_NOT_MAPPED = 404,
    ERR_NO_SUCH_THREAD = 405,
    ERR_FAILED_TO_SET_SYSTEM_TIME = 406,
    ERR_UNKNOWN_COMMAND = 407,
    ERR_NOT_STOPPED = 408,
    ERR_FILE_MUST_BE_COPIED = 409,
    ERR_EXISTS = 410,
    ERR_DIRECTORY_NOT_EMPTY = 411,
    ERR_FILENAME_INVALID = 412,
    ERR_CREATE_FILE_FAILED = 413,
    ERR_ACCESS_DENIED = 414,
    ERR_NO_ROOM_ON_DEVICE = 415,
    ERR_NOT_DEBUGGABLE = 416,
    ERR_TYPE_INVALID = 417,
    ERR_DATA_NOT_AVAILABLE = 418,
    ERR_BOX_NOT_LOCKED = 420,
    ERR_KEY_EXCHANGE_REQUIRED = 421,
    ERR_DEDICATED_CONNECTION_REQUIRED = 422,
  };

  //! Indicates that a binary response is expected and that the size of the
  //! binary will be provided as the first 4 bytes in the response.
  static constexpr size_t kBinaryInt32SizePrefix = -1;
  //! Indicates that a response cannot contain binary data.
  static constexpr size_t kBinaryNotAllowed = 0;

 public:
  RDCPResponse(StatusCode status, std::string message, std::vector<char> data) : status_(status), response_message_(std::move(message)), data_(std::move(data)) {}

  [[nodiscard]] StatusCode status() const { return status_; }
  [[nodiscard]] const std::string &message() const { return response_message_; }
  [[nodiscard]] const std::vector<char> &data() const { return data_; }

  static long Parse(
      std::shared_ptr<RDCPResponse> &response,
      const char *buffer,
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
