#include "rdcp_response.h"

#include <algorithm>
#include <arpa/inet.h>
#include <boost/log/trivial.hpp>
#include <ostream>


static constexpr uint8_t kTerminator[] = {'\r', '\n'};
static constexpr long kTerminatorLen = sizeof(kTerminator) / sizeof(kTerminator[0]);
static constexpr uint8_t kMultilineTerminator[] = {'\r', '\n', '.', '\r', '\n'};
static constexpr long kMultilineTerminatorLen = sizeof(kMultilineTerminator) / sizeof(kMultilineTerminator[0]);


std::ostream &operator<<(std::ostream &os, RDCPResponse const &r) {
  return os << "RDCPResponse [" << r.status_ << "]";
}

long RDCPResponse::Parse(const char *buffer, size_t buffer_length, long binary_response_size) {
  if (buffer_length < 4) {
    return 0;
  }

  auto buffer_end = buffer + buffer_length;
  auto terminator = std::search(buffer, buffer_end, kTerminator, kTerminator + kTerminatorLen);
  if (terminator == buffer_end) {
    return 0;
  }

  long packet_size = (terminator - buffer) + kTerminatorLen;

  if (packet_size < 4) {
    BOOST_LOG_TRIVIAL(error) << "Invalid RDCP packet, length is " << packet_size << std::endl;
    return -packet_size;
  }

  if (buffer[3] != '-') {
    BOOST_LOG_TRIVIAL(error) << "Invalid RDCP packet, missing code_buffer delimiter. Received " << buffer[3] << std::endl;
    return -packet_size;
  }

  char code_buffer[4] = {0};
  memcpy(code_buffer, buffer, 3);
  status_ = static_cast<StatusCode>(strtol(code_buffer, nullptr, 10));
  response_message_.assign(buffer + 5, buffer + packet_size - kTerminatorLen);

  auto body_start = buffer + packet_size;
  const char *after_body_end = nullptr;

  switch (status_) {
    case OK_MULTILINE_RESPONSE:
      after_body_end = ParseMultilineResponse(body_start, buffer_end);
      break;

    case OK_BINARY_RESPONSE:
      after_body_end = ParseBinaryResponse(body_start, buffer_end, binary_response_size);
      break;

    default:
      data_.assign(body_start, terminator);
      after_body_end = terminator + kTerminatorLen;
      break;
  }

  if (!after_body_end) {
    return 0;
  }

  return after_body_end - buffer;
}

const char *RDCPResponse::ParseMultilineResponse(const char *body_start,
                                            const char *buffer_end) {
  auto terminator = std::search(body_start, buffer_end, kMultilineTerminator, kMultilineTerminator + kMultilineTerminatorLen);
  if (terminator == buffer_end) {
    return nullptr;
  }

  data_.assign(body_start, terminator);
  return terminator + kMultilineTerminatorLen;
}

const char *RDCPResponse::ParseBinaryResponse(const char *buffer,
                                              const char *buffer_end,
                                              long binary_response_size) {
  if (binary_response_size == kBinaryNotAllowed) {
    BOOST_LOG_TRIVIAL(error) << "Invalid RDCP packet, response contains binary data but no binary was expected." << std::endl;
    return nullptr;
  }

  auto bytes_available = buffer_end - buffer;
  if (binary_response_size == kBinaryInt32SizePrefix) {
    if (bytes_available < 4) {
      return nullptr;
    }

    binary_response_size = ntohl(*reinterpret_cast<int32_t const*>(buffer));
    bytes_available -= 4;
    buffer += 4;
  }

  if (bytes_available < binary_response_size) {
    return nullptr;
  }

  const char *body_end = buffer + binary_response_size;
  data_.assign(buffer, body_end);
  return body_end;
}
