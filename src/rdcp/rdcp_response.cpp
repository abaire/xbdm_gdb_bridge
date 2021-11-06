#include "rdcp_response.h"

#include <arpa/inet.h>

#include <algorithm>
#include <boost/log/trivial.hpp>
#include <ostream>

std::ostream &operator<<(std::ostream &os, RDCPResponse const &r) {
  return os << "RDCPResponse [" << r.status_ << "] " << r.response_message_
            << " size: " << r.data_.size();
}

static const char *ParseMultilineResponse(std::vector<char> &data,
                                          const char *body_start,
                                          const char *buffer_end) {
  auto terminator =
      std::search(body_start, buffer_end, RDCPResponse::kMultilineTerminator,
                  RDCPResponse::kMultilineTerminator +
                      RDCPResponse::kMultilineTerminatorLen);
  if (terminator == buffer_end) {
    return nullptr;
  }

  data.assign(body_start, terminator);
  return terminator + RDCPResponse::kMultilineTerminatorLen;
}

static const char *ParseBinaryResponse(std::vector<char> &data,
                                       const char *buffer,
                                       const char *buffer_end,
                                       long binary_response_size) {
  if (binary_response_size == RDCPResponse::kBinaryNotAllowed) {
    BOOST_LOG_TRIVIAL(error) << "Invalid RDCP packet, response contains binary "
                                "data but no binary was expected."
                             << std::endl;
    return nullptr;
  }

  auto bytes_available = buffer_end - buffer;
  if (binary_response_size == RDCPResponse::kBinaryInt32SizePrefix) {
    if (bytes_available < 4) {
      return nullptr;
    }

    binary_response_size = *reinterpret_cast<uint32_t const *>(buffer);
    bytes_available -= 4;
    buffer += 4;
  }

  if (bytes_available < binary_response_size) {
    return nullptr;
  }

  const char *body_end = buffer + binary_response_size;
  data.assign(buffer, body_end);
  return body_end;
}

long RDCPResponse::Parse(std::shared_ptr<RDCPResponse> &response,
                         const char *buffer, size_t buffer_length,
                         long binary_response_size) {
  response.reset();
  if (buffer_length < 4) {
    return 0;
  }

  auto buffer_end = buffer + buffer_length;
  auto terminator = std::search(buffer, buffer_end, kTerminator,
                                kTerminator + kTerminatorLen);
  if (terminator == buffer_end) {
    return 0;
  }

  long packet_size = (terminator - buffer) + kTerminatorLen;

  if (packet_size < 4) {
    BOOST_LOG_TRIVIAL(error)
        << "Invalid RDCP packet, length is " << packet_size << std::endl;
    return -packet_size;
  }

  if (buffer[3] != '-') {
    BOOST_LOG_TRIVIAL(error)
        << "Invalid RDCP packet, missing code_buffer delimiter. Received "
        << buffer[3] << std::endl;
    return -packet_size;
  }

  char code_buffer[4] = {0};
  memcpy(code_buffer, buffer, 3);
  auto status = static_cast<StatusCode>(strtol(code_buffer, nullptr, 10));

  std::string response_message;
  const char *after_body_end = nullptr;

  std::vector<char> data;

  switch (status) {
    case OK_MULTILINE_RESPONSE: {
      const char *end_of_message = buffer + packet_size - kTerminatorLen;
      response_message.assign(buffer + 5, end_of_message);
      // Empty multiline responses use the message terminator as part of the
      // multiline termination.
      if (!memcmp(end_of_message, kMultilineTerminator,
                  kMultilineTerminatorLen)) {
        after_body_end = end_of_message + kMultilineTerminatorLen;
      } else {
        after_body_end =
            ParseMultilineResponse(data, buffer + packet_size, buffer_end);
      }
    } break;

    case OK_BINARY_RESPONSE:
      after_body_end = ParseBinaryResponse(data, buffer + packet_size,
                                           buffer_end, binary_response_size);
      break;

    default:
      data.assign(buffer + 5, terminator);
      response_message.assign(buffer + 5, terminator);
      after_body_end = terminator + kTerminatorLen;
      break;
  }

  if (!after_body_end) {
    return 0;
  }

  response = std::make_shared<RDCPResponse>(status, std::move(response_message),
                                            std::move(data));
  return after_body_end - buffer;
}
