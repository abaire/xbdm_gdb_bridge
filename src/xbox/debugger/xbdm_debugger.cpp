#include "xbdm_debugger.h"

#include "notification/xbdm_notification.h"
#include "util/logging.h"
#include "util/path.h"
#include "util/timer.h"
#include "xbox/xbdm_context.h"

static constexpr uint32_t kRestartRebootingMaxWaitMilliseconds = 5 * 1000;
static constexpr uint32_t kRestartPendingMaxWaitMilliseconds = 15 * 1000;
static constexpr uint32_t kBreakAtStartMaxWaitMilliseconds = 10 * 1000;
static constexpr uint32_t kPostBreakAtStartThreadCreateMakeWaitMilliseconds =
    1 * 1000;

XBDMDebugger::XBDMDebugger(std::shared_ptr<XBDMContext> context)
    : context_(std::move(context)) {}

static bool RequestDebugNotifications(uint16_t port,
                                      std::shared_ptr<XBDMContext> context) {
  // Attempt to unregister any previous notifications on the given port.
  {
    auto request = std::make_shared<NotifyAt>(port, true, true);
    context->SendCommandSync(request);
  }

  {
    auto request = std::make_shared<NotifyAt>(port, false, true);
    context->SendCommandSync(request);
    if (!request->IsOK()) {
      LOG_DEBUGGER(error) << "Failed to request notifications "
                          << request->status << " " << request->message;
      return false;
    }
  }

  return true;
}

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

  // If a notification handler is currently active, attempt to wait until the
  // target is likely to be responsive before re-registering the notification
  // handler. This avoids a potential race condition where a gdb debugger
  // attaches during launch of an XBE, fails to reattach a notification handler,
  // and waits forever for the target to enter an interactive state.
  if (notification_handler_id_) {
    auto unsafe_states = std::set<ExecutionState>({S_INVALID, S_REBOOTING});
    WaitForStateNotIn(unsafe_states, kAttachSafeStateMaxWaitMilliseconds);
  }

  context_->UnregisterNotificationHandler(notification_handler_id_);
  notification_handler_id_ = context_->RegisterNotificationHandler(
      [this](const std::shared_ptr<XBDMNotification> &notification,
             XBDMContext &) { this->OnNotification(notification); });

  if (!RequestDebugNotifications(address.Port(), context_)) {
    context_->UnregisterNotificationHandler(notification_handler_id_);
    return false;
  }

  if (!SetDebugger(true)) {
    context_->UnregisterNotificationHandler(notification_handler_id_);
    return false;
  }

  if (!FetchThreads()) {
    return false;
  }

  if (!FetchModules()) {
    return false;
  }

  if (!FetchMemoryMap()) {
    return false;
  }

  is_attached_ = true;
  return true;
}

void XBDMDebugger::Shutdown() {
  is_attached_ = false;
  SetDebugger(false);
  // TODO: Request a notifyat drop as well.
  context_->UnregisterNotificationHandler(notification_handler_id_);
  notification_handler_id_ = 0;
}

bool XBDMDebugger::DebugXBE(const std::string &path, bool wait_forever,
                            bool break_at_start) {
  return DebugXBE(path, "", wait_forever, break_at_start);
}

bool XBDMDebugger::DebugXBE(const std::string &path,
                            const std::string &command_line, bool wait_forever,
                            bool break_at_start) {
  std::string xbe_dir;
  std::string xbe_name;
  if (!SplitXBEPath(path, xbe_dir, xbe_name)) {
    LOG_DEBUGGER(error) << "Invalid XBE path '" << path << "'";
    return false;
  }

  uint32_t flags = Reboot::kWait | Reboot::kWarm;
  if (wait_forever) {
    flags |= Reboot::kStop;
  }
  if (!RestartAndReconnect(flags)) {
    LOG_DEBUGGER(error) << "Failed to restart.";
    return false;
  }

  {
    auto request =
        std::make_shared<LoadOnBootTitle>(xbe_name, xbe_dir, command_line);
    context_->SendCommandSync(request);
    if (!request->IsOK()) {
      LOG_DEBUGGER(error) << "Failed to set load on boot title "
                          << request->status << " " << request->message;
      return false;
    }
  }

  if (!break_at_start) {
    return Go();
  }

  if (!BreakAtStart()) {
    return false;
  }

  if (!Go()) {
    return false;
  }

  if (!WaitForState(S_STOPPED, kBreakAtStartMaxWaitMilliseconds)) {
    // This indicates that the target has failed to break at start. It may be
    // worthwhile to stop and halt all threads to attempt to recover.
    LOG_DEBUGGER(error) << "Timed out waiting for break at start.";
  }

  if (!FetchThreads()) {
    LOG_DEBUGGER(warning)
        << "Failed to fetch threads while at start breakpoint.";
  }

  // Wait until the first application thread is created.
  {
    auto request = std::make_shared<StopOn>(StopOn::kCreateThread |
                                            StopOn::kFirstChanceException);
    context_->SendCommandSync(request);
    if (!request->IsOK()) {
      LOG_DEBUGGER(error) << "Failed to enable StopOn CreateThread "
                          << request->status << " " << request->message;
      return false;
    }
  }

  if (!ContinueAll()) {
    LOG_DEBUGGER(error) << "Failed to ContinueAll waiting on first app thread.";
  }

  if (!Go()) {
    LOG_DEBUGGER(error) << "Failed to Go waiting on first app thread.";
    return false;
  }

  auto remove_break_on_create = [this]() -> bool {
    auto request = std::make_shared<NoStopOn>(NoStopOn::kCreateThread);
    context_->SendCommandSync(request);
    if (!request->IsOK()) {
      LOG_DEBUGGER(error) << "Failed to disable StopOn CreateThread "
                          << request->status << " " << request->message;
      return false;
    }

    return true;
  };

  if (WaitForState(S_STOPPED,
                   kPostBreakAtStartThreadCreateMakeWaitMilliseconds)) {
    if (!remove_break_on_create()) {
      return false;
    }
  } else {
    remove_break_on_create();

    // This indicates that no new threads were created within the timeout. This
    // may be normal operation, particularly for alternatives to the official
    // XDK.
    LOG_DEBUGGER(warning) << "Timed out waiting for first app thread.";

    // If no threads are known, force a stop in order to determine an active
    // thread and produce a consistent state.
    if (!ActiveThread()) {
      if (!Stop()) {
        LOG_DEBUGGER(warning)
            << "Failed to stop when attempting to determine active thread.";
      }
      if (!FetchThreads()) {
        LOG_DEBUGGER(warning) << "Failed to fetch threads when attempting to "
                                 "determine active thread.";
      }
      if (!Go()) {
        LOG_DEBUGGER(error) << "Failed to Go after determining active thread.";
        return false;
      }
    }
  }

  FetchMemoryMap();

  return true;
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
  if (threads_.empty()) {
    return {};
  }
  ret.reserve(threads_.size());

  int32_t active_thread_id = ActiveThreadID();
  if (active_thread_id > 0) {
    ret.push_back(active_thread_id);
  }

  for (auto &thread : threads_) {
    if (thread->thread_id == active_thread_id) {
      continue;
    }
    ret.push_back(thread->thread_id);
  }
  return std::move(ret);
}

std::shared_ptr<Thread> XBDMDebugger::ActiveThread() {
  return GetThread(active_thread_id_);
}

std::shared_ptr<Thread> XBDMDebugger::GetAnyThread() {
  return GetThread(AnyThreadID());
}

std::shared_ptr<Thread> XBDMDebugger::GetThread(int thread_id) {
  if (thread_id < 0) {
    return nullptr;
  }

  const std::lock_guard<std::recursive_mutex> lock(threads_lock_);
  for (auto thread : threads_) {
    if (thread->thread_id == thread_id) {
      return thread;
    }
  }
  return nullptr;
}

std::shared_ptr<Thread> XBDMDebugger::GetFirstStoppedThread() {
  LOG_DEBUGGER(trace) << "Looking for first stopped thread";
  std::list<std::shared_ptr<Thread>> threads;
  int active_thread_id;
  {
    const std::lock_guard<std::recursive_mutex> lock(threads_lock_);
    if (threads_.empty()) {
      LOG_DEBUGGER(trace) << "No known threads";
      return nullptr;
    }
    threads = threads_;
    active_thread_id = active_thread_id_;
  }

  // Prefer the active active_thread if it's still stopped.
  std::shared_ptr<Thread> active_thread = ActiveThread();
  if (active_thread && active_thread->FetchStopReasonSync(*context_) &&
      active_thread->stopped) {
    return active_thread;
  }

  for (auto thread : threads) {
    if (thread->thread_id == active_thread_id) {
      continue;
    }
    if (thread->FetchStopReasonSync(*context_) && thread->stopped) {
      return thread;
    }
  }

  LOG_DEBUGGER(trace) << "No stopped threads";
  return nullptr;
}

bool XBDMDebugger::SetActiveThread(int thread_id) {
  const std::lock_guard<std::recursive_mutex> lock(threads_lock_);
  for (auto thread : threads_) {
    if (thread->thread_id == thread_id) {
      active_thread_id_ = thread_id;
      return true;
    }
  }

  active_thread_id_ = -1;
  return false;
}

void XBDMDebugger::OnNotification(
    const std::shared_ptr<XBDMNotification> &notification) {
  if (notification->Type() == NT_CUSTOM) {
    return;
  }

  LOG_DEBUGGER(trace) << "Notification received " << *notification;
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
      LOG_DEBUGGER(error) << "XBDMNotif: Received invalid notification type.";
      break;

    case NT_CUSTOM:
      // Custom events are not interesting to the debugger.
      break;
  }
}

void XBDMDebugger::OnVX(const std::shared_ptr<NotificationVX> &msg) {
  LOG_DEBUGGER(info) << "XBDMNotif: VX notification: " << std::endl << *msg;
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
    LOG_DEBUGGER(info) << std::endl << existing->second << *msg;
    debugstr_accumulator_.erase(existing);
    return;
  }

  LOG_DEBUGGER(info) << std::endl << *msg;
}

void XBDMDebugger::OnModuleLoaded(
    const std::shared_ptr<NotificationModuleLoaded> &msg) {
  LOG_DEBUGGER(info) << "Module loaded";
  std::unique_lock<std::recursive_mutex> lock(modules_lock_);
  modules_.push_back(std::make_shared<Module>(msg->module));
  FetchMemoryMap();
}

void XBDMDebugger::OnSectionLoaded(
    const std::shared_ptr<NotificationSectionLoaded> &msg) {
  std::unique_lock<std::recursive_mutex> lock(sections_lock_);
  sections_.push_back(std::make_shared<Section>(msg->section));
  FetchMemoryMap();
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
  FetchMemoryMap();
}

void XBDMDebugger::OnThreadCreated(
    const std::shared_ptr<NotificationThreadCreated> &msg) {
  LOG_DEBUGGER(info) << "Thread created: " << msg->thread_id;
  const std::lock_guard<std::recursive_mutex> lock(threads_lock_);
  for (auto thread : threads_) {
    if (thread->thread_id == msg->thread_id) {
      LOG_DEBUGGER(warning)
          << "Ignoring duplicate thread creation for " << msg->thread_id;
      return;
    }
  }

  threads_.push_back(std::make_shared<Thread>(msg->thread_id));
}

void XBDMDebugger::OnThreadTerminated(
    const std::shared_ptr<NotificationThreadTerminated> &msg) {
  LOG_DEBUGGER(info) << "Thread terminated: " << msg->thread_id;
  const std::lock_guard<std::recursive_mutex> lock(threads_lock_);
  for (auto it = threads_.begin(); it != threads_.end(); ++it) {
    auto &thread = *it;
    if (thread->thread_id == msg->thread_id) {
      if (thread->thread_id == active_thread_id_) {
        active_thread_id_ = -1;
      }
      threads_.erase(it);
      return;
    }
  }

  LOG_DEBUGGER(warning)
      << "XBDMNotif: Received thread termination message for unknown thread "
      << msg->thread_id;
}

void XBDMDebugger::OnExecutionStateChanged(
    const std::shared_ptr<NotificationExecutionStateChanged> &msg) {
  LOG_DEBUGGER(info) << "XBDMNotif: State changed: " << *msg;

  {
    const std::lock_guard<std::mutex> lock(state_lock_);
    state_ = msg->state;
    if (state_ == ExecutionState::S_REBOOTING) {
      modules_.clear();
      sections_.clear();
    }
  }
  if (state_ == ExecutionState::S_STOPPED) {
    auto stopped_thread = GetFirstStoppedThread();
    if (stopped_thread) {
      SetActiveThread(stopped_thread->thread_id);
    }
    FetchMemoryMap();
  }
  state_condition_variable_.notify_all();
}

void XBDMDebugger::OnBreakpoint(
    const std::shared_ptr<NotificationBreakpoint> &msg) {
  auto thread = GetThread(msg->thread_id);
  if (!thread) {
    LOG_DEBUGGER(warning)
        << "XBDMNotif: Received breakpoint message for unknown thread "
        << msg->thread_id;
    return;
  }

  SetActiveThread(thread->thread_id);
  thread->last_known_address = msg->address;
  // TODO: Set the stop reason from the notification content.
  thread->FetchStopReasonSync(*context_);

  // Threads created with StopOn CreateThread will be started in a suspended
  // state and should be resumed here.
  thread->FetchInfoSync(*context_);
  if (thread->suspend_count > 0) {
    thread->Resume(*context_);
  }
}

void XBDMDebugger::OnWatchpoint(
    const std::shared_ptr<NotificationWatchpoint> &msg) {
  LOG_DEBUGGER(trace) << "Watchpoint " << msg->thread_id << "@" << std::hex
                      << msg->address << " accessing " << msg->watched_address
                      << std::dec;
}

void XBDMDebugger::OnSingleStep(
    const std::shared_ptr<NotificationSingleStep> &msg) {
  auto thread = GetThread(msg->thread_id);
  if (!thread) {
    LOG_DEBUGGER(warning)
        << "XBDMNotif: Received breakpoint message for unknown thread "
        << msg->thread_id;
    return;
  }

  SetActiveThread(thread->thread_id);
  thread->last_known_address = msg->address;
  // TODO: Set the stop reason from the notification content.
  thread->FetchStopReasonSync(*context_);
}

void XBDMDebugger::OnException(
    const std::shared_ptr<NotificationException> &msg) {
  LOG_DEBUGGER(warning) << "Received exception: " << *msg;
  auto thread = GetThread(msg->thread_id);
  if (!thread) {
    LOG_DEBUGGER(warning)
        << "XBDMNotif: Received breakpoint message for unknown thread "
        << msg->thread_id;
    return;
  }

  SetActiveThread(thread->thread_id);
  thread->last_known_address = msg->address;
  // TODO: Set the stop reason from the notification content.
  thread->FetchStopReasonSync(*context_);
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
    LOG_DEBUGGER(error) << "'stop' failed: " << request->status << " "
                        << request->message;
    return false;
  }
  return true;
}

bool XBDMDebugger::Go() const {
  auto request = std::make_shared<::Go>();
  context_->SendCommandSync(request);
  if (!request->IsOK()) {
    LOG_DEBUGGER(error) << "'go' failed: " << request->status << " "
                        << request->message;
    return false;
  }
  return true;
}

bool XBDMDebugger::BreakAtStart() const {
  auto request = std::make_shared<::BreakAtStart>();
  context_->SendCommandSync(request);
  if (!request->IsOK()) {
    LOG_DEBUGGER(error) << "Failed to request break at start "
                        << request->status << " " << request->message;
    return false;
  }
  return true;
}

bool XBDMDebugger::SetDebugger(bool enabled) {
  auto request = std::make_shared<Debugger>();
  context_->SendCommandSync(request);
  if (!request->IsOK()) {
    LOG_DEBUGGER(error) << "Failed to enable debugger " << request->status
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
    LOG_DEBUGGER(error) << "Failed to fetch thread list " << request->status
                        << " " << request->message;
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
      LOG_DEBUGGER(error) << "Failed to fetch info for thread "
                          << thread->thread_id;
    }
  }

  return true;
}

bool XBDMDebugger::FetchModules() {
  auto request = std::make_shared<::Modules>();
  context_->SendCommandSync(request);
  if (!request->IsOK()) {
    LOG_DEBUGGER(error) << "Failed to fetch module list " << request->status
                        << " " << request->message;
    return false;
  }

  const std::lock_guard<std::recursive_mutex> lock(modules_lock_);
  modules_.clear();
  for (auto &module : request->modules) {
    modules_.emplace_back(std::make_shared<Module>(module));
  }

  return true;
}

bool XBDMDebugger::FetchMemoryMap() {
  auto request = std::make_shared<::WalkMem>();
  context_->SendCommandSync(request);
  if (!request->IsOK()) {
    LOG_DEBUGGER(error) << "Failed to fetch memory map " << request->status
                        << " " << request->message;
    return false;
  }

  const std::lock_guard<std::recursive_mutex> lock(memory_regions_lock_);
  memory_regions_.clear();
  for (auto &region : request->regions) {
    memory_regions_.emplace_back(std::make_shared<MemoryRegion>(region));
  }
  memory_regions_.sort([](const std::shared_ptr<MemoryRegion> &a,
                          const std::shared_ptr<MemoryRegion> &b) {
    return a->start < b->start;
  });

  return true;
}

bool XBDMDebugger::RestartAndReconnect(uint32_t reboot_flags) {
  assert(context_);
  LOG_DEBUGGER(trace) << "Rebooting remote with flags " << std::setw(8)
                      << std::hex << reboot_flags;
  {
    auto request = std::make_shared<::Reboot>(reboot_flags);
    context_->SendCommandSync(request);
    if (!request->IsOK()) {
      LOG_DEBUGGER(error) << "'reboot' failed: " << request->status << " "
                          << request->message;
      return false;
    }
  }

  // Wait for the Xbox to indicate that it is about to reboot.
  LOG_DEBUGGER(trace) << "Awaiting rebooting notification.";
  if (!WaitForState(S_REBOOTING, kRestartRebootingMaxWaitMilliseconds)) {
    LOG_DEBUGGER(warning) << "Timed out waiting for rebooting message.";
  }

  // Gracefully drop all connections.
  {
    context_->ResetNotificationConnections();

    LOG_DEBUGGER(trace) << "Sending bye message.";
    auto request = std::make_shared<::Bye>();
    context_->SendCommandSync(request);
    // No need to check for success or failure.

    context_->CloseActiveConnections();
  }

  // Then for the notification connection to be reestablished. A real devkit
  // interaction waits for a pending notification before reconnecting the 731
  // transport, but the bridge fetches module information on the modload
  // notifications that come in before pending. To avoid having to delay these
  // fetches, the notification channel reconnect triggers the transport level
  // reconnect in .

  LOG_DEBUGGER(trace) << "Awaiting pending notification.";
  if (!WaitForState(S_PENDING, kRestartPendingMaxWaitMilliseconds)) {
    LOG_DEBUGGER(warning) << "Timed out waiting for pending message.";
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

bool XBDMDebugger::WaitForStateIn(const std::set<ExecutionState> &target_states,
                                  uint32_t max_wait_milliseconds) {
  std::unique_lock<std::mutex> lock(state_lock_);
  return state_condition_variable_.wait_for(
      lock, std::chrono::milliseconds(max_wait_milliseconds),
      [this, &target_states] {
        return target_states.find(this->state_) != target_states.end();
      });
}

bool XBDMDebugger::WaitForStateNotIn(
    const std::set<ExecutionState> &banned_states,
    uint32_t max_wait_milliseconds) {
  std::unique_lock<std::mutex> lock(state_lock_);
  return state_condition_variable_.wait_for(
      lock, std::chrono::milliseconds(max_wait_milliseconds),
      [this, &banned_states] {
        return banned_states.find(this->state_) == banned_states.end();
      });
}

ExecutionState XBDMDebugger::CurrentKnownState() {
  std::unique_lock<std::mutex> lock(state_lock_);
  return state_;
}

bool XBDMDebugger::StepInstruction() {
  auto thread = ActiveThread();
  if (!thread) {
    LOG_DEBUGGER(error) << "StepInstruction called with no active thread.";
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
      LOG_DEBUGGER(error) << "Failed to continue thread " << thread->thread_id;
      ret = false;
    }
  }
  return ret;
}

bool XBDMDebugger::ContinueThread(int thread_id, bool no_break_on_exception) {
  auto thread = GetThread(thread_id);
  if (!thread) {
    LOG_DEBUGGER(error) << "Failed to continue unknown thread " << thread_id;
    return false;
  }

  if (!thread->Continue(*context_, no_break_on_exception)) {
    LOG_DEBUGGER(error) << "Failed to continue thread " << thread->thread_id;
    return false;
  }
  return true;
}

bool XBDMDebugger::HaltAll(uint32_t optimistic_max_wait) {
  std::list<std::shared_ptr<Thread>> threads = Threads();
  if (threads.empty()) {
    LOG_DEBUGGER(warning) << "HaltAll called with no threads.";
    return true;
  }

  {
    auto request = std::make_shared<::Halt>();
    context_->SendCommandSync(request);
    if (!request->IsOK()) {
      LOG_DEBUGGER(error) << "Failed to request halt " << request->status << " "
                          << request->message;
      return false;
    }
  }

  if (!WaitForState(S_STOPPED, optimistic_max_wait)) {
    LOG_DEBUGGER(error) << "Failed to reach 'stopped' state. Current state is "
                        << state_;
    return false;
  }

  auto active_thread = GetFirstStoppedThread();
  if (!active_thread) {
    LOG_DEBUGGER(warning) << "No threads stopped after HaltAll";
    active_thread = threads.front();
  }
  SetActiveThread(active_thread->thread_id);

  // TODO: Verify that this state is possible.
  // If the active_thread is not stopped, poll until it stops.
  // It is likely that this case can never happen in practice, as the execution
  // state is already guaranteed to be S_STOPPED at this point.
  constexpr uint32_t kDelayPerLoopMilliseconds = 10;
  while (!active_thread->stopped && optimistic_max_wait) {
    WaitMilliseconds(kDelayPerLoopMilliseconds);
    LOG_DEBUGGER(trace) << "Polling for thread stop state...";
    active_thread->FetchStopReasonSync(*context_);

    if (kDelayPerLoopMilliseconds > optimistic_max_wait) {
      break;
    }
    optimistic_max_wait -= kDelayPerLoopMilliseconds;
  }
  if (!active_thread->stopped) {
    LOG_DEBUGGER(warning) << "No threads stopped after HaltAll";
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
  if (!ValidateMemoryAccess(address, length)) {
    return std::nullopt;
  }

  auto request = std::make_shared<GetMemBinary>(address, length);
  context_->SendCommandSync(request);
  if (!request->IsOK()) {
    LOG_DEBUGGER(error) << "Failed to read memory " << request->status << " "
                        << request->message;
    return std::nullopt;
  }

  return request->data;
}

std::optional<uint32_t> XBDMDebugger::GetDWORD(uint32_t address) {
  auto raw = GetMemory(address, 4);
  if (!raw.has_value()) {
    return std::nullopt;
  }

  return *reinterpret_cast<uint32_t *>(raw->data());
}

bool XBDMDebugger::SetMemory(uint32_t address,
                             const std::vector<uint8_t> &data) {
  if (!ValidateMemoryAccess(address, data.size(), true)) {
    return false;
  }

  auto request = std::make_shared<SetMem>(address, data);
  context_->SendCommandSync(request);
  return request->IsOK();
}

bool XBDMDebugger::AddBreakpoint(uint32_t address) {
  auto request = std::make_shared<BreakAddress>(address);
  context_->SendCommandSync(request);
  return request->IsOK();
}

bool XBDMDebugger::AddReadWatch(uint32_t address, uint32_t length) {
  auto request = std::make_shared<BreakOnRead>(address, length);
  context_->SendCommandSync(request);
  return request->IsOK();
}

bool XBDMDebugger::AddWriteWatch(uint32_t address, uint32_t length) {
  auto request = std::make_shared<BreakOnWrite>(address, length);
  context_->SendCommandSync(request);
  return request->IsOK();
}

bool XBDMDebugger::RemoveBreakpoint(uint32_t address) {
  auto request = std::make_shared<BreakAddress>(address, true);
  context_->SendCommandSync(request);
  return request->IsOK();
}

bool XBDMDebugger::RemoveReadWatch(uint32_t address, uint32_t length) {
  auto request = std::make_shared<BreakOnRead>(address, length, true);
  context_->SendCommandSync(request);
  return request->IsOK();
}

bool XBDMDebugger::RemoveWriteWatch(uint32_t address, uint32_t length) {
  auto request = std::make_shared<BreakOnWrite>(address, length, true);
  context_->SendCommandSync(request);
  return request->IsOK();
}

bool XBDMDebugger::ValidateMemoryAccess(uint32_t address, uint32_t length,
                                        bool is_write) {
  std::lock_guard<std::recursive_mutex> lock(memory_regions_lock_);
  if (memory_regions_.empty()) {
    LOG_DEBUGGER(warning) << "No memory regions mapped, assuming access is OK.";
    return true;
  }

  uint32_t start = address;
  uint32_t end = address + length;
  for (auto &region : memory_regions_) {
    if (region->Contains(start)) {
      if (region->Contains(end)) {
        if (is_write && !region->IsWritable()) {
          return false;
        }
        return true;
      }
      start = region->end;
    }
  }

  return false;
}

std::shared_ptr<Module> XBDMDebugger::GetModule(
    const std::string &module_name) const {
  auto modules = Modules();
  for (auto &module : modules) {
    if (module->name == module_name) {
      return module;
    }
  }
  return nullptr;
}
