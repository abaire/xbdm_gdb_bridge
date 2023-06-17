#include "gdb_packet.h"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <cstdint>

#include "util/logging.h"

static constexpr char kPacketLeader = '$';
static constexpr char kPacketTrailer = '#';
static constexpr char kPacketEscapeChar = '}';
static constexpr char kEscapeCharacterSet[] = {kPacketEscapeChar, kPacketLeader,
                                               kPacketTrailer, 0};

struct GDBPacketEscaper {
  static std::vector<uint8_t> EscapeBuffer(const std::vector<uint8_t> &buffer) {
    auto escaped_body = boost::algorithm::find_format_all_copy(
        buffer, boost::token_finder(boost::is_any_of(kEscapeCharacterSet)),
        GDBPacketEscaper());
    return escaped_body;
  }

  template <typename FindResultT>
  std::vector<char> operator()(const FindResultT &match) const {
    std::vector<char> ret;
    ret.reserve(match.size() * 2);
    for (auto it = match.begin(); it != match.end(); ++it) {
      ret.push_back(kPacketEscapeChar);
      ret.push_back(*it ^ 0x20);
    }
    return ret;
  }
};

static uint8_t Mod256Checksum(const uint8_t *buffer, long buffer_len) {
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

std::vector<uint8_t>::const_iterator GDBPacket::FindFirst(char item) const {
  return std::find(data_.cbegin(), data_.cend(), static_cast<uint8_t>(item));
}

long GDBPacket::Parse(const uint8_t *buffer, size_t buffer_length) {
  const auto *packet_start = static_cast<const uint8_t *>(
      memchr(buffer, kPacketLeader, buffer_length));
  if (!packet_start) {
    return 0;
  }

  const uint8_t *body_start = packet_start + 1;
  size_t max_size = buffer_length - (body_start - buffer);
  const auto *terminator = static_cast<const uint8_t *>(
      memchr(body_start, kPacketTrailer, max_size));
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
    LOG_GDB(error) << "Non-numeric checksum " << checksum_str << std::endl;
    // This should never happen, but consume the packet through the terminator
    // and leave the non-numeric chars.
    return terminator - buffer + 1;
  }

  data_.assign(body_start, terminator);
  CalculateChecksum();

  checksum_ok_ = checksum == checksum_;
  if (!checksum_ok_) {
    LOG_GDB(error) << "Checksum mismatch " << checksum_ << " != sent checksum "
                   << checksum << std::endl;
  }

  return (terminator - buffer) + 3;
}

std::vector<uint8_t> GDBPacket::Serialize() const {
  std::vector<uint8_t> escaped_body = GDBPacketEscaper::EscapeBuffer(data_);

  std::vector<uint8_t> ret;
  ret.reserve(escaped_body.size() + 4);
  ret.push_back(kPacketLeader);
  ret.insert(ret.end(), escaped_body.begin(), escaped_body.end());
  ret.push_back(kPacketTrailer);

  char checksum_buf[3] = {0};
  snprintf(checksum_buf, 3, "%02x", checksum_);
  ret.insert(ret.end(), checksum_buf, checksum_buf + 2);

  return ret;
}

long GDBPacket::UnescapeBuffer(const std::vector<uint8_t> &buffer,
                               std::vector<uint8_t> &out_buffer) {
  if (buffer.empty()) {
    return 0;
  }

  long ret = 0;
  auto start = buffer.begin();
  auto it = std::find(start, buffer.end(), kPacketEscapeChar);

  for (; it != buffer.end();
       it = std::find(start, buffer.end(), kPacketEscapeChar)) {
    ret += it - start;
    out_buffer.insert(out_buffer.end(), start, it);

    ++it;
    if (it == buffer.end()) {
      return ret;
    }

    out_buffer.push_back(*it ^ 0x20);
    start = it + 1;
  }

  ret += buffer.end() - start;
  out_buffer.insert(out_buffer.end(), start, buffer.end());

  return ret;
}
