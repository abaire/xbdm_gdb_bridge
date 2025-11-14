#include "xbdm_notification_transport.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "util/logging.h"

static constexpr uint8_t kTerminator[] = {'\r', '\n'};
static constexpr long kTerminatorLen =
    sizeof(kTerminator) / sizeof(kTerminator[0]);

static const char* ParseMessage(const char* buffer, const char* buffer_end);

XBDMNotificationTransport::XBDMNotificationTransport(
    std::string name, int sock, const IPAddress& address,
    NotificationHandler handler)
    : TCPConnection(std::move(name), sock, address),
      notification_handler_(std::move(handler)),
      hello_received_(false) {}

void XBDMNotificationTransport::OnBytesRead() {
  TCPConnection::OnBytesRead();

  const std::lock_guard<std::recursive_mutex> read_lock(read_lock_);
  char const* buffer = reinterpret_cast<char*>(read_buffer_.data());

  auto buffer_end = buffer + read_buffer_.size();
  auto message_end = ParseMessage(buffer, buffer_end);
  long bytes_processed = 0;

  while (message_end) {
    long message_len = message_end - buffer;
    long packet_len = message_len + kTerminatorLen;
    bytes_processed += packet_len;

    HandleNotification(buffer, message_len);
    buffer += packet_len;
    message_end = ParseMessage(buffer, buffer_end);
  }

  ShiftReadBuffer(bytes_processed);
}

void XBDMNotificationTransport::HandleNotification(const char* message,
                                                   long message_len) {
  if (message_len == 5 && !memcmp(message, "hello", message_len)) {
    hello_received_ = true;
    return;
  }

  auto notification = ParseXBDMNotification(message, message_len);
  if (!notification) {
    std::string dbg_message(message, message + message_len);
    LOG(warning) << "Unhandled notification '" << dbg_message << "'"
                 << std::endl;
    return;
  }

  notification_handler_(notification);
}

static const char* ParseMessage(const char* buffer, const char* buffer_end) {
  auto terminator = std::search(buffer, buffer_end, kTerminator,
                                kTerminator + kTerminatorLen);
  if (terminator == buffer_end) {
    return nullptr;
  }

  return terminator;
}
