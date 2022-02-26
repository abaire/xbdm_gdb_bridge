#include "handler_requests.h"

#include <iostream>

void HandlerInvokeMultiline::ProcessResponse(
    const std::shared_ptr<RDCPResponse>& response) {
  if (!IsOK()) {
    return;
  }

  auto parsed = RDCPMultilineResponse(response->Data());
  for (auto& line : parsed.lines) {
    std::string as_string(line.begin(), line.end());
    std::cout << as_string << std::endl;
  }
}

HandlerDDXTLoad::HandlerDDXTLoad(std::vector<uint8_t> dll_image)
    : RDCPProcessedRequest("ddxt!load"), binary_payload(std::move(dll_image)) {
  SetData(" size=");
  assert(binary_payload.size() < 0xFFFFFFFF);
  uint32_t size = binary_payload.size();
  AppendHexString(size);
}

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

HandlerDDXTInstall::HandlerDDXTInstall(
    uint32_t image_base, std::vector<uint8_t> buffer,
    const std::vector<uint32_t>& tls_callbacks, uint32_t entrypoint)
    : RDCPProcessedRequest("ddxt!install"), binary_payload(std::move(buffer)) {
  SetData(" base=");
  AppendHexString(image_base);
  AppendData(" length=");
  AppendHexString(static_cast<uint32_t>(binary_payload.size()));
  AppendData(" entrypoint=");
  AppendHexString(entrypoint);

  // TODO: IMPLEMENT ME.
  assert(tls_callbacks.empty() && "TLS Callback support not implemented.");
}

HandlerDDXTExport::HandlerDDXTExport(const std::string& module_name,
                                     uint32_t ordinal, uint32_t address,
                                     const std::string& export_name)
    : RDCPProcessedRequest("ddxt!export") {
  SetData(" module=\"");
  AppendData(module_name);
  AppendData("\" ordinal=");
  AppendHexString(ordinal);
  AppendData(" addr=");
  AppendHexString(address);

  if (!export_name.empty()) {
    AppendData(" name=\"");
    AppendData(export_name);
    AppendData("\"");
  }
}
