#include "rdcp_processed_request.h"

#include <chrono>

#include "util/parsing.h"

std::ostream& operator<<(std::ostream& os, RDCPProcessedRequest const& r) {
  return os << r.command_ << ": " << r.status << " " << r.message;
}

void RDCPProcessedRequest::Complete(
    const std::shared_ptr<RDCPResponse>& response) {
  status = response->Status();
  message = response->Message();

  ProcessResponse(response);

  completed_.notify_all();
}

void RDCPProcessedRequest::Abandon() {
  status = StatusCode::ERR_ABANDONED;
  completed_.notify_all();
}

void RDCPProcessedRequest::WaitUntilCompleted() {
  std::unique_lock<std::mutex> lock(mutex_);
  completed_.wait(lock);
}

bool RDCPProcessedRequest::WaitUntilCompleted(int max_wait_milliseconds) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto result = completed_.wait_for(
      lock, std::chrono::milliseconds(max_wait_milliseconds));
  return result == std::cv_status::no_timeout;
}
