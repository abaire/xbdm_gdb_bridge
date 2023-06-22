#include "ResolveExportList.h"

namespace DynDXTLoader {

ResolveExportList::ResolveExportList(
    const std::map<uint32_t, std::vector<ResolveRequest>> &request)
    : RDCPProcessedRequest("ldxt!r") {
  SetData("");
  uint32_t size = 0;
  for (const auto &block : request) {
    AppendData(" b=");
    AppendHexString(block.first);

    for (const auto &request_struct : block.second) {
      AppendData(" o=");
      AppendHexString(request_struct.ordinal);
      out_vector_.emplace_back(request_struct.out);

      // Each entry will return a 32-bit address.
      size += 4;
    }
  }

  binary_response_size_parser_ = [size](uint8_t const *buffer,
                                        uint32_t buffer_size, long &binary_size,
                                        uint32_t &bytes_consumed) {
    binary_size = size;
    bytes_consumed = 0;
    return true;
  };
}

void ResolveExportList::ProcessResponse(
    const std::shared_ptr<RDCPResponse> &response) {
  if (!IsOK()) {
    return;
  }

  auto data = reinterpret_cast<const uint32_t *>(response->Data().data());
  for (const auto &out : out_vector_) {
    out->real_address = *data++;
  }
}

}  // namespace DynDXTLoader
