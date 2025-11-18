#include "tcp_connection.h"

#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <boost/log/trivial.hpp>

#include "configure.h"
#include "util/logging.h"

void TCPConnection::ShiftReadBuffer(long shift_bytes) {
  if (!shift_bytes) {
    return;
  }

  const std::lock_guard lock(read_lock_);
  if (shift_bytes > read_buffer_.size()) {
    return;
  }

  read_buffer_.erase(read_buffer_.begin(),
                     std::next(read_buffer_.begin(), shift_bytes));
}

size_t TCPConnection::BytesAvailable() {
  const std::lock_guard lock(read_lock_);
  return read_buffer_.size();
}

void TCPConnection::DropReceiveBuffer() {
  const std::lock_guard lock(read_lock_);
  read_buffer_.clear();
}

void TCPConnection::DropSendBuffer() {
  const std::lock_guard lock(write_lock_);
  write_buffer_.clear();
}

void TCPConnection::Send(const uint8_t* buffer, size_t len) {
  const std::lock_guard lock(write_lock_);
  write_buffer_.insert(write_buffer_.end(), buffer, buffer + len);
  SignalProcessingNeeded();
}

bool TCPConnection::HasBufferedData() {
  const std::lock_guard read_lock(read_lock_);
  const std::lock_guard write_lock(write_lock_);

  return !read_buffer_.empty() || !write_buffer_.empty();
}

int TCPConnection::Select(fd_set& read_fds, fd_set& write_fds,
                          fd_set& except_fds) {
  int ret = TCPSocketBase::Select(read_fds, write_fds, except_fds);
  const std::lock_guard lock(socket_lock_);
  if (socket_ < 0) {
    return ret;
  }

  FD_SET(socket_, &read_fds);
  FD_SET(socket_, &except_fds);

  const std::lock_guard write_lock(write_lock_);
  if (!write_buffer_.empty()) {
    FD_SET(socket_, &write_fds);
  }

  return std::max(socket_, ret);
}

bool TCPConnection::Process(const fd_set& read_fds, const fd_set& write_fds,
                            const fd_set& except_fds) {
  if (!TCPSocketBase::Process(read_fds, write_fds, except_fds)) {
    return !is_shutdown_;
  }

  const std::lock_guard lock(socket_lock_);
  if (socket_ < 0) {
    // If the socket was previously connected and is now shutdown, request
    // deletion.
    return !is_shutdown_;
  }

  if (FD_ISSET(socket_, &except_fds)) {
    LOG_TAGGED(trace, name_) << "Socket exception detected.";
    Close();
    return false;
  }

  if (FD_ISSET(socket_, &write_fds)) {
    DoSend();
    if (close_after_flush_ && write_buffer_.empty()) {
      Close();
      close_after_flush_ = false;
      return false;
    }
  }

  if (FD_ISSET(socket_, &read_fds)) {
    if (DoReceive()) {
      OnBytesRead();
    }
  }

  return true;
}

bool TCPConnection::DoReceive() {
  const std::lock_guard socket_lock(socket_lock_);
  std::vector<uint8_t> buffer(1024);

  ssize_t bytes_read = recv(socket_, buffer.data(), buffer.size(), 0);
  if (!bytes_read) {
    LOG_TAGGED(trace, name_) << "remote closed socket " << *this;
    Close();
    return false;
  }
  if (bytes_read < 0) {
    LOG_TAGGED(trace, name_) << "recv returned " << bytes_read
                             << " errno: " << errno << " " << *this;
    Close();
    return false;
  }

  const std::lock_guard read_lock(read_lock_);
  buffer.resize(bytes_read);
  read_buffer_.insert(read_buffer_.end(), buffer.begin(), buffer.end());
  return true;
}

void TCPConnection::DoSend() {
  const std::lock_guard socket_lock(socket_lock_);
  const std::lock_guard write_lock(write_lock_);

  ssize_t bytes_sent =
      send(socket_, write_buffer_.data(), write_buffer_.size(), 0);
  if (bytes_sent < 0) {
    LOG_TAGGED(trace, name_)
        << "send returned " << bytes_sent << " errno: " << errno;
    Close();
    return;
  }

#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
  {
    std::string data(write_buffer_.begin(), write_buffer_.begin() + bytes_sent);
    // Special case XBDM message terminators to condense log.
    boost::algorithm::trim_right(data);

    bool maybeBinary = false;
    auto bytes_checked = 0;
    const auto data_end = data.end();
    for (auto i = data.begin(); i != data_end && bytes_checked < 256;
         ++i, ++bytes_checked) {
      if (!std::isprint(*i)) {
        maybeBinary = true;
        break;
      }
    }
    if (maybeBinary) {
      LOG_TAGGED(trace, name_)
          << "-> Sent " << bytes_sent << " bytes (binary)" << std::endl;
    } else {
      LOG_TAGGED(trace, name_)
          << "-> Sent " << bytes_sent << " bytes" << std::endl
          << data << std::endl;
    }
  }
#endif

  write_buffer_.erase(write_buffer_.begin(),
                      std::next(write_buffer_.begin(), bytes_sent));
}

std::vector<uint8_t>::iterator TCPConnection::FirstIndexOf(uint8_t element) {
  const std::lock_guard lock(read_lock_);
  return std::find(read_buffer_.begin(), read_buffer_.end(), element);
}

std::vector<uint8_t>::iterator TCPConnection::FirstIndexOf(
    const std::vector<uint8_t>& pattern) {
  const std::lock_guard lock(read_lock_);

  return std::search(read_buffer_.begin(), read_buffer_.end(), pattern.begin(),
                     pattern.end());
}

void TCPConnection::FlushAndClose() {
  const std::lock_guard write_lock(write_lock_);

  if (write_buffer_.empty()) {
    Close();
    return;
  }

  close_after_flush_ = true;
}
