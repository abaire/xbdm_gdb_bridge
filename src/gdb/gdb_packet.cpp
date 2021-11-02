#include "gdb_packet.h"

#include <boost/algorithm/string.hpp>
#include <boost/log/trivial.hpp>
#include <cstdint>

static constexpr char kPacketLeader = '$';
static constexpr char kPacketTrailer = '#';
static constexpr char kPacketEscapeChar = '}';
static constexpr char kEscapeCharacterSet[] = {kPacketEscapeChar, kPacketLeader,
                                               kPacketTrailer, 0};

struct GDBPacketEscaper {
  static std::vector<char> EscapeBuffer(const std::vector<char> &buffer) {
    auto escaped_body = boost::algorithm::find_format_all_copy(
        buffer, boost::token_finder(boost::is_any_of(kEscapeCharacterSet)),
        GDBPacketEscaper());
    return escaped_body;
  }

  template <typename FindResultT>
  std::vector<char> operator()(const FindResultT &match) const {
    std::vector<char> ret(match.size() * 2);
    for (auto it = match.begin(); it != match.end(); ++it) {
      ret.push_back(kPacketEscapeChar);
      ret.push_back(*it ^ 0x20);
    }
    return ret;
  }
};

static uint8_t Mod256Checksum(const char *buffer, long buffer_len) {
  int ret = 0;

  if (!buffer || buffer_len <= 0) {
    return ret;
  }

  for (auto i = 0; i < buffer_len; ++i, ++buffer) {
    ret = (ret + *buffer) & 0xFF;
  }

  return ret;
}

void GDBPacket::CalculateChecksum() {
  checksum_ = Mod256Checksum(data_.data(), static_cast<long>(data_.size()));
}

long GDBPacket::Parse(const char *buffer, size_t buffer_length) {
  const char *packet_start =
      static_cast<const char *>(memchr(buffer, kPacketLeader, buffer_length));
  if (!packet_start) {
    return 0;
  }

  const char *body_start = packet_start + 1;
  size_t max_size = buffer_length - (body_start - buffer);
  const char *terminator =
      static_cast<const char *>(memchr(body_start, kPacketTrailer, max_size));
  if (!terminator) {
    return 0;
  }

  size_t body_size = terminator - body_start;

  // Ensure the checksum bytes are in the buffer.
  max_size -= body_size + 1;
  if (max_size < 2) {
    return 0;
  }

  char checksum_str[3] = {0};
  memcpy(checksum_str, terminator + 1, 2);
  char *checksum_end;
  long checksum = strtol(checksum_str, &checksum_end, 16);
  if (checksum_end != checksum_str + 2) {
    BOOST_LOG_TRIVIAL(error)
        << "Non-numeric checksum " << checksum_str << std::endl;
    // This should never happen, but consume the packet through the terminator
    // and leave the non-numeric chars.
    return terminator - buffer + 1;
  }

  data_.assign(body_start, terminator);
  CalculateChecksum();

  if (checksum != checksum_) {
    BOOST_LOG_TRIVIAL(error) << "Checksum mismatch " << checksum_
                             << " != sent checksum " << checksum << std::endl;
  }

  return (terminator - buffer) + 3;
}

std::vector<uint8_t> GDBPacket::Serialize() const {
  std::vector<char> escaped_body = GDBPacketEscaper::EscapeBuffer(data_);

  std::vector<uint8_t> ret(escaped_body.size() + 4);
  ret.push_back(kPacketLeader);
  ret.insert(ret.end(), escaped_body.begin(), escaped_body.end());
  ret.push_back(kPacketTrailer);

  char checksum_buf[3] = {0};
  sprintf(checksum_buf, "%02x", checksum_);
  ret.insert(ret.end(), checksum_buf, checksum_buf + 2);

  return ret;
}
