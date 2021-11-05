#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_THREAD_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_THREAD_H_

#include <cinttypes>
#include <memory>
#include <optional>

#include "rdcp/rdcp_response_processors.h"
#include "rdcp/types/thread_context.h"
#include "rdcp/xbdm_stop_reasons.h"

class XBDMContext;

struct Thread {
  static constexpr uint32_t kTrapFlag = 0x100;

  explicit Thread(int thread_id) : thread_id(thread_id) {}

  bool FetchInfoSync(XBDMContext &ctx);
  bool FetchContextSync(XBDMContext &ctx);
  bool FetchFloatContextSync(XBDMContext &ctx);
  bool FetchStopReasonSync(XBDMContext &ctx);

  bool Halt(XBDMContext &ctx);
  bool Continue(XBDMContext &ctx, bool break_on_exceptions = true);
  bool Suspend(XBDMContext &ctx);
  bool Resume(XBDMContext &ctx);

  bool StepInstruction(XBDMContext &ctx);

 private:
  void Parse(const RDCPMapResponse &parsed) {
    suspend_count = parsed.GetDWORD("suspend");
    priority = parsed.GetDWORD("priority");
    tls_base = parsed.GetDWORD("tlsbase");
    start = parsed.GetDWORD("start");
    base = parsed.GetDWORD("base");
    limit = parsed.GetDWORD("limit");
    create_timestamp = parsed.GetQWORD("createlo", "createhi");
  }

  friend std::ostream &operator<<(std::ostream &os, const Thread &t);

 public:
  int thread_id;
  std::optional<int32_t> suspend_count;
  std::optional<int32_t> priority;
  std::optional<uint32_t> tls_base;
  std::optional<uint32_t> start;
  std::optional<uint32_t> base;
  std::optional<uint32_t> limit;
  std::optional<uint64_t> create_timestamp;

  std::optional<ThreadContext> context;
  std::optional<ThreadFloatContext> float_context;

  std::optional<uint32_t> last_known_address;
  bool stopped{false};
  std::shared_ptr<StopReasonBase_> last_stop_reason;
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_THREAD_H_
