#ifndef XBDM_GDB_BRIDGE_RDCP_PROCESSED_REQUEST_H
#define XBDM_GDB_BRIDGE_RDCP_PROCESSED_REQUEST_H

#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "rdcp_request.h"
#include "rdcp_response.h"
#include "rdcp_response_processors.h"
#include "rdcp_status_code.h"

struct RDCPProcessedRequest : public RDCPRequest {
 public:
  explicit RDCPProcessedRequest(std::string command)
      : RDCPRequest(std::move(command)) {}
  RDCPProcessedRequest(std::string command, std::vector<uint8_t> data)
      : RDCPRequest(std::move(command), std::move(data)) {}

  [[nodiscard]] virtual bool IsOK() const { return status == StatusCode::OK; }

  void Complete(const std::shared_ptr<RDCPResponse>& response) final;

  void Abandon() final;

  void WaitUntilCompleted();
  bool WaitUntilCompleted(int max_wait_milliseconds);

  virtual void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) {}

  friend std::ostream& operator<<(std::ostream&, RDCPProcessedRequest const&);

 public:
  StatusCode status{INVALID};
  std::string message;

 protected:
  std::mutex mutex_;
  std::condition_variable completed_;
};

#endif  // XBDM_GDB_BRIDGE_RDCP_PROCESSED_REQUEST_H
