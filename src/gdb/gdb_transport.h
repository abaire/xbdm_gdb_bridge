#ifndef XBDM_GDB_BRIDGE_GDB_TRANSPORT_H
#define XBDM_GDB_BRIDGE_GDB_TRANSPORT_H

#include "net/tcp_connection.h"

class GDBTransport : public TCPConnection {
 public:
  GDBTransport(std::string name, int sock, IPAddress address)
      : TCPConnection(std::move(name), sock, std::move(address)) {}
};

#endif  // XBDM_GDB_BRIDGE_GDB_TRANSPORT_H
