#include "discoverer.h"

#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include <boost/log/trivial.hpp>

#include "util/timer.h"

#define XBDM_DISCOVERY_PORT 731

struct NAPPacket {
  enum Type : uint8_t { INVALID = 0, LOOKUP, REPLY, WILDCARD };

  uint8_t type;
  std::string name;

  NAPPacket() : type(Type::INVALID) {}
  explicit NAPPacket(Type t) : type(t) {}
  NAPPacket(Type t, std::string n) : type(t), name(std::move(n)) {}

  [[nodiscard]] std::vector<uint8_t> Serialize() const {
    std::vector<uint8_t> ret(2 + name.size());

    ret[0] = type;
    uint8_t name_len = name.size() & 0xFF;
    ret[1] = name_len;
    memcpy(&ret[2], name.c_str(), name_len);

    return ret;
  }

  uint32_t Deserialize(const std::vector<uint8_t> &buffer) {
    return Deserialize(buffer.data(), buffer.size());
  }

  uint32_t Deserialize(const uint8_t *buffer, size_t buffer_len) {
    type = Type::INVALID;
    if (buffer_len < 2) {
      return 0;
    }

    type = buffer[0];
    uint32_t name_len = buffer[1];
    if (buffer_len < 2 + name_len) {
      return 0;
    }

    name.assign(reinterpret_cast<char const *>(buffer) + 2, name_len);

    return 2 + name_len;
  }
};

Discoverer::Discoverer(Address bind_address)
    : bind_address_(std::move(bind_address)) {
  socket_ = -1;
}

std::set<Discoverer::XBDMServer> Discoverer::Run(int wait_milliseconds) {
  assert(wait_milliseconds >= 0);

  std::set<Discoverer::XBDMServer> ret;

  if (!BindSocket()) {
    return ret;
  }

  if (!SendDiscoveryPacket()) {
    return ret;
  }

  fd_set recv_fds;
  FD_ZERO(&recv_fds);
  FD_SET(socket_, &recv_fds);

  struct timeval timeout = {.tv_sec = 0, .tv_usec = 1000 * wait_milliseconds};

  Timer timer;
  timer.Start();
  XBDMServer response;

  do {
    int fds = select(socket_ + 1, &recv_fds, nullptr, nullptr, &timeout);
    if (fds <= 0) {
      return ret;
    }

    assert(FD_ISSET(socket_, &recv_fds) &&
           "select() returned positive value but socket not readable");
    if (ReceiveResponse(response)) {
      ret.insert(response);
    }

    wait_milliseconds -= static_cast<int>(timer.MillisecondsElapsed());
    timeout.tv_usec = 1000 * wait_milliseconds;
  } while (wait_milliseconds > 0);

  return ret;
}

bool Discoverer::BindSocket() {
  if (socket_) {
    return true;
  }

  socket_ = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_ < 0) {
    return false;
  }

  int enabled = 1;
  const struct sockaddr_in &addr = bind_address_.address();

  if (setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, &enabled,
                 sizeof(enabled))) {
    goto close_and_fail;
  }

  if (bind(socket_, reinterpret_cast<struct sockaddr const *>(&addr),
           sizeof(addr))) {
    goto close_and_fail;
  }

  return true;

close_and_fail:
  shutdown(socket_, SHUT_RDWR);
  close(socket_);
  socket_ = -1;
  return false;
}

bool Discoverer::SendDiscoveryPacket() const {
  NAPPacket packet(NAPPacket::WILDCARD);
  std::vector<uint8_t> buffer = packet.Serialize();
  struct sockaddr_in addr {
    .sin_family = AF_INET, .sin_port = XBDM_DISCOVERY_PORT,
    .sin_addr = {.s_addr = INADDR_BROADCAST},
  };

  ssize_t bytes_sent =
      sendto(socket_, buffer.data(), buffer.size(), 0,
             reinterpret_cast<struct sockaddr const *>(&addr), sizeof(addr));
  return bytes_sent == buffer.size();
}

bool Discoverer::ReceiveResponse(XBDMServer &result) const {
  NAPPacket packet;
  uint8_t buffer[257] = {0};
  struct sockaddr_in recv_addr {};
  socklen_t recv_addr_len = sizeof(recv_addr);

  ssize_t received =
      recvfrom(socket_, buffer, sizeof(buffer), 0,
               reinterpret_cast<struct sockaddr *>(&recv_addr), &recv_addr_len);
  if (received < 0) {
    BOOST_LOG_TRIVIAL(trace) << "recvfrom failed" << std::endl;
    return false;
  }

  auto bytes_processed = packet.Deserialize(buffer, received);
  if (bytes_processed != received) {
    BOOST_LOG_TRIVIAL(trace)
        << "Received " << received << " bytes but NAPPacket only processed "
        << bytes_processed << std::endl;
  }

  if (packet.type != NAPPacket::REPLY) {
    BOOST_LOG_TRIVIAL(trace) << "Received unexpected response packet of type "
                             << packet.type << std::endl;
    return false;
  }

  result.name = packet.name;
  result.address = Address(recv_addr);

  return true;
}

bool Discoverer::XBDMServer::operator<(
    const Discoverer::XBDMServer &other) const {
  return address < other.address;
}
