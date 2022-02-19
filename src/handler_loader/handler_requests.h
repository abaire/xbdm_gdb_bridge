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

// The maximum size of an RDCP command string.
#define MAXIMUM_SEND_LENGTH 512

struct HandlerRequest : public RDCPProcessedRequest {
  explicit HandlerRequest(const std::string &command)
      : RDCPProcessedRequest("bl2!" + command) {}
};

struct HandlerHello : public HandlerRequest {
  HandlerHello() : HandlerRequest("hello") {}
};

#endif  // XBDM_GDB_BRIDGE_SRC_HANDLER_LOADER_HANDLER_REQUESTS_H_
