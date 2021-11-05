#include "gdb_transport.h"

#include <boost/log/trivial.hpp>
#include <utility>

static constexpr uint8_t kAck[1] = {'+'};
static constexpr uint8_t kNAck[1] = {'-'};

GDBTransport::GDBTransport(std::string name, int sock, IPAddress address,
                           PacketReceivedHandler handler)
    : TCPConnection(std::move(name), sock, std::move(address)),
      packet_received_handler_(std::move(handler)) {}

void GDBTransport::Send(const GDBPacket &packet) {
  TCPConnection::Send(packet.Serialize());
}

void GDBTransport::OnBytesRead() {
  TCPConnection::OnBytesRead();

  {
    const std::lock_guard<std::recursive_mutex> unescaped_lock(
        unescaped_read_lock_);
    size_t read_buffer_size = unescaped_read_buffer_.size();

    {
      const std::lock_guard<std::recursive_mutex> lock(read_lock_);

      auto it = read_buffer_.begin();
      for (; it != read_buffer_.end(); ++it) {
        switch (*it) {
          case '+':
            BOOST_LOG_TRIVIAL(trace) << "Ack received";
            continue;

          case '-':
            BOOST_LOG_TRIVIAL(warning) << "Remote requested resend.";
            continue;

          case 0x03:
            unescaped_read_buffer_.push_back(0x03);
            continue;

          default:
            break;
        }

        break;
      }
      ShiftReadBuffer(it - read_buffer_.begin());

      long bytes_consumed =
          GDBPacket::UnescapeBuffer(read_buffer_, unescaped_read_buffer_);
      ShiftReadBuffer(bytes_consumed);
    }

    if (unescaped_read_buffer_.size() == read_buffer_size) {
      return;
    }
  }

  ProcessUnescapedReadBuffer();
}

void GDBTransport::ProcessUnescapedReadBuffer() {
  std::list<std::shared_ptr<GDBPacket>> packets;

  {
    const std::lock_guard<std::recursive_mutex> unescaped_lock(
        unescaped_read_lock_);
    auto it = unescaped_read_buffer_.begin();
    auto it_end = unescaped_read_buffer_.end();

    GDBPacket packet;
    while (it != it_end) {
      if (*it == 0x03) {
        uint8_t interrupt_command = 0x03;
        packets.emplace_back(
            std::make_shared<GDBPacket>(&interrupt_command, 1));
        ++it;
        if (!no_ack_mode_) {
          TCPConnection::Send(kAck, 1);
        }
        continue;
      }

      long bytes_consumed = packet.Parse(it.base(), it_end - it);
      if (!bytes_consumed) {
        break;
      }
      packets.push_back(std::make_shared<GDBPacket>(packet));
      it += bytes_consumed;
      if (!no_ack_mode_) {
        TCPConnection::Send(kAck, 1);
      }
    }

    unescaped_read_buffer_.erase(unescaped_read_buffer_.begin(), it);
  }

  for (auto &packet : packets) {
    packet_received_handler_(packet);
  }
}
