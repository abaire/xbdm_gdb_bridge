#include "debugger_xbox_interface.h"

#include <iostream>

#include "notification/xbdm_notification.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"

bool DebuggerXBOXInterface::AttachDebugger() {
  if (!xbdm_debugger_) {
    xbdm_debugger_ = std::make_shared<XBDMDebugger>(xbdm_context_);
  }

  return xbdm_debugger_->Attach();
}

void DebuggerXBOXInterface::DetachDebugger() {
  if (!xbdm_debugger_) {
    return;
  }

  xbdm_debugger_->Shutdown();
  xbdm_debugger_.reset();
}

void DebuggerXBOXInterface::AttachDebugNotificationHandler() {
  if (debug_notification_handler_id_ > 0) {
    return;
  }

  debug_notification_handler_id_ = xbdm_context_->RegisterNotificationHandler(
      [](const std::shared_ptr<XBDMNotification>& notification, XBDMContext&) {
        std::cout << *notification << std::endl;
      });
}

void DebuggerXBOXInterface::DetachDebugNotificationHandler() {
  if (debug_notification_handler_id_ <= 0) {
    return;
  }

  xbdm_context_->UnregisterNotificationHandler(debug_notification_handler_id_);
  debug_notification_handler_id_ = 0;
}
