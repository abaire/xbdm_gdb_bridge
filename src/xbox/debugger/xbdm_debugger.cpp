#include "xbdm_debugger.h"

#include <boost/log/trivial.hpp>

#include "notification/xbdm_notification.h"
#include "util/path.h"
#include "xbox/xbdm_context.h"

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

  {
    auto request = std::make_shared<Debugger>();
    context_->SendCommandSync(request);
    if (!request->IsOK()) {
      BOOST_LOG_TRIVIAL(error) << "Failed to enable debugger "
                               << request->status << " " << request->message;
      context_->UnregisterNotificationHandler(notification_handler_id_);
      return false;
    }

    target_not_debuggable_ = !request->debuggable;
  }
  return true;
}

void XBDMDebugger::Shutdown() {
  context_->SendCommand(std::make_shared<Debugger>(false));
  // TODO: Request a notifyat drop as well.
  context_->UnregisterNotificationHandler(notification_handler_id_);
  notification_handler_id_ = 0;
}

bool XBDMDebugger::DebugXBE(const std::string &path) {
  return DebugXBE(path, "");
}

bool XBDMDebugger::DebugXBE(const std::string &path,
                            const std::string &command_line) {
  uint32_t flags = 0;

  std::string xbe_dir;
  std::string xbe_name;
  if (!SplitXBEPath(path, xbe_dir, xbe_name)) {
    BOOST_LOG_TRIVIAL(error) << "Invalid XBE path '" << path << "'";
    return false;
  }

  auto request =
      std::make_shared<LoadOnBootTitle>(xbe_name, xbe_dir, command_line);

  return false;
}

std::shared_ptr<Thread> XBDMDebugger::GetThread(int thread_id) const {
  for (auto &it : threads_) {
    if (it->thread_id == thread_id) {
      return it;
    }
  }
  return {};
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

    default:
      BOOST_LOG_TRIVIAL(error)
          << "Unknown notification type received " << notification->Type();
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
  modules_.push_back(std::make_shared<Module>(msg->module));
}

void XBDMDebugger::OnSectionLoaded(
    const std::shared_ptr<NotificationSectionLoaded> &msg) {
  sections_.push_back(std::make_shared<Section>(msg->section));
}

void XBDMDebugger::OnThreadCreated(
    const std::shared_ptr<NotificationThreadCreated> &msg) {
  threads_.push_back(std::make_shared<Thread>(msg->thread_id));
}

void XBDMDebugger::OnThreadTerminated(
    const std::shared_ptr<NotificationThreadTerminated> &msg) {
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
  if (msg->state == NotificationExecutionStateChanged::S_REBOOTING) {
    modules_.clear();
    sections_.clear();
  }
}

void XBDMDebugger::OnBreakpoint(
    const std::shared_ptr<NotificationBreakpoint> &msg) const {
  auto thread = GetThread(msg->thread_id);
  if (!thread) {
    BOOST_LOG_TRIVIAL(warning)
        << "Received breakpoint message for unknown thread " << msg->thread_id;
    return;
  }

  // TODO: Consider fetching the full stop reason.
  thread->last_known_address = msg->address;
}

void XBDMDebugger::OnWatchpoint(
    const std::shared_ptr<NotificationWatchpoint> &msg) const {
  auto thread = GetThread(msg->thread_id);
  if (!thread) {
    BOOST_LOG_TRIVIAL(warning)
        << "Received breakpoint message for unknown thread " << msg->thread_id;
    return;
  }

  thread->last_known_address = msg->address;
}

void XBDMDebugger::OnSingleStep(
    const std::shared_ptr<NotificationSingleStep> &msg) const {
  auto thread = GetThread(msg->thread_id);
  if (!thread) {
    BOOST_LOG_TRIVIAL(warning)
        << "Received breakpoint message for unknown thread " << msg->thread_id;
    return;
  }

  thread->last_known_address = msg->address;
}

void XBDMDebugger::OnException(
    const std::shared_ptr<NotificationException> &msg) const {
  BOOST_LOG_TRIVIAL(warning) << "Received exception: " << *msg;
  auto thread = GetThread(msg->thread_id);
  if (!thread) {
    BOOST_LOG_TRIVIAL(warning)
        << "Received breakpoint message for unknown thread " << msg->thread_id;
    return;
  }
  thread->last_known_address = msg->address;
}
