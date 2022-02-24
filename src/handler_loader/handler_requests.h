#ifndef XBDM_GDB_BRIDGE_SRC_HANDLER_LOADER_HANDLER_REQUESTS_H_
#define XBDM_GDB_BRIDGE_SRC_HANDLER_LOADER_HANDLER_REQUESTS_H_

#include <arpa/inet.h>

#include <boost/optional/optional_io.hpp>
#include <optional>
#include <ostream>
#include <set>
#include <string>

#include "net/ip_address.h"
#include "rdcp/rdcp_processed_request.h"
#include "rdcp/rdcp_request.h"
#include "rdcp/rdcp_response.h"
#include "rdcp/rdcp_status_code.h"
#include "rdcp/types/memory_region.h"
#include "rdcp/types/module.h"
#include "rdcp/types/thread_context.h"
#include "rdcp/xbdm_stop_reasons.h"
#include "util/parsing.h"

struct HandlerInvokeSimple : public RDCPProcessedRequest {
  explicit HandlerInvokeSimple(const std::string& command,
                               const std::string& args = "")
      : RDCPProcessedRequest(command) {}
};

struct HandlerBL2Reserve : public RDCPProcessedRequest {
  explicit HandlerBL2Reserve(uint32_t image_size);
  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override;

  uint32_t allocated_address{0};
};

struct HandlerBL2Load : public RDCPProcessedRequest {
  HandlerBL2Load(uint32_t image_base, std::vector<uint8_t> buffer);

  [[nodiscard]] const std::vector<uint8_t>* BinaryPayload() override {
    return &binary_payload;
  }

  std::vector<uint8_t> binary_payload;
};

#endif  // XBDM_GDB_BRIDGE_SRC_HANDLER_LOADER_HANDLER_REQUESTS_H_
