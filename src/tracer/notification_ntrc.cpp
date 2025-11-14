#include "notification_ntrc.h"

#include "ntrc_dyndxt.h"
#include "rdcp/rdcp_response_processors.h"

namespace NTRCTracer {

NotificationNTRC::NotificationNTRC(const char* buffer_start,
                                   const char* buffer_end)
    : content(buffer_start, buffer_end) {}

std::string NotificationNTRC::NotificationPrefix() const {
  return NTRC_HANDLER_NAME;
}

std::ostream& NotificationNTRC::WriteStream(std::ostream& os) const {
  os << "NTRC: " << content;
  return os;
}

}  // namespace NTRCTracer
