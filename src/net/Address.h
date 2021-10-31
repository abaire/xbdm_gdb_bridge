#ifndef XBDM_GDB_BRIDGE_SRC_NET_ADDRESS_H_
#define XBDM_GDB_BRIDGE_SRC_NET_ADDRESS_H_

#include <netinet/in.h>
#include <string>

class Address {
public:
  Address(const std::string &addr);
  Address(const std::string &addr, uint16_t default_port);

  [[nodiscard]] const std::string &hostname() const { return hostname_; }
  [[nodiscard]] struct in_addr ip() const { return addr_.sin_addr; }
  [[nodiscard]] uint16_t port() const { return addr_.sin_port; }

private:
  std::string hostname_;
  struct sockaddr_in addr_;

  friend std::ostream &operator<<(std::ostream &os, Address const &m);
};

#endif // XBDM_GDB_BRIDGE_SRC_NET_ADDRESS_H_
