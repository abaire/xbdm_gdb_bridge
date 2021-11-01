#include "rdcp_response.h"

#include <ostream>

static constexpr uint8_t kTerminator[] = {'\r', '\n'};
static constexpr uint8_t kMultilineTerminator[] = {'\r', '\n', '.', '\r', '\n'};

std::ostream &operator<<(std::ostream &os, RDCPResponse const &r) {
  return os << "RDCPResponse [" << r.status_ << "]";
}
