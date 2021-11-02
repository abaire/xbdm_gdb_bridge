#include "xbdm_transport.h"

#include <boost/log/trivial.hpp>

#include "rdcp/rdcp_response.h"

bool XBDMTransport::Connect(const IPAddress &address) {
  if (socket_ >= 0) {
    Close();
  }

  socket_ = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket_ < 0) {
    return false;
  }

  address_ = address;
  const struct sockaddr_in &addr = address.address();
  if (connect(socket_, reinterpret_cast<struct sockaddr const *>(&addr),
              sizeof(addr))) {
    BOOST_LOG_TRIVIAL(error) << "connect failed " << errno << std::endl;
    close(socket_);
    socket_ = -1;
    return false;
  }

  return true;
}

void XBDMTransport::Close() {
  state_ = ConnectionState::INIT;
  TCPConnection::Close();
}

void XBDMTransport::SetConnected() {
  if (state_ < ConnectionState::CONNECTED) {
    state_ = ConnectionState::CONNECTED;
  }
}

void XBDMTransport::Send(const RDCPRequest &request) {
  const std::lock_guard<std::mutex> lock(request_queue_lock_);
  request_queue_.push_back(request);

  WriteNextRequest();
}

void XBDMTransport::WriteNextRequest() {
  if (state_ != ConnectionState::CONNECTED) {
    return;
  }

  const std::lock_guard<std::mutex> lock(request_queue_lock_);
  if (request_queue_.empty()) {
    return;
  }

  std::vector<uint8_t> buffer =
      static_cast<std::vector<uint8_t>>(request_queue_.front());
  TCPConnection::Send(buffer);
}

void XBDMTransport::OnBytesRead() {
  TCPConnection::OnBytesRead();

  const std::lock_guard<std::mutex> read_lock(read_lock_);
  char const *char_buffer = reinterpret_cast<char *>(read_buffer_.data());

  RDCPResponse response;
  auto bytes_consumed = response.Parse(char_buffer, read_buffer_.size());
  if (!bytes_consumed) {
    return;
  }

  if (bytes_consumed < 0) {
    bytes_consumed *= -1;
    BOOST_LOG_TRIVIAL(trace)
        << "Discarding " << bytes_consumed << " bytes" << std::endl;
  }

  ShiftReadBuffer(bytes_consumed);
}
