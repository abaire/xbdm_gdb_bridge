#include "rdcp_request.h"

static constexpr uint8_t kTerminator[] = {'\r', '\n'};
static constexpr long kTerminatorLen =
    sizeof(kTerminator) / sizeof(kTerminator[0]);

RDCPRequest::operator std::vector<uint8_t>() const {
  std::vector<uint8_t> ret(command_.size() + data_.size() + kTerminatorLen);
  ret.assign(command_.begin(), command_.end());
  ret.insert(ret.end(), data_.begin(), data_.end());
  ret.insert(ret.end(), kTerminator, kTerminator + kTerminatorLen);
  return ret;
}

std::ostream &operator<<(std::ostream &os, RDCPRequest const &r) {
  return os << r.command_ << " " << r.data_.size();
}
