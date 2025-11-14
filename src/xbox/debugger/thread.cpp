#include "thread.h"

#include "rdcp/xbdm_requests.h"
#include "util/logging.h"
#include "xbox/xbdm_context.h"

// x86 single step flag.
static constexpr uint32_t TRAP_FLAG = 0x100;

std::ostream& operator<<(std::ostream& os, const Thread& t) {
  os << "Thread " << std::dec << t.thread_id << std::endl;

  auto print_decimal = [&os](const char* prefix,
                             const std::optional<int32_t>& val) {
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

  auto print_hex = [&os](const char* prefix,
                         const std::optional<uint32_t>& val) {
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

  os << std::dec << std::setfill(' ');
  return os;
}

bool Thread::FetchInfoSync(XBDMContext& ctx) {
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

bool Thread::FetchContextSync(XBDMContext& ctx) {
  auto request = std::make_shared<GetContext>(thread_id, true, true, true);
  ctx.SendCommandSync(request);
  if (!request->IsOK()) {
    context.reset();
    return false;
  }

  context = request->context;
  return true;
}

bool Thread::PushContextSync(XBDMContext& ctx) {
  if (!context.has_value()) {
    return false;
  }
  auto request = std::make_shared<::SetContext>(thread_id, context.value());
  ctx.SendCommandSync(request);
  return request->IsOK();
}

bool Thread::FetchFloatContextSync(XBDMContext& ctx) {
  auto request = std::make_shared<GetExtContext>(thread_id);
  ctx.SendCommandSync(request);
  if (!request->IsOK()) {
    float_context.reset();
    return false;
  }
  float_context = request->context;
  return true;
}

bool Thread::PushFloatContextSync(XBDMContext& ctx) {
  if (!float_context.has_value()) {
    return false;
  }
  auto request =
      std::make_shared<::SetContext>(thread_id, float_context.value());
  ctx.SendCommandSync(request);
  return request->IsOK();
}

bool Thread::FetchStopReasonSync(XBDMContext& ctx) {
  auto request = std::make_shared<IsStopped>(thread_id);
  ctx.SendCommandSync(request);
  if (!request->IsOK()) {
    last_stop_reason.reset();
    return false;
  }

  stopped = request->stopped;
  last_stop_reason = request->stop_reason;
  return true;
}

bool Thread::Halt(XBDMContext& ctx) {
  auto request = std::make_shared<::Halt>(thread_id);
  ctx.SendCommandSync(request);
  return request->IsOK();
}

bool Thread::Continue(XBDMContext& ctx, bool break_on_exceptions) {
  auto request = std::make_shared<::Continue>(thread_id);
  ctx.SendCommandSync(request);
  return request->IsOK();
}

bool Thread::Suspend(XBDMContext& ctx) {
  auto request = std::make_shared<::Suspend>(thread_id);
  ctx.SendCommandSync(request);
  return request->IsOK();
}

bool Thread::Resume(XBDMContext& ctx) {
  auto request = std::make_shared<::Resume>(thread_id);
  ctx.SendCommandSync(request);
  return request->IsOK();
}

bool Thread::StepInstruction(XBDMContext& ctx) {
  if (!FetchContextSync(ctx)) {
    LOG_DEBUGGER(error) << "Failed to fetch context in StepInstruction.";
    return false;
  }

  auto flags = context->eflags;
  if (!flags.has_value()) {
    LOG_DEBUGGER(error) << "Context does not contain eflags in StepInstruction";
    return false;
  }

  uint32_t new_flags = flags.value() | TRAP_FLAG;
  if (new_flags == flags) {
    return true;
  }

  {
    ThreadContext new_context(context.value());
    new_context.eflags = new_flags;
    auto request = std::make_shared<::SetContext>(thread_id, new_context);
    ctx.SendCommandSync(request);
    if (!request->IsOK()) {
      LOG_DEBUGGER(error) << "Failed to set trap flag in StepInstruction";
      return false;
    }
  }

  return Continue(ctx);
}
