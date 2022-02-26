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
      : RDCPProcessedRequest(command) {
    if (!args.empty()) {
      SetData(args);
    }
  }
};

struct HandlerInvokeMultiline : public RDCPProcessedRequest {
  explicit HandlerInvokeMultiline(const std::string& command,
                                  const std::string& args = "")
      : RDCPProcessedRequest(command) {}

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_MULTILINE_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override;
};

struct HandlerDDXTReserve : public RDCPProcessedRequest {
  explicit HandlerDDXTReserve(uint32_t image_size);
  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override;

  uint32_t allocated_address{0};
};

struct HandlerDDXTLoad : public RDCPProcessedRequest {
  HandlerDDXTLoad(uint32_t image_base, std::vector<uint8_t> buffer,
                  const std::vector<uint32_t>& tls_callbacks,
                  uint32_t entrypoint);

  [[nodiscard]] const std::vector<uint8_t>* BinaryPayload() override {
    return &binary_payload;
  }

  std::vector<uint8_t> binary_payload;
};

struct HandlerDDXTExport : public RDCPProcessedRequest {
  HandlerDDXTExport(const std::string& module_name, uint32_t ordinal,
                    uint32_t address, const std::string& export_name = "");
};

#endif  // XBDM_GDB_BRIDGE_SRC_HANDLER_LOADER_HANDLER_REQUESTS_H_
