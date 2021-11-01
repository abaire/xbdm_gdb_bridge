#include "tcp_connection.h"

#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <boost/log/trivial.hpp>

void TCPConnection::ShiftReadBuffer(long shift_bytes) {
  if (!shift_bytes) {
    return;
  }

  const std::lock_guard<std::mutex> lock(read_lock_);
  if (shift_bytes > read_buffer_.size()) {
    return;
  }

  read_buffer_.erase(read_buffer_.begin(), std::next(read_buffer_.begin(), shift_bytes));
}

size_t TCPConnection::BytesAvailable() {
  const std::lock_guard<std::mutex> lock(read_lock_);
  return read_buffer_.size();
}

void TCPConnection::DropReceiveBuffer() {
  const std::lock_guard<std::mutex> lock(read_lock_);
  read_buffer_.clear();
}

void TCPConnection::DropSendBuffer() {
  const std::lock_guard<std::mutex> lock(write_lock_);
  write_buffer_.clear();
}

void TCPConnection::Send(const uint8_t *buffer, size_t len) {
  const std::lock_guard<std::mutex> lock(write_lock_);
  write_buffer_.insert(write_buffer_.end(), buffer, buffer + len);
}

bool TCPConnection::HasBufferedData() {
  const std::lock_guard<std::mutex> read_lock(read_lock_);
  const std::lock_guard<std::mutex> write_lock(write_lock_);

  return !read_buffer_.empty() || !write_buffer_.empty();
}

int TCPConnection::Select(fd_set &read_fds, fd_set &write_fds,
                         fd_set &except_fds) {
  const std::lock_guard<std::mutex> lock(socket_lock_);
  if (socket_ < 0) {
    return socket_;
  }

  FD_SET(socket_, &read_fds);
  FD_SET(socket_, &except_fds);

  const std::lock_guard<std::mutex> write_lock(write_lock_);
  if (!write_buffer_.empty()) {
    FD_SET(socket_, &write_fds);
  }

  return socket_;
}

bool TCPConnection::Process(const fd_set &read_fds, const fd_set &write_fds,
                          const fd_set &except_fds) {
  const std::lock_guard<std::mutex> lock(socket_lock_);
  if (socket_ < 0) {
    return false;
  }

  if (FD_ISSET(socket_, &except_fds)) {
    BOOST_LOG_TRIVIAL(trace) << "Socket exception detected." << std::endl;
    Close();
    return false;
  }

  if (FD_ISSET(socket_, &read_fds)) {
    DoReceive();
    OnBytesRead();
  }

  if (FD_ISSET(socket_, &write_fds)) {
    DoSend();
  }

  return true;
}

void TCPConnection::DoReceive() {
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

void TCPConnection::DoSend() {
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

std::vector<uint8_t>::iterator TCPConnection::FirstIndexOf(uint8_t element) {
  const std::lock_guard<std::mutex> lock(read_lock_);
  return std::find(read_buffer_.begin(), read_buffer_.end(), element);
}

std::vector<uint8_t>::iterator TCPConnection::FirstIndexOf(
    const std::vector<uint8_t> &pattern) {
  const std::lock_guard<std::mutex> lock(read_lock_);

  return std::search(read_buffer_.begin(), read_buffer_.end(), pattern.begin(), pattern.end());
}
