#ifndef XBDM_GDB_BRIDGE_GDB_TRANSPORT_H
#define XBDM_GDB_BRIDGE_GDB_TRANSPORT_H

#include <any>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <string>

#include "gdb/gdb_packet.h"
#include "net/tcp_connection.h"

class GDBTransport : public TCPConnection {
 public:
  enum BreakpointType {
    SOFTWARE = 0,
    HARDWARE,
    WRITE,
    READ,
    ACCESS,
  };

  typedef std::function<void(GDBPacket &)> PacketReceivedHandler;

 public:
  GDBTransport(std::string name, int sock, IPAddress address,
               PacketReceivedHandler handler);

  void Send(const GDBPacket &packet);

  void SetNoAckMode(bool value) { no_ack_mode_ = value; }
  [[nodiscard]] bool NoAckMode() const { return no_ack_mode_; }

 private:
  void WriteNextPacket();
  void OnBytesRead() override;
  void ProcessUnescapedReadBuffer();

 private:
  bool no_ack_mode_{false};

  std::recursive_mutex packet_queue_lock_;
  std::deque<GDBPacket> packet_queue_;

  std::recursive_mutex unescaped_read_lock_;
  std::vector<uint8_t> unescaped_read_buffer_;

  PacketReceivedHandler packet_received_handler_;
};

#endif  // XBDM_GDB_BRIDGE_GDB_TRANSPORT_H
