#include "ip_address.h"

#include <arpa/inet.h>
#include <strings.h>
#include <sys/socket.h>

#include <cctype>
#include <cstring>
#include <ostream>

IPAddress::IPAddress(const std::string& addr) {
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
  } else if (strcasecmp(hostname_.c_str(), "localhost") == 0) {
    addr_.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  } else {
    addr_.sin_addr.s_addr = htonl(inet_network(hostname_.c_str()));
    if (addr_.sin_addr.s_addr == INADDR_NONE) {
      // See if the value is actually a valid port number.
      uint32_t val = strtoul(addr.c_str(), nullptr, 10);
      if (val > 0 && val < 0xFFFF) {
        addr_.sin_port = htons(val & 0xFFFF);
      }
      addr_.sin_addr.s_addr = INADDR_ANY;
    }
  }
}

IPAddress::IPAddress(uint16_t port) {
  addr_.sin_family = AF_INET;
  addr_.sin_port = htons(port);
  addr_.sin_addr.s_addr = INADDR_ANY;
}

IPAddress::IPAddress(const std::string& addr, uint16_t default_port)
    : IPAddress(addr) {
  if (!addr_.sin_port) {
    addr_.sin_port = htons(default_port);
  }
}

IPAddress::IPAddress(const sockaddr_in& addr) : hostname_(), addr_(addr) {}

IPAddress IPAddress::WithPort(uint16_t port) const {
  auto addr = addr_;
  addr.sin_port = htons(port);

  return IPAddress(addr);
}

bool IPAddress::IsIPv4Address(const std::string& addr) {
  if (addr.empty()) {
    return false;
  }

  auto split = addr.find(':');
  std::string ip_part;
  std::string port_part;

  if (split != std::string::npos) {
    if (addr.find(':', split + 1) != std::string::npos) {
      return false;
    }
    ip_part = addr.substr(0, split);
    port_part = addr.substr(split + 1);

    if (port_part.empty()) {
      return false;
    }
    for (char c : port_part) {
      if (!std::isdigit(static_cast<unsigned char>(c))) {
        return false;
      }
    }
    try {
      unsigned long port = std::stoul(port_part);
      if (port > 65535) {
        return false;
      }
    } catch (...) {
      return false;
    }
  } else {
    ip_part = addr;
  }

  if (ip_part.empty()) {
    return split != std::string::npos;
  }

  if (strcasecmp(ip_part.c_str(), "localhost") == 0) {
    return true;
  }

  struct in_addr in{};
  if (inet_pton(AF_INET, ip_part.c_str(), &in) == 1) {
    return true;
  }

  return false;
}

std::ostream& operator<<(std::ostream& os, IPAddress const& addr) {
  char buf[64] = {0};
  inet_ntop(addr.addr_.sin_family, &addr.addr_.sin_addr, buf, 64);
  return os << buf << ":" << ntohs(addr.addr_.sin_port);
}

bool IPAddress::operator<(const IPAddress& other) const {
  const struct sockaddr_in& oaddr = other.addr_;
  if (addr_.sin_addr.s_addr < oaddr.sin_addr.s_addr) {
    return true;
  }
  if (addr_.sin_addr.s_addr > oaddr.sin_addr.s_addr) {
    return true;
  }
  return addr_.sin_port < oaddr.sin_port;
}

bool IPAddress::operator==(const IPAddress& other) const {
  if (hostname_ != other.hostname_) {
    return false;
  }

  return !memcmp(&addr_, &other.addr_, sizeof(addr_));
}
