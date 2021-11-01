#include "ip_transport.h"

#include <algorithm>
#include <sys/select.h>
#include <unistd.h>

#include <boost/log/trivial.hpp>

void IPTransport::SetConnection(int sock, const Address &address) {
  const std::lock_guard<std::mutex> lock(socket_lock_);
  socket_ = sock;
  address_ = address;
}

void IPTransport::ShiftReadBuffer(long shift_bytes) {
  if (!shift_bytes) {
    return;
  }

  const std::lock_guard<std::mutex> lock(read_lock_);
  if (shift_bytes > read_buffer_.size()) {
    return;
  }

  read_buffer_.erase(read_buffer_.begin(), std::next(read_buffer_.begin(), shift_bytes));
}

size_t IPTransport::BytesAvailable() {
  const std::lock_guard<std::mutex> lock(read_lock_);
  return read_buffer_.size();
}

void IPTransport::Close() {
  if (socket_ < 0) {
    return;
  }
  const std::lock_guard<std::mutex> lock(socket_lock_);
  shutdown(socket_, SHUT_RDWR);
  close(socket_);
  socket_ = -1;
}

void IPTransport::DropReceiveBuffer() {
  const std::lock_guard<std::mutex> lock(read_lock_);
  read_buffer_.clear();
}

void IPTransport::DropSendBuffer() {
  const std::lock_guard<std::mutex> lock(write_lock_);
  write_buffer_.clear();
}

void IPTransport::Send(const uint8_t *buffer, size_t len) {
  const std::lock_guard<std::mutex> lock(write_lock_);
  write_buffer_.insert(write_buffer_.end(), buffer, buffer + len);
}

bool IPTransport::HasBufferedData() {
  const std::lock_guard<std::mutex> read_lock(read_lock_);
  const std::lock_guard<std::mutex> write_lock(write_lock_);

  return !read_buffer_.empty() || !write_buffer_.empty();
}

void IPTransport::Select(fd_set &read_fds, fd_set &write_fds,
                         fd_set &except_fds) {
  const std::lock_guard<std::mutex> lock(socket_lock_);
  if (socket_ < 0) {
    return;
  }

  FD_SET(socket_, &read_fds);
  FD_SET(socket_, &except_fds);

  const std::lock_guard<std::mutex> write_lock(write_lock_);
  if (!write_buffer_.empty()) {
    FD_SET(socket_, &write_fds);
  }
}

void IPTransport::Process(const fd_set &read_fds, const fd_set &write_fds,
                          const fd_set &except_fds) {
  const std::lock_guard<std::mutex> lock(socket_lock_);
  if (socket_ < 0) {
    return;
  }

  if (FD_ISSET(socket_, &except_fds)) {
    BOOST_LOG_TRIVIAL(trace) << "Socket exception detected." << std::endl;
    Close();
    return;
  }

  if (FD_ISSET(socket_, &read_fds)) {
    DoReceive();
    OnBytesRead();
  }

  if (FD_ISSET(socket_, &write_fds)) {
    DoSend();
  }
}

void IPTransport::DoReceive() {
  const std::lock_guard<std::mutex> socket_lock(socket_lock_);
  std::vector<uint8_t> buffer(1024);

  ssize_t bytes_read = recv(socket_, buffer.data(), buffer.size(), 0);
  if (!bytes_read) {
    Close();
    return;
  }
  if (bytes_read < 0) {
    BOOST_LOG_TRIVIAL(trace)
        << "recv returned " << bytes_read << " errno: " << errno << std::endl;
    Close();
    return;
  }

  const std::lock_guard<std::mutex> read_lock(read_lock_);
  buffer.resize(bytes_read);
  read_buffer_.insert(read_buffer_.end(), buffer.begin(), buffer.end());
}

void IPTransport::DoSend() {
  const std::lock_guard<std::mutex> socket_lock(socket_lock_);
  const std::lock_guard<std::mutex> write_lock(write_lock_);

  ssize_t bytes_sent = send(socket_, write_buffer_.data(), write_buffer_.size(), 0);
  if (bytes_sent < 0) {
    BOOST_LOG_TRIVIAL(trace)
        << "send returned " << bytes_sent << " errno: " << errno << std::endl;
    Close();
    return;
  }

  write_buffer_.erase(write_buffer_.begin(), std::next(write_buffer_.begin(), bytes_sent));
}

std::vector<uint8_t>::iterator IPTransport::FirstIndexOf(uint8_t element) {
  const std::lock_guard<std::mutex> lock(read_lock_);
  return std::find(read_buffer_.begin(), read_buffer_.end(), element);
}

std::vector<uint8_t>::iterator IPTransport::FirstIndexOf(
    const std::vector<uint8_t> &pattern) {
  const std::lock_guard<std::mutex> lock(read_lock_);

  return std::search(read_buffer_.begin(), read_buffer_.end(), pattern.begin(), pattern.end());
}
