#include "xbdm_transport.h"

#include <boost/log/trivial.hpp>

#include "rdcp/rdcp_response.h"

bool XBDMTransport::Connect(const IPAddress &address) {
  if (socket_ >= 0) {
    Close();
  }

  BOOST_LOG_TRIVIAL(trace) << "Connecting to XBDM at " << address;
  socket_ = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket_ < 0) {
    return false;
  }

  address_ = address;
  const struct sockaddr_in &addr = address.address();
  if (connect(socket_, reinterpret_cast<struct sockaddr const *>(&addr),
              sizeof(addr))) {
    BOOST_LOG_TRIVIAL(error) << "connect failed " << errno;
    close(socket_);
    socket_ = -1;
    return false;
  }

  return true;
}

void XBDMTransport::Close() {
  state_ = ConnectionState::INIT;
  TCPConnection::Close();

  const std::lock_guard<std::recursive_mutex> lock(request_queue_lock_);
  for (auto &request: request_queue_) {
    request->Abandon();
  }
  request_queue_.clear();
}

void XBDMTransport::SetConnected() {
  if (state_ < ConnectionState::CONNECTED) {
    state_ = ConnectionState::CONNECTED;
  }
}

void XBDMTransport::Send(const std::shared_ptr<RDCPRequest> &request) {
  const std::lock_guard<std::recursive_mutex> lock(request_queue_lock_);
  request_queue_.push_back(request);

  WriteNextRequest();
}

void XBDMTransport::WriteNextRequest() {
  if (state_ != ConnectionState::CONNECTED) {
    return;
  }

  const std::lock_guard<std::recursive_mutex> lock(request_queue_lock_);
  if (request_queue_.empty()) {
    return;
  }

  const auto &request = request_queue_.front();
  BOOST_LOG_TRIVIAL(trace) << "Sending " << *request;
  std::vector<uint8_t> buffer =
      static_cast<std::vector<uint8_t>>(*request);
  TCPConnection::Send(buffer);
}

void XBDMTransport::OnBytesRead() {
  TCPConnection::OnBytesRead();

  const std::lock_guard<std::recursive_mutex> read_lock(read_lock_);
  char const *char_buffer = reinterpret_cast<char *>(read_buffer_.data());

  auto request = request_queue_.front();
  std::shared_ptr<RDCPResponse> response;
  long binary_response_size = 0;
  if (request) {
    binary_response_size = request->ExpectedBinaryResponseSize();
  }

  auto bytes_consumed =
      RDCPResponse::Parse(response, char_buffer, read_buffer_.size(),
                          binary_response_size);
  if (!bytes_consumed) {
    return;
  }

  if (bytes_consumed < 0) {
    bytes_consumed *= -1;
    BOOST_LOG_TRIVIAL(trace) << "Discarding " << bytes_consumed << " bytes";
  }

  ShiftReadBuffer(bytes_consumed);

  BOOST_LOG_TRIVIAL(trace) << "Received response " << *response;

  if (request_queue_.empty()) {
    // On initial connection, XBDM will send an unsolicited OK response.
    HandleInitialConnectResponse(response);
  } else {
    request_queue_.pop_front();
    WriteNextRequest();

    request->Complete(response);
  }
}

void XBDMTransport::HandleInitialConnectResponse(const std::shared_ptr<RDCPResponse>& response) {
  if (response->Status() == StatusCode::OK_CONNECTED) {
    state_ = ConnectionState::CONNECTED;
    return;
  }

  BOOST_LOG_TRIVIAL(error) << "Received unsolicited response " << response->Status();
  Close();
}