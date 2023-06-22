#ifndef XBDM_GDB_BRIDGE_SRC_DYNDXT_LOADER_RESOLVEEXPORTLIST_H_
#define XBDM_GDB_BRIDGE_SRC_DYNDXT_LOADER_RESOLVEEXPORTLIST_H_

#include <map>
#include <vector>

#include "dxt_library.h"
#include "rdcp/rdcp_processed_request.h"

namespace DynDXTLoader {

struct ResolveExportList : public RDCPProcessedRequest {
  struct ResolveRequest {
    ResolveRequest(uint32_t ordinal, DXTLibraryImport *out)
        : ordinal(ordinal), out(out) {}
    uint32_t ordinal;
    DXTLibraryImport *out;
  };

  explicit ResolveExportList(
      const std::map<uint32_t, std::vector<ResolveRequest>> &);

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_BINARY_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse> &response) override;

 private:
  std::vector<DXTLibraryImport *> out_vector_;
};

}  // namespace DynDXTLoader

#endif  // XBDM_GDB_BRIDGE_SRC_DYNDXT_LOADER_RESOLVEEXPORTLIST_H_
