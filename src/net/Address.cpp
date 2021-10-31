#include "Address.h"

#include <arpa/inet.h>
#include <ostream>
#include <sys/socket.h>

Address::Address(const std::string &addr) {
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

Address::Address(const std::string &addr, uint16_t default_port)
    : Address(addr) {
  if (!addr_.sin_port) {
    addr_.sin_port = htons(default_port);
  }
}

std::ostream &operator<<(std::ostream &os, Address const &addr) {
  char buf[64];
  inet_ntop(addr.addr_.sin_family, &addr.addr_.sin_addr, buf, 64);
  return os << buf << ":" << ntohs(addr.addr_.sin_port);
}
