#include "xbdm_transport.h"

void XBDMTransport::Close() {
  state_ = ConnectionState::INIT;
  IPTransport::Close();
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

  std::vector<uint8_t> buffer = request_queue_.front();
  IPTransport::Send(buffer);
}

void XBDMTransport::OnBytesRead() { IPTransport::OnBytesRead(); }
