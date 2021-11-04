#include "xbdm_debugger.h"

#include <boost/log/trivial.hpp>

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
      [this](const XBDMNotification& notification) {
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
  context_->UnregisterNotificationHandler(notification_handler_id_);
}

bool XBDMDebugger::DebugXBE(const std::string& path) {
  return DebugXBE(path, "");
}

bool XBDMDebugger::DebugXBE(const std::string& path,
                            const std::string& command_line) {
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

void XBDMDebugger::OnNotification(const XBDMNotification& notification) {}
