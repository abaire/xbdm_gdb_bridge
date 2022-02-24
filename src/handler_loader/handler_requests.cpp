#include "handler_requests.h"

HandlerDDXTReserve::HandlerDDXTReserve(uint32_t image_size)
    : RDCPProcessedRequest("ddxt!reserve") {
  SetData(" size=");
  AppendHexString(image_size);
}

void HandlerDDXTReserve::ProcessResponse(
    const std::shared_ptr<RDCPResponse>& response) {
  if (!IsOK()) {
    return;
  }
  auto parsed = RDCPMapResponse(response->Data());
  allocated_address = parsed.GetDWORD("addr");
}

HandlerDDXTLoad::HandlerDDXTLoad(uint32_t image_base,
                                 std::vector<uint8_t> buffer)
    : RDCPProcessedRequest("ddxt!install"), binary_payload(std::move(buffer)) {
  SetData(" base=");
  AppendHexString(image_base);
  AppendData(" length=");
  AppendHexString(static_cast<uint32_t>(binary_payload.size()));
}
