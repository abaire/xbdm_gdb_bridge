#ifndef XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_REQUEST_H_
#define XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_REQUEST_H_

#include <vector>

class RDCPRequest {
 public:
  operator std::vector<uint8_t>() const;
};

#endif  // XBDM_GDB_BRIDGE_SRC_RDCP_RDCP_REQUEST_H_
