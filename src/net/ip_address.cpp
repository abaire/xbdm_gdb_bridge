#include "ip_address.h"

#include <arpa/inet.h>
#include <sys/socket.h>

#include <ostream>

IPAddress::IPAddress(const std::string &addr) {
  auto split = addr.find(':');

  addr_.sin_family = AF_INET;
  if (split == std::string::npos) {
    addr_.sin_port = 0;
  } else {
    uint32_t val = strtoul(&addr.c_str()[split + 1], nullptr, 10);
    addr_.sin_port = htons(val & 0xFFFF);
  }

  hostname_ = addr.substr(0, split);
  if (hostname_.empty()) {
    addr_.sin_addr.s_addr = INADDR_ANY;
  } else {
    addr_.sin_addr.s_addr = htonl(inet_network(hostname_.c_str()));
    if (addr_.sin_addr.s_addr == INADDR_NONE) {
      addr_.sin_addr.s_addr = INADDR_ANY;
    }
  }
}

IPAddress::IPAddress(const std::string &addr, uint16_t default_port)
    : IPAddress(addr) {
  if (!addr_.sin_port) {
    addr_.sin_port = htons(default_port);
  }
}

IPAddress::IPAddress(const sockaddr_in &addr) : hostname_(), addr_(addr) {}

std::ostream &operator<<(std::ostream &os, IPAddress const &addr) {
  char buf[64] = {0};
  inet_ntop(addr.addr_.sin_family, &addr.addr_.sin_addr, buf, 64);
  return os << buf << ":" << ntohs(addr.addr_.sin_port);
}

bool IPAddress::operator<(const IPAddress &other) const {
  const struct sockaddr_in &oaddr = other.addr_;
  if (addr_.sin_addr.s_addr < oaddr.sin_addr.s_addr) {
    return true;
  }
  if (addr_.sin_addr.s_addr > oaddr.sin_addr.s_addr) {
    return true;
  }
  return addr_.sin_port < oaddr.sin_port;
}
