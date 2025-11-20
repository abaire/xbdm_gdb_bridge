#include "debugger_xbox_interface.h"

#include <iostream>
#include <utility>

#include "debugger_expression_parser.h"
#include "notification/xbdm_notification.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"

struct ContextAwareExpressionParser : DebuggerExpressionParser {
  explicit ContextAwareExpressionParser(std::shared_ptr<XBDMDebugger> debugger)
      : debugger_(std::move(debugger)) {}

  std::expected<uint32_t, std::string> Parse(const std::string& expr) override {
    if (debugger_) {
      auto active_thread = debugger_->ActiveThread();
      if (active_thread) {
        auto& context = active_thread->context;
        if (context) {
          context_ = *context;
          thread_id_ = active_thread->thread_id;
          return DebuggerExpressionParser::Parse(expr);
        }
      }
    }

    context_.Reset();
    return DebuggerExpressionParser::Parse(expr);
  }

 private:
  std::shared_ptr<XBDMDebugger> debugger_;
};

bool DebuggerXBOXInterface::AttachDebugger() {
  if (!xbdm_debugger_) {
    xbdm_debugger_ = std::make_shared<XBDMDebugger>(xbdm_context_);
    SetExpressionParser(
        std::make_shared<ContextAwareExpressionParser>(xbdm_debugger_));
  }

  return xbdm_debugger_->Attach();
}

void DebuggerXBOXInterface::DetachDebugger() {
  if (!xbdm_debugger_) {
    return;
  }

  xbdm_debugger_->Shutdown();
  xbdm_debugger_.reset();

  SetExpressionParser(std::make_shared<DebuggerExpressionParser>());
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
