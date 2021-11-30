#include "xbdm_transport.h"

#include <boost/log/trivial.hpp>

#include "configure.h"
#include "rdcp/rdcp_request.h"
#include "rdcp/rdcp_response.h"
#include "util/logging.h"

bool XBDMTransport::Connect(const IPAddress &address) {
  if (socket_ >= 0) {
    Close();
  }

  LOG_XBDM(trace) << "Connecting to XBDM at " << address;
  socket_ = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket_ < 0) {
    return false;
  }

  address_ = address;
  const struct sockaddr_in &addr = address.Address();
  if (connect(socket_, reinterpret_cast<struct sockaddr const *>(&addr),
              sizeof(addr))) {
    LOG_XBDM(error) << "connect failed " << errno;
    close(socket_);
    socket_ = -1;
    return false;
  }

  LOG_XBDM(trace) << "Connected.";
  return true;
}

void XBDMTransport::Close() {
  state_ = ConnectionState::INIT;
  TCPConnection::Close();

  const std::lock_guard<std::recursive_mutex> lock(request_queue_lock_);
  for (auto &request : request_queue_) {
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
#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
  LOG_XBDM(trace) << "XBDM request: " << *request;
  request_sent_.Start();
#endif
  std::vector<uint8_t> buffer = static_cast<std::vector<uint8_t>>(*request);
  TCPConnection::Send(buffer);
}

void XBDMTransport::OnBytesRead() {
  TCPConnection::OnBytesRead();

  const std::lock_guard<std::recursive_mutex> read_lock(read_lock_);
  char const *char_buffer = reinterpret_cast<char *>(read_buffer_.data());

  std::shared_ptr<RDCPRequest> request;
  if (!request_queue_.empty()) {
    request = request_queue_.front();
  }
  std::shared_ptr<RDCPResponse> response;
  long binary_response_size = 0;
  if (request) {
    binary_response_size = request->ExpectedBinaryResponseSize();
  }

  auto bytes_consumed = RDCPResponse::Parse(
      response, char_buffer, read_buffer_.size(), binary_response_size);
  if (!bytes_consumed) {
    return;
  }

  if (bytes_consumed < 0) {
    bytes_consumed *= -1;
    LOG_XBDM(trace) << "Discarding " << bytes_consumed << " bytes";
  }

  ShiftReadBuffer(bytes_consumed);

#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
  LOG_XBDM(trace) << "Response: " << *response;
#endif

  if (request_queue_.empty()) {
    // On initial connection, XBDM will send an unsolicited OK response.
    HandleInitialConnectResponse(response);
  } else {
    if (response->Status() == OK_SEND_BINARY_DATA) {
      auto payload = request->BinaryPayload();
      if (!payload) {
        LOG_XBDM(error) << "Binary payload requested from remote but "
                           "not attached to request.";
        // TODO: Close connection forcefully and complete request as an error?
        assert(!"Binary payload requested from remote but not attached to request.");
      }

      TCPConnection::Send(*payload);

      // The request will be finished by the response to the binary being sent.
      return;
    }

    request_queue_.pop_front();
    WriteNextRequest();

#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
    LOG_XBDM(trace) << "Request " << *request << " round trip "
                    << request_sent_.FractionalMillisecondsElapsed() << " ms";
    request_sent_.Start();
#endif
    request->Complete(response);
#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
    LOG_XBDM(trace) << "Completion of request " << *request << " took "
                    << request_sent_.FractionalMillisecondsElapsed() << " ms";
#endif
  }
}

void XBDMTransport::HandleInitialConnectResponse(
    const std::shared_ptr<RDCPResponse> &response) {
  if (response->Status() == StatusCode::OK_CONNECTED) {
    state_ = ConnectionState::CONNECTED;
    return;
  }

  LOG_XBDM(error) << "Received unsolicited response " << response->Status();
  Close();
}
