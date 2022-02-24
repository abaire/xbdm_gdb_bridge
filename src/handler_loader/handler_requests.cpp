#include "handler_requests.h"

HandlerBL2Reserve::HandlerBL2Reserve(uint32_t image_size)
    : RDCPProcessedRequest("bl2!reserve") {
  SetData(" size=");
  AppendHexString(image_size);
}

void HandlerBL2Reserve::ProcessResponse(
    const std::shared_ptr<RDCPResponse>& response) {
  if (!IsOK()) {
    return;
  }
  auto parsed = RDCPMapResponse(response->Data());
  allocated_address = parsed.GetDWORD("address");
}

HandlerBL2Load::HandlerBL2Load(uint32_t image_base, std::vector<uint8_t> buffer)
    : RDCPProcessedRequest("bl2!install"), binary_payload(std::move(buffer)) {
  SetData(" base=");
  AppendHexString(image_base);
  AppendData(" length=");
  AppendHexString(static_cast<uint32_t>(binary_payload.size()));
}
