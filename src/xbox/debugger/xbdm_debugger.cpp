#include "xbdm_debugger.h"

#include <boost/log/trivial.hpp>

#include "notification/xbdm_notification.h"
#include "util/path.h"
#include "util/timer.h"
#include "xbox/xbdm_context.h"

static constexpr uint32_t kRestartRebootingMaxWaitMilliseconds = 5 * 1000;
static constexpr uint32_t kRestartPendingMaxWaitMilliseconds = 15 * 1000;
static constexpr uint32_t kRestartStoppedMaxAtStartWaitMilliseconds = 1 * 1000;

XBDMDebugger::XBDMDebugger(std::shared_ptr<XBDMContext> context)
    : context_(std::move(context)) {}

bool XBDMDebugger::Attach() {
  IPAddress address;
  if (!context_->GetNotificationServerAddress(address)) {
    if (!context_->StartNotificationListener(address)) {
      return false;
    }
    bool is_started = context_->GetNotificationServerAddress(address);
    if (!is_started) {
      assert(!"Failed to start notification server.");
      return false;
    }
  }

  context_->UnregisterNotificationHandler(notification_handler_id_);
  notification_handler_id_ = context_->RegisterNotificationHandler(
      [this](const std::shared_ptr<XBDMNotification> &notification) {
        this->OnNotification(notification);
      });

  {
    auto request = std::make_shared<NotifyAt>(address.Port(), false, true);
    context_->SendCommandSync(request);
    if (!request->IsOK()) {
      BOOST_LOG_TRIVIAL(error) << "Failed to request notifications "
                               << request->status << " " << request->message;
      context_->UnregisterNotificationHandler(notification_handler_id_);
      return false;
    }
  }

  if (!SetDebugger(true)) {
    context_->UnregisterNotificationHandler(notification_handler_id_);
    return false;
  }

  if (!FetchThreads()) {
    return false;
  }

  return FetchModules();
}

void XBDMDebugger::Shutdown() {
  SetDebugger(false);
  // TODO: Request a notifyat drop as well.
  context_->UnregisterNotificationHandler(notification_handler_id_);
  notification_handler_id_ = 0;
}

bool XBDMDebugger::DebugXBE(const std::string &path, bool wait_forever) {
  return DebugXBE(path, "", wait_forever);
}

bool XBDMDebugger::DebugXBE(const std::string &path,
                            const std::string &command_line,
                            bool wait_forever) {
  std::string xbe_dir;
  std::string xbe_name;
  if (!SplitXBEPath(path, xbe_dir, xbe_name)) {
    BOOST_LOG_TRIVIAL(error) << "Invalid XBE path '" << path << "'";
    return false;
  }

  uint32_t flags = Reboot::kWait | Reboot::kWarm;
  if (wait_forever) {
    flags |= Reboot::kStop;
  }
  if (!RestartAndReconnect(flags)) {
    BOOST_LOG_TRIVIAL(error) << "Failed to restart.";
    return false;
  }

  {
    auto request =
        std::make_shared<LoadOnBootTitle>(xbe_name, xbe_dir, command_line);
    context_->SendCommandSync(request);
    if (!request->IsOK()) {
      BOOST_LOG_TRIVIAL(error) << "Failed to set load on boot title "
                               << request->status << " " << request->message;
      return false;
    }
  }
  if (!BreakAtStart()) {
    return false;
  }

  if (!Go()) {
    return false;
  }

  if (!WaitForState(S_STOPPED, kRestartStoppedMaxAtStartWaitMilliseconds)) {
    BOOST_LOG_TRIVIAL(warning) << "Timed out waiting for pending message.";
  }

  if (!FetchThreads()) {
    BOOST_LOG_TRIVIAL(warning)
        << "Failed to fetch threads while at start breakpoint.";
  }
  return ContinueAll();
}

std::list<std::shared_ptr<Thread>> XBDMDebugger::Threads() {
  std::unique_lock<std::recursive_mutex> lock(threads_lock_);
  return threads_;
}

std::list<std::shared_ptr<Module>> XBDMDebugger::Modules() {
  std::unique_lock<std::recursive_mutex> lock(modules_lock_);
  return modules_;
}
std::list<std::shared_ptr<Section>> XBDMDebugger::Sections() {
  std::unique_lock<std::recursive_mutex> lock(sections_lock_);
  return sections_;
}

std::vector<int32_t> XBDMDebugger::GetThreadIDs() {
  std::unique_lock<std::recursive_mutex> lock(threads_lock_);
  std::vector<int32_t> ret;
  ret.reserve(threads_.size());

  int32_t active_thread_id = ActiveThreadID();
  ret.push_back(active_thread_id);

  for (auto &thread : threads_) {
    if (thread->thread_id == active_thread_id) {
      continue;
    }
    ret.push_back(thread->thread_id);
  }
  return std::move(ret);
}

std::shared_ptr<Thread> XBDMDebugger::GetAnyThread() {
  return GetThread(AnyThreadID());
}

std::shared_ptr<Thread> XBDMDebugger::GetThread(int thread_id) {
  const std::lock_guard<std::recursive_mutex> lock(threads_lock_);
  for (auto &it : threads_) {
    if (it->thread_id == thread_id) {
      return it;
    }
  }
  return {};
}

bool XBDMDebugger::SetActiveThread(int thread_id) {
  if (GetThread(thread_id)) {
    active_thread_index_ = thread_id;
    return true;
  }
  return false;
}

void XBDMDebugger::OnNotification(
    const std::shared_ptr<XBDMNotification> &notification) {
  switch (notification->Type()) {
    case NT_VX:
      OnVX(std::dynamic_pointer_cast<NotificationVX>(notification));
      break;

    case NT_DEBUGSTR:
      OnDebugStr(std::dynamic_pointer_cast<NotificationDebugStr>(notification));
      break;

    case NT_MODULE_LOADED:
      OnModuleLoaded(
          std::dynamic_pointer_cast<NotificationModuleLoaded>(notification));
      break;

    case NT_SECTION_LOADED:
      OnSectionLoaded(
          std::dynamic_pointer_cast<NotificationSectionLoaded>(notification));
      break;

    case NT_SECTION_UNLOADED:
      OnSectionUnloaded(
          std::dynamic_pointer_cast<NotificationSectionUnloaded>(notification));
      break;

    case NT_THREAD_CREATED:
      OnThreadCreated(
          std::dynamic_pointer_cast<NotificationThreadCreated>(notification));
      break;

    case NT_THREAD_TERMINATED:
      OnThreadTerminated(
          std::dynamic_pointer_cast<NotificationThreadTerminated>(
              notification));
      break;

    case NT_EXECUTION_STATE_CHANGED:
      OnExecutionStateChanged(
          std::dynamic_pointer_cast<NotificationExecutionStateChanged>(
              notification));
      break;

    case NT_BREAKPOINT:
      OnBreakpoint(
          std::dynamic_pointer_cast<NotificationBreakpoint>(notification));
      break;

    case NT_WATCHPOINT:
      OnWatchpoint(
          std::dynamic_pointer_cast<NotificationWatchpoint>(notification));
      break;

    case NT_SINGLE_STEP:
      OnSingleStep(
          std::dynamic_pointer_cast<NotificationSingleStep>(notification));
      break;

    case NT_EXCEPTION:
      OnException(
          std::dynamic_pointer_cast<NotificationException>(notification));
      break;

    case NT_INVALID:
      BOOST_LOG_TRIVIAL(error) << "Received invalid notification type.";
      break;
  }
}

void XBDMDebugger::OnVX(const std::shared_ptr<NotificationVX> &msg) {
  BOOST_LOG_TRIVIAL(info) << "VX notification: " << std::endl << *msg;
}

void XBDMDebugger::OnDebugStr(
    const std::shared_ptr<NotificationDebugStr> &msg) {
  if (!msg->is_terminated) {
    auto existing = debugstr_accumulator_.find(msg->thread_id);
    if (existing != debugstr_accumulator_.end()) {
      debugstr_accumulator_[msg->thread_id] += msg->text;
    } else {
      debugstr_accumulator_[msg->thread_id] = msg->text;
    }
    return;
  }

  auto existing = debugstr_accumulator_.find(msg->thread_id);
  if (existing != debugstr_accumulator_.end()) {
    BOOST_LOG_TRIVIAL(info) << std::endl << existing->second << *msg;
    ;
    debugstr_accumulator_.erase(existing);
  }

  BOOST_LOG_TRIVIAL(info) << std::endl << *msg;
}

void XBDMDebugger::OnModuleLoaded(
    const std::shared_ptr<NotificationModuleLoaded> &msg) {
  std::unique_lock<std::recursive_mutex> lock(modules_lock_);
  modules_.push_back(std::make_shared<Module>(msg->module));
}

void XBDMDebugger::OnSectionLoaded(
    const std::shared_ptr<NotificationSectionLoaded> &msg) {
  std::unique_lock<std::recursive_mutex> lock(sections_lock_);
  sections_.push_back(std::make_shared<Section>(msg->section));
}

void XBDMDebugger::OnSectionUnloaded(
    const std::shared_ptr<NotificationSectionUnloaded> &msg) {
  std::unique_lock<std::recursive_mutex> lock(sections_lock_);
  auto &section = msg->section;
  sections_.remove_if([&section](const std::shared_ptr<Section> &other) {
    if (!other) {
      return false;
    }

    return other->base_address == section.base_address;
  });
}

void XBDMDebugger::OnThreadCreated(
    const std::shared_ptr<NotificationThreadCreated> &msg) {
  const std::lock_guard<std::recursive_mutex> lock(threads_lock_);
  threads_.push_back(std::make_shared<Thread>(msg->thread_id));
}

void XBDMDebugger::OnThreadTerminated(
    const std::shared_ptr<NotificationThreadTerminated> &msg) {
  const std::lock_guard<std::recursive_mutex> lock(threads_lock_);
  for (auto it = threads_.begin(); it != threads_.end(); ++it) {
    auto &thread = *it;
    if (thread->thread_id == msg->thread_id) {
      threads_.erase(it);
      return;
    }
  }

  BOOST_LOG_TRIVIAL(warning)
      << "Received thread termination message for unknown thread "
      << msg->thread_id;
}

void XBDMDebugger::OnExecutionStateChanged(
    const std::shared_ptr<NotificationExecutionStateChanged> &msg) {
  BOOST_LOG_TRIVIAL(info) << "State changed: " << *msg;

  {
    const std::lock_guard<std::mutex> lock(state_lock_);
    state_ = msg->state;
    if (state_ == ExecutionState::S_REBOOTING) {
      modules_.clear();
      sections_.clear();
    }
  }
  state_condition_variable_.notify_all();
}

void XBDMDebugger::OnBreakpoint(
    const std::shared_ptr<NotificationBreakpoint> &msg) {
  auto thread = GetThread(msg->thread_id);
  if (!thread) {
    BOOST_LOG_TRIVIAL(warning)
        << "Received breakpoint message for unknown thread " << msg->thread_id;
    return;
  }

  thread->last_known_address = msg->address;
  thread->FetchStopReasonSync(*context_);
}

void XBDMDebugger::OnWatchpoint(
    const std::shared_ptr<NotificationWatchpoint> &msg) {
  auto thread = GetThread(msg->thread_id);
  if (!thread) {
    BOOST_LOG_TRIVIAL(warning)
        << "Received breakpoint message for unknown thread " << msg->thread_id;
    return;
  }

  thread->last_known_address = msg->address;
}

void XBDMDebugger::OnSingleStep(
    const std::shared_ptr<NotificationSingleStep> &msg) {
  auto thread = GetThread(msg->thread_id);
  if (!thread) {
    BOOST_LOG_TRIVIAL(warning)
        << "Received breakpoint message for unknown thread " << msg->thread_id;
    return;
  }

  thread->last_known_address = msg->address;
}

void XBDMDebugger::OnException(
    const std::shared_ptr<NotificationException> &msg) {
  BOOST_LOG_TRIVIAL(warning) << "Received exception: " << *msg;
  auto thread = GetThread(msg->thread_id);
  if (!thread) {
    BOOST_LOG_TRIVIAL(warning)
        << "Received breakpoint message for unknown thread " << msg->thread_id;
    return;
  }
  thread->last_known_address = msg->address;
}

bool XBDMDebugger::RestartAndAttach(int flags) {
  if (!RestartAndReconnect(flags)) {
    return false;
  }
  if (!BreakAtStart()) {
    return false;
  }

  return Go();
}

bool XBDMDebugger::Stop() const {
  auto request = std::make_shared<::Stop>();
  context_->SendCommandSync(request);
  if (!request->IsOK()) {
    BOOST_LOG_TRIVIAL(error)
        << "'stop' failed: " << request->status << " " << request->message;
    return false;
  }
  return true;
}

bool XBDMDebugger::Go() const {
  auto request = std::make_shared<::Go>();
  context_->SendCommandSync(request);
  if (!request->IsOK()) {
    BOOST_LOG_TRIVIAL(error)
        << "'go' failed: " << request->status << " " << request->message;
    return false;
  }
  return true;
}

bool XBDMDebugger::BreakAtStart() const {
  auto request = std::make_shared<::BreakAtStart>();
  context_->SendCommandSync(request);
  if (!request->IsOK()) {
    BOOST_LOG_TRIVIAL(error) << "Failed to request break at start "
                             << request->status << " " << request->message;
    return false;
  }
  return true;
}

bool XBDMDebugger::SetDebugger(bool enabled) {
  auto request = std::make_shared<Debugger>();
  context_->SendCommandSync(request);
  if (!request->IsOK()) {
    BOOST_LOG_TRIVIAL(error) << "Failed to enable debugger " << request->status
                             << " " << request->message;
    return false;
  }
  target_not_debuggable_ = !request->debuggable;
  return true;
}

bool XBDMDebugger::FetchThreads() {
  auto request = std::make_shared<::Threads>();
  context_->SendCommandSync(request);
  if (!request->IsOK()) {
    BOOST_LOG_TRIVIAL(error) << "Failed to fetch thread list "
                             << request->status << " " << request->message;
    return false;
  }

  const std::lock_guard<std::recursive_mutex> lock(threads_lock_);
  threads_.clear();
  for (auto thread_id : request->threads) {
    threads_.emplace_back(std::make_shared<Thread>(thread_id));
  }

  auto &context = *context_;
  for (auto &thread : threads_) {
    if (!thread->FetchInfoSync(context)) {
      BOOST_LOG_TRIVIAL(error)
          << "Failed to fetch info for thread " << thread->thread_id;
    }
  }

  return true;
}

bool XBDMDebugger::FetchModules() {
  auto request = std::make_shared<::Modules>();
  context_->SendCommandSync(request);
  if (!request->IsOK()) {
    BOOST_LOG_TRIVIAL(error) << "Failed to fetch module list "
                             << request->status << " " << request->message;
    return false;
  }

  const std::lock_guard<std::recursive_mutex> lock(modules_lock_);
  modules_.clear();
  for (auto &module : request->modules) {
    modules_.emplace_back(std::make_shared<Module>(module));
  }

  return true;
}

bool XBDMDebugger::RestartAndReconnect(uint32_t reboot_flags) {
  assert(context_);
  auto request = std::make_shared<::Reboot>(reboot_flags);
  context_->SendCommandSync(request);
  if (!request->IsOK()) {
    BOOST_LOG_TRIVIAL(error)
        << "'reboot' failed: " << request->status << " " << request->message;
    return false;
  }

  // Wait for the connection to be dropped.
  if (!WaitForState(S_REBOOTING, kRestartRebootingMaxWaitMilliseconds)) {
    BOOST_LOG_TRIVIAL(warning) << "Timed out waiting for rebooting message.";
  }

  // Then for the connection to be reestablished and a pending state set.
  if (!WaitForState(S_PENDING, kRestartPendingMaxWaitMilliseconds)) {
    BOOST_LOG_TRIVIAL(warning) << "Timed out waiting for pending message.";
  }

  // Reconnect the control channel.
  if (!context_->Reconnect()) {
    BOOST_LOG_TRIVIAL(error) << "Failed to reconnect after reboot.";
    return false;
  }

  return SetDebugger(true);
}

bool XBDMDebugger::WaitForState(ExecutionState s,
                                uint32_t max_wait_milliseconds) {
  std::unique_lock<std::mutex> lock(state_lock_);
  return state_condition_variable_.wait_for(
      lock, std::chrono::milliseconds(max_wait_milliseconds),
      [this, s] { return this->state_ == s; });
}

bool XBDMDebugger::StepInstruction() {
  auto thread = ActiveThread();
  if (!thread) {
    BOOST_LOG_TRIVIAL(error) << "StepInstruction called with no active thread.";
    return false;
  }

  if (!Stop()) {
    return false;
  }

  if (!thread->StepInstruction(*context_)) {
    return false;
  }

  return Go();
}

bool XBDMDebugger::StepFunction() {
  int thread_id = ActiveThreadID();
  if (thread_id < 0) {
    return false;
  }

  if (!Stop()) {
    return false;
  }

  bool ret = true;
  auto request = std::make_shared<::FuncCall>(thread_id);
  context_->SendCommandSync(request);
  ret = request->IsOK();
  return Go() && ret;
}

bool XBDMDebugger::ContinueAll(bool no_break_on_exception) {
  std::list<std::shared_ptr<Thread>> threads = Threads();
  bool ret = true;
  for (auto &thread : threads) {
    if (!thread->Continue(*context_, no_break_on_exception)) {
      BOOST_LOG_TRIVIAL(error)
          << "Failed to continue thread " << thread->thread_id;
      ret = false;
    }
  }
  return ret;
}

bool XBDMDebugger::HaltAll(uint32_t optimistic_max_wait) {
  std::list<std::shared_ptr<Thread>> threads = Threads();
  if (threads.empty()) {
    BOOST_LOG_TRIVIAL(warning) << "HaltAll called with no threads.";
    return true;
  }

  {
    auto request = std::make_shared<::Halt>();
    context_->SendCommandSync(request);
    if (!request->IsOK()) {
      BOOST_LOG_TRIVIAL(error) << "Failed to request halt " << request->status
                               << " " << request->message;
      return false;
    }
  }

  if (!WaitForState(S_STOPPED, optimistic_max_wait)) {
    BOOST_LOG_TRIVIAL(error)
        << "Failed to reach 'stopped' state. Current state is " << state_;
    return false;
  }

  auto active_thread = ActiveThread();
  if (active_thread) {
    active_thread->FetchStopReasonSync(*context_);
  } else {
    for (auto &thread : threads) {
      if (!thread->FetchStopReasonSync(*context_)) {
        continue;
      }
      if (thread->stopped) {
        SetActiveThread(thread->thread_id);
        active_thread = thread;
        break;
      }
    }
    if (!active_thread) {
      active_thread = threads.front();
    }
  }

  // TODO: Verify that this state is possible.
  // If the active_thread is not stopped, poll until it stops.
  // It is likely that this case can never happen in practice, as the execution
  // state is already guaranteed to be S_STOPPED at this point.
  constexpr uint32_t kDelayPerLoopMilliseconds = 10;
  while (!active_thread->stopped && optimistic_max_wait) {
    WaitMilliseconds(kDelayPerLoopMilliseconds);
    active_thread->FetchStopReasonSync(*context_);

    if (kDelayPerLoopMilliseconds > optimistic_max_wait) {
      break;
    }
    optimistic_max_wait -= kDelayPerLoopMilliseconds;
  }

  return active_thread->stopped;
}

bool XBDMDebugger::Halt() {
  auto thread = ActiveThread();
  if (!thread) {
    return false;
  }

  return thread->Halt(*context_);
}

std::optional<std::vector<uint8_t>> XBDMDebugger::GetMemory(uint32_t address,
                                                            uint32_t length) {
  // TODO: IMPLEMENT ME;
  BOOST_LOG_TRIVIAL(error) << "TODO: IMPLEMENT ME";
  return {};
}

bool XBDMDebugger::SetMemory(uint32_t address,
                             const std::vector<uint8_t> &data) {
  return false;
}

bool XBDMDebugger::AddBreakpoint(uint32_t address) { return false; }

bool XBDMDebugger::AddReadWatch(uint32_t address, uint32_t length) {
  return false;
}

bool XBDMDebugger::AddWriteWatch(uint32_t address, uint32_t length) {
  return false;
}

bool XBDMDebugger::RemoveBreakpoint(uint32_t address) { return false; }

bool XBDMDebugger::RemoveReadWatch(uint32_t address, uint32_t length) {
  return false;
}

bool XBDMDebugger::RemoveWriteWatch(uint32_t address, uint32_t length) {
  return false;
}
