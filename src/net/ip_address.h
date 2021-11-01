#ifndef XBDM_GDB_BRIDGE_SRC_NET_ADDRESS_H_
#define XBDM_GDB_BRIDGE_SRC_NET_ADDRESS_H_

#include <netinet/in.h>

#include <string>

class IPAddress {
 public:
  IPAddress() = default;
  explicit IPAddress(const std::string &addr);
  IPAddress(const std::string &addr, uint16_t default_port);
  explicit IPAddress(const struct sockaddr_in &addr);

  [[nodiscard]] const std::string &hostname() const { return hostname_; }
  [[nodiscard]] const struct sockaddr_in &address() const { return addr_; }
  [[nodiscard]] struct in_addr ip() const { return addr_.sin_addr; }
  [[nodiscard]] uint16_t port() const { return addr_.sin_port; }

  explicit operator struct sockaddr const *() const {
    return reinterpret_cast<struct sockaddr const *>(&addr_);
  }
  bool operator<(const IPAddress &other) const;

 private:
  friend std::ostream &operator<<(std::ostream &, IPAddress const &);

 private:
  std::string hostname_;
  struct sockaddr_in addr_ {};
};

#endif  // XBDM_GDB_BRIDGE_SRC_NET_ADDRESS_H_
