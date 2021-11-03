#ifndef XBDM_GDB_BRIDGE_RDCP_PROCESSED_REQUEST_H
#define XBDM_GDB_BRIDGE_RDCP_PROCESSED_REQUEST_H

#include <boost/optional.hpp>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "rdcp_request.h"
#include "rdcp_response.h"
#include "rdcp_status_code.h"

struct RDCPProcessedRequest : public RDCPRequest {
 public:
  explicit RDCPProcessedRequest(std::string command)
      : RDCPRequest(std::move(command)) {}
  RDCPProcessedRequest(std::string command, std::vector<uint8_t> data)
      : RDCPRequest(std::move(command), std::move(data)) {}

  [[nodiscard]] virtual bool IsOK() const { return status == StatusCode::OK; }

  void Complete(const std::shared_ptr<RDCPResponse> &response) final {
    status = response->Status();
    message = response->Message();

    ProcessResponse(response);

    completed_.notify_all();
  }

  void Abandon() final {
    status = StatusCode::ERR_ABANDONED;
    completed_.notify_all();
  }

  virtual void ProcessResponse(const std::shared_ptr<RDCPResponse> &response) = 0;

  void WaitUntilCompleted() {
    std::unique_lock<std::mutex> lock(mutex_);
    completed_.wait(lock);
  }

  bool WaitUntilCompleted(int max_wait_milliseconds) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto result = completed_.wait_for(lock, std::chrono::milliseconds(max_wait_milliseconds));
    return result == std::cv_status::no_timeout;
  }
 public:
  StatusCode status{INVALID};
  std::string message;

 protected:
  std::mutex mutex_;
  std::condition_variable completed_;
};

struct RDCPMultilineResponse {
  explicit RDCPMultilineResponse(const std::vector<char> &data);

  std::vector<std::vector<char>> lines;
};

struct RDCPMapResponse {
  explicit RDCPMapResponse(const std::vector<char> &data);

  [[nodiscard]] bool HasKey(const std::string &key) const;

  [[nodiscard]] std::string GetString(const std::string &key) const {
    return GetString(key, "");
  }
  [[nodiscard]] std::string GetString(const std::string &key,
                                      const std::string &default_value) const;

  [[nodiscard]] boost::optional<int32_t> GetOptionalDWORD(
      const std::string &key) const;

  [[nodiscard]] int32_t GetDWORD(const std::string &key) const {
    return GetDWORD(key, 0);
  }
  [[nodiscard]] int32_t GetDWORD(const std::string &key,
                                 int32_t default_value) const;

  [[nodiscard]] uint32_t GetUInt32(const std::string &key) const {
    return GetUInt32(key, 0);
  }
  [[nodiscard]] uint32_t GetUInt32(const std::string &key,
                                 uint32_t default_value) const;

  [[nodiscard]] int64_t GetQWORD(const std::string &low_key,
                                 const std::string &high_key) const {
    return GetQWORD(low_key, high_key, 0);
  }
  [[nodiscard]] int64_t GetQWORD(const std::string &low_key,
                                 const std::string &high_key,
                                 int64_t default_value) const;

  static int32_t ParseInteger(const std::vector<uint8_t> &value);
  static int32_t ParseInteger(const std::vector<char> &data);
  static int32_t ParseInteger(const std::string &value);

  std::map<std::string, std::string> map;
  std::set<std::string> valueless_keys;
};

struct RDCPMultiMapResponse {
  explicit RDCPMultiMapResponse(const std::vector<char> &data);

  std::vector<RDCPMapResponse> maps;
};

#endif  // XBDM_GDB_BRIDGE_RDCP_PROCESSED_REQUEST_H
