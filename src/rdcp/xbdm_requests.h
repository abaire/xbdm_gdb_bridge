#ifndef XBDM_GDB_BRIDGE_XBDM_REQUESTS_H
#define XBDM_GDB_BRIDGE_XBDM_REQUESTS_H

#include "net/ip_address.h"
#include "rdcp/rdcp_request.h"
#include "rdcp/rdcp_response.h"
#include "rdcp/rdcp_status_code.h"


struct AltAddr : public ProcessedRequest {
 public:
  AltAddr() : RDCPRequest("altaddr") {}

  void Complete(const std::shared_ptr<RDCPResponse>& response) override {
    ProcessedRequest::Complete(response);
    if (!IsOK()) {
      return;
    }


  }

  IPAddress address{};
};

#endif  // XBDM_GDB_BRIDGE_XBDM_REQUESTS_H
