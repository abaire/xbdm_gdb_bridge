#ifndef XBDM_GDB_BRIDGE_SRC_GDB_GDB_PACKET_H_
#define XBDM_GDB_BRIDGE_SRC_GDB_GDB_PACKET_H_

#include <cstdint>
#include <utility>
#include <vector>

class GDBPacket {
 public:
  GDBPacket() = default;
  explicit GDBPacket(std::vector<uint8_t> data) : data_(std::move(data)) {
    CalculateChecksum();
  }
  GDBPacket(const uint8_t *data, size_t data_len) {
    data_.assign(data, data + data_len);
    CalculateChecksum();
  }

  [[nodiscard]] const std::vector<uint8_t> &Data() const { return data_; }
  [[nodiscard]] uint8_t Checksum() const { return checksum_; }
  [[nodiscard]] bool ChecksumOK() const { return checksum_ok_; }

  long Parse(const uint8_t *buffer, size_t buffer_length);
  [[nodiscard]] std::vector<uint8_t> Serialize() const;

  static long UnescapeBuffer(const std::vector<uint8_t> &buffer,
                             std::vector<uint8_t> &out_buffer);

 protected:
  void CalculateChecksum();

 private:
  std::vector<uint8_t> data_;
  uint8_t checksum_{0};
  bool checksum_ok_{false};
};

#endif  // XBDM_GDB_BRIDGE_SRC_GDB_GDB_PACKET_H_
