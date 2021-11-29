#ifndef XBDM_GDB_BRIDGE_GDB_TRANSPORT_H
#define XBDM_GDB_BRIDGE_GDB_TRANSPORT_H

#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "gdb/gdb_packet.h"
#include "net/tcp_connection.h"

class GDBTransport : public TCPConnection {
 public:
  typedef std::function<void(const std::shared_ptr<GDBPacket> &)>
      PacketReceivedHandler;

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

  std::recursive_mutex unescaped_read_lock_;
  // Buffer of received bytes that have been unescaped and are ready for
  // processing at higher levels.
  std::vector<uint8_t> unescaped_read_buffer_;

  std::recursive_mutex ack_buffer_lock_;
  // List of serialized packets that have been sent but not yet acknowledged by
  // the debugger.
  std::list<std::vector<uint8_t>> ack_buffer_;

  PacketReceivedHandler packet_received_handler_;
};

#endif  // XBDM_GDB_BRIDGE_GDB_TRANSPORT_H
