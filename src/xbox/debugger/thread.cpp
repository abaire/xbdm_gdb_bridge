#include "thread.h"

#include "rdcp/xbdm_requests.h"
#include "xbox/xbdm_context.h"

std::ostream &operator<<(std::ostream &os, const Thread &t) {
  os << "Thread " << t.thread_id << std::endl;

  auto print_decimal = [&os](const char *prefix,
                             const std::optional<int32_t> &val) {
    os << prefix << " ";
    if (val.has_value()) {
      os << val.value();
    } else {
      os << "???";
    }
    os << std::endl;
  };

  print_decimal("Priority", t.priority);
  print_decimal("Suspend count", t.suspend_count);

  auto print_hex = [&os](const char *prefix,
                         const std::optional<uint32_t> &val) {
    os << prefix << " ";
    if (val.has_value()) {
      os << "0x" << std::hex << std::setw(8) << std::setfill('0') << *val;
    } else {
      os << "???";
    }
    os << std::endl;
  };

  print_hex("Base: ", t.base);
  print_hex("Start: ", t.start);
  print_hex("Thread local base: ", t.tls_base);
  print_hex("Limit: ", t.limit);

  return os;
}

bool Thread::FetchInfoSync(XBDMContext &ctx) {
  auto request = std::make_shared<ThreadInfo>(thread_id);
  ctx.SendCommandSync(request);
  if (!request->IsOK()) {
    suspend_count.reset();
    priority.reset();
    tls_base.reset();
    start.reset();
    base.reset();
    limit.reset();
    create_timestamp.reset();
    return false;
  }
  suspend_count = request->suspend_count;
  priority = request->priority;
  tls_base = request->tls_base;
  start = request->start;
  base = request->base;
  limit = request->limit;
  create_timestamp = request->create_timestamp;
  return true;
}

bool Thread::FetchContextSync(XBDMContext &ctx) {
  auto request = std::make_shared<GetContext>(thread_id);
  ctx.SendCommandSync(request);
  if (!request->IsOK()) {
    context.reset();
    return false;
  }

  context = request->context;
  return true;
}

bool Thread::FetchFloatContextSync(XBDMContext &ctx) {
  auto request = std::make_shared<GetExtContext>(thread_id);
  ctx.SendCommandSync(request);
  if (!request->IsOK()) {
    float_context.reset();
    return false;
  }
  float_context = request->context;
  return true;
}

bool Thread::FetchStopReasonSync(XBDMContext &ctx) {
  auto request = std::make_shared<IsStopped>(thread_id);
  ctx.SendCommandSync(request);
  if (!request->IsOK()) {
    last_stop_reason.reset();
    return false;
  }

  last_stop_reason = request->stop_reason;
  return true;
}

bool Thread::Halt(XBDMContext &ctx) {
  auto request = std::make_shared<::Halt>(thread_id);
  ctx.SendCommandSync(request);
  return request->IsOK();
}

bool Thread::Continue(XBDMContext &ctx, bool break_on_exceptions) {
  auto request = std::make_shared<::Continue>(thread_id);
  ctx.SendCommandSync(request);
  return request->IsOK();
}

bool Thread::Suspend(XBDMContext &ctx) {
  auto request = std::make_shared<::Suspend>(thread_id);
  ctx.SendCommandSync(request);
  return request->IsOK();
}

bool Thread::Resume(XBDMContext &ctx) {
  auto request = std::make_shared<::Resume>(thread_id);
  ctx.SendCommandSync(request);
  return request->IsOK();
}
