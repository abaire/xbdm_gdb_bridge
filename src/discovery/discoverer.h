#ifndef XBDM_GDB_BRIDGE_SRC_DISCOVERY_DISCOVERER_H_
#define XBDM_GDB_BRIDGE_SRC_DISCOVERY_DISCOVERER_H_

#include <set>
#include <string>

#include "net/ip_address.h"

class Discoverer {
 public:
  struct XBDMServer {
    std::string name;
    IPAddress address;

    bool operator<(const XBDMServer& other) const;
  };

 public:
  explicit Discoverer(IPAddress bind_address);

  //! Sends a discovery packet and waits `wait_milliseconds` for replies.
  std::set<XBDMServer> Run(int wait_milliseconds = 50);

 private:
  bool BindSocket();
  [[nodiscard]] bool SendDiscoveryPacket() const;
  [[nodiscard]] bool ReceiveResponse(XBDMServer& result) const;

 private:
  IPAddress bind_address_;
  int socket_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_DISCOVERY_DISCOVERER_H_
