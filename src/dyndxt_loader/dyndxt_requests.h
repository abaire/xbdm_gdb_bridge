#ifndef XBDM_GDB_BRIDGE_SRC_HANDLER_LOADER_HANDLER_REQUESTS_H_
#define XBDM_GDB_BRIDGE_SRC_HANDLER_LOADER_HANDLER_REQUESTS_H_

#include <boost/optional/optional_io.hpp>
#include <string>

#include "net/ip_address.h"
#include "rdcp/rdcp_processed_request.h"
#include "rdcp/rdcp_request.h"
#include "rdcp/rdcp_response.h"
#include "rdcp/rdcp_status_code.h"
#include "util/parsing.h"

namespace DynDXTLoader {

struct InvokeSimple : public RDCPProcessedRequest {
  explicit InvokeSimple(const std::string& command,
                        const std::string& args = "")
      : RDCPProcessedRequest(command) {
    if (!args.empty()) {
      SetData(" " + args);
    }
  }
};

struct InvokeMultiline : public RDCPProcessedRequest {
  explicit InvokeMultiline(const std::string& command,
                           const std::string& args = "")
      : RDCPProcessedRequest(command) {
    if (!args.empty()) {
      SetData(" " + args);
    }
  }

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_MULTILINE_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override;
};

struct InvokeSendBinary : public RDCPProcessedRequest {
  explicit InvokeSendBinary(const std::string& command,
                            std::vector<uint8_t> binary,
                            const std::string& args = "");

  [[nodiscard]] const std::vector<uint8_t>* BinaryPayload() override {
    return &binary_payload;
  }

  std::vector<uint8_t> binary_payload;
};

struct InvokeSendKnownSizeBinary : public RDCPProcessedRequest {
  explicit InvokeSendKnownSizeBinary(const std::string& command,
                                     std::vector<uint8_t> binary,
                                     const std::string& args = "");

  [[nodiscard]] const std::vector<uint8_t>* BinaryPayload() override {
    return &binary_payload;
  }

  std::vector<uint8_t> binary_payload;
};

struct InvokeReceiveSizePrefixedBinary : public RDCPProcessedRequest {
  explicit InvokeReceiveSizePrefixedBinary(const std::string& command,
                                           const std::string& args = "");

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_BINARY_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override;

  std::vector<uint8_t> response_data;
};

struct InvokeReceiveKnownSizedBinary : public RDCPProcessedRequest {
  explicit InvokeReceiveKnownSizedBinary(const std::string& command,
                                         uint32_t size,
                                         const std::string& args = "");

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_BINARY_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override;

  std::vector<uint8_t> response_data;
};

// Load the given DynDXT image, performing relocation on device.
struct LoadDynDXT : public RDCPProcessedRequest {
  explicit LoadDynDXT(std::vector<uint8_t> dll_image);

  [[nodiscard]] const std::vector<uint8_t>* BinaryPayload() override {
    return &binary_payload;
  }

  std::vector<uint8_t> binary_payload;
};

//! Reserve memory in the debug region.
struct Reserve : public RDCPProcessedRequest {
  explicit Reserve(uint32_t image_size);
  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override;

  uint32_t allocated_address{0};
};

// Install a pre-relocated DynDXT image.
struct InstallImage : public RDCPProcessedRequest {
  InstallImage(uint32_t image_base, std::vector<uint8_t> buffer,
               const std::vector<uint32_t>& tls_callbacks, uint32_t entrypoint);

  [[nodiscard]] const std::vector<uint8_t>* BinaryPayload() override {
    return &binary_payload;
  }

  std::vector<uint8_t> binary_payload;
};

// Register a function exported by the given DLL module.
struct RegisterExport : public RDCPProcessedRequest {
  RegisterExport(const std::string& module_name, uint32_t ordinal,
                 uint32_t address, const std::string& export_name = "");
};

}  // namespace DynDXTLoader

#endif  // XBDM_GDB_BRIDGE_SRC_HANDLER_LOADER_HANDLER_REQUESTS_H_
