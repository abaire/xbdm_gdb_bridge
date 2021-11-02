#ifndef XBDM_GDB_BRIDGE_SRC_GDB_GDB_PACKET_H_
#define XBDM_GDB_BRIDGE_SRC_GDB_GDB_PACKET_H_

#include <cstdint>
#include <utility>
#include <vector>

class GDBPacket {
 public:
  GDBPacket() = default;
  explicit GDBPacket(std::vector<char> data) : data_(std::move(data)) {
    CalculateChecksum();
  }

  [[nodiscard]] const std::vector<char> &Data() const { return data_; }
  [[nodiscard]] uint8_t Checksum() const { return checksum_; }

  long Parse(const char *buffer, size_t buffer_length);
  [[nodiscard]] std::vector<uint8_t> Serialize() const;

 protected:
  void CalculateChecksum();

 private:
  std::vector<char> data_;
  uint8_t checksum_{0};
};

#endif  // XBDM_GDB_BRIDGE_SRC_GDB_GDB_PACKET_H_
