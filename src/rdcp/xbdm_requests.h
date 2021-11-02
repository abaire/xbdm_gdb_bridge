#ifndef XBDM_GDB_BRIDGE_XBDM_REQUESTS_H
#define XBDM_GDB_BRIDGE_XBDM_REQUESTS_H

#include <arpa/inet.h>
#include <string>

#include "net/ip_address.h"
#include "rdcp/rdcp_processed_request.h"
#include "rdcp/rdcp_request.h"
#include "rdcp/rdcp_response.h"
#include "rdcp/rdcp_status_code.h"


struct AltAddr : public RDCPProcessedRequest {
  AltAddr() : RDCPProcessedRequest("altaddr") {}

  void Complete(const std::shared_ptr<RDCPResponse>& response) override {
    RDCPProcessedRequest::Complete(response);
    if (!IsOK()) {
      return;
    }

    auto parsed = RDCPMapResponse(response->Data());
    address = parsed.GetDWORD("addr");

    char buf[64] = {0};
    if (inet_ntop(AF_INET, &address, buf, 64)) {
      address_string = buf;
    } else {
      address_string.clear();
    }
  }

  uint32_t address;
  std::string address_string;
};

struct BreakBase_ : public RDCPProcessedRequest {
  BreakBase_() : RDCPProcessedRequest("break") {}
};

struct BreakAtStart : public BreakBase_ {
  BreakAtStart() : BreakBase_() {
    SetData(" start");
  }
};

#endif  // XBDM_GDB_BRIDGE_XBDM_REQUESTS_H
