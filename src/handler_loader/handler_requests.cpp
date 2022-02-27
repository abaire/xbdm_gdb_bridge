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

HandlerInvokeSendBinary::HandlerInvokeSendBinary(const std::string& command,
                                                 std::vector<uint8_t> binary,
                                                 const std::string& args)
    : RDCPProcessedRequest(command), binary_payload(std::move(binary)) {
  SetData("length=");
  AppendHexString(static_cast<uint32_t>(binary.size()));

  if (!args.empty()) {
    AppendData(" ");
    AppendData(args);
  }
}

HandlerInvokeReceiveSizePrefixedBinary::HandlerInvokeReceiveSizePrefixedBinary(
    const std::string& command, const std::string& args)
    : RDCPProcessedRequest(command) {
  if (!args.empty()) {
    SetData(args);
  }

  binary_response_size_parser_ = [](uint8_t const* buffer, uint32_t buffer_size,
                                    long& binary_size,
                                    uint32_t& bytes_consumed) {
    if (buffer_size < 4) {
      return false;
    }

    binary_size = *reinterpret_cast<uint32_t const*>(buffer);
    bytes_consumed = 4;
    return true;
  };
}

void HandlerInvokeReceiveSizePrefixedBinary::ProcessResponse(
    const std::shared_ptr<RDCPResponse>& response) {
  if (!IsOK()) {
    return;
  }

  const auto& data = response->Data();
  response_data.insert(response_data.begin(), data.begin(), data.end());
}

HandlerInvokeReceiveKnownSizedBinary::HandlerInvokeReceiveKnownSizedBinary(
    const std::string& command, uint32_t size, const std::string& args)
    : RDCPProcessedRequest(command) {
  if (!args.empty()) {
    AppendData(args);
  }

  binary_response_size_parser_ = [size](uint8_t const* buffer,
                                        uint32_t buffer_size, long& binary_size,
                                        uint32_t& bytes_consumed) {
    binary_size = size;
    bytes_consumed = 0;
    return true;
  };
}

void HandlerInvokeReceiveKnownSizedBinary::ProcessResponse(
    const std::shared_ptr<RDCPResponse>& response) {
  if (!IsOK()) {
    return;
  }

  const auto& data = response->Data();
  response_data.insert(response_data.begin(), data.begin(), data.end());
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
