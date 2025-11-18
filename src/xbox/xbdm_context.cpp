#include "xbdm_context.h"

#include <boost/asio/dispatch.hpp>
#include <utility>

#include "net/delegating_server.h"
#include "net/select_thread.h"
#include "notification/xbdm_notification_transport.h"
#include "rdcp/rdcp_processed_request.h"
#include "rdcp/xbdm_transport.h"
#include "util/logging.h"
#include "util/timer.h"

XBDMContext::XBDMContext(std::string name, IPAddress xbox_address,
                         std::shared_ptr<SelectThread> select_thread)
    : name_(std::move(name)),
      xbox_address_(std::move(xbox_address)),
      select_thread_(std::move(select_thread)) {
  xbdm_transport_ = std::make_shared<XBDMTransport>(logging::kLoggingTagXBDM);
  select_thread_->AddConnection(xbdm_transport_);

  notification_server_ = std::make_shared<DelegatingServer>(
      name_ + "__xbdm_notification_server",
      [this](int sock, IPAddress& address) {
        OnNotificationChannelConnected(sock, address);
      });
  select_thread_->AddConnection(notification_server_);

  xbdm_control_executor_ = std::make_shared<boost::asio::thread_pool>(1);
  notification_executor_ = std::make_shared<boost::asio::thread_pool>(1);
}

void XBDMContext::Shutdown() {
  for (const auto& it : dedicated_transports_) {
    it.second->Close();
  }
  dedicated_transports_.clear();

  if (xbdm_transport_) {
    xbdm_transport_->Close();
    xbdm_transport_.reset();
  }
  if (xbdm_control_executor_) {
    xbdm_control_executor_->stop();
    xbdm_control_executor_->join();
    xbdm_control_executor_.reset();
  }
  if (notification_server_) {
    notification_server_->Close();
    notification_server_.reset();
  }
  if (notification_executor_) {
    notification_executor_->stop();
    notification_executor_->join();
    notification_executor_.reset();
  }
}

bool XBDMContext::StartNotificationListener(const IPAddress& address) {
  if (notification_server_->IsConnected()) {
    LOG_XBDM(trace) << "Notification server may only be started once.";
    return false;
  }

  return notification_server_->Listen(address);
}

bool XBDMContext::GetNotificationServerAddress(IPAddress& address) const {
  if (!notification_server_ || !notification_server_->IsConnected()) {
    return false;
  }

  address = notification_server_->Address();
  return true;
}

void XBDMContext::OnNotificationChannelConnected(int sock, IPAddress& address) {
  LOG_XBDM(trace) << "Notification channel established from " << address;

  // After a reboot, XBDM will no longer send an initial empty message
  // indicating that the connection is fully established, and will instead
  // reconnect the notification channel. In a real XBDM session, the reconnect
  // would be delayed until an "execution pending" notification is received. To
  // reproduce this would require queueing the handling of `modload`
  // notifications, and it seems like an immediate reconnect here causes no
  // issues.
  if (!xbdm_transport_ || !xbdm_transport_->CanProcessCommands()) {
    LOG_XBDM(trace) << "Reconnecting XBDM transport due to notification.";
    Reconnect();
  }

  auto transport = std::make_shared<XBDMNotificationTransport>(
      logging::kLoggingTagXBDMNotification, sock, address,
      [this](std::shared_ptr<XBDMNotification> notification) {
        this->OnNotificationReceived(std::move(notification));
      });

  notification_transports_.insert(transport);

  select_thread_->AddConnection(transport, [this, transport]() {
    const std::lock_guard lock(notification_transports_lock_);
    notification_transports_.erase(transport);
  });
}

void XBDMContext::OnNotificationReceived(
    std::shared_ptr<XBDMNotification> notification) {
  assert(notification_executor_);
  boost::asio::dispatch(*notification_executor_,
                        [this, notification]() mutable {
                          this->DispatchNotification(notification);
                        });
}

void XBDMContext::CloseActiveConnections() {
  if (xbdm_transport_) {
    xbdm_transport_->Close();
  }

  ResetNotificationConnections();
}

void XBDMContext::ResetNotificationConnections() {
  const std::lock_guard lock(notification_transports_lock_);
  for (auto& transport : notification_transports_) {
    transport->Close();
  }
}

bool XBDMContext::Reconnect() {
  if (xbdm_transport_) {
    xbdm_transport_->Close();
  }

  xbdm_transport_ = std::make_shared<XBDMTransport>(logging::kLoggingTagXBDM);
  select_thread_->AddConnection(xbdm_transport_);
  return xbdm_transport_->Connect(xbox_address_);
}

std::shared_ptr<RDCPProcessedRequest> XBDMContext::SendCommandSync(
    const std::shared_ptr<RDCPProcessedRequest>& command) {
  if (!xbdm_transport_) {
    return nullptr;
  }

  auto future = SendCommand(command);
  future.get();
  return command;
}

std::future<std::shared_ptr<RDCPProcessedRequest>> XBDMContext::SendCommand(
    const std::shared_ptr<RDCPProcessedRequest>& command) {
  return SendCommand(command, xbdm_transport_);
}

std::shared_ptr<RDCPProcessedRequest> XBDMContext::SendCommandSync(
    std::shared_ptr<RDCPProcessedRequest> command,
    const std::shared_ptr<XBDMTransport>& transport) {
  auto future = SendCommand(command, transport);
  future.get();
  return command;
}

std::future<std::shared_ptr<RDCPProcessedRequest>> XBDMContext::SendCommand(
    const std::shared_ptr<RDCPProcessedRequest>& command,
    const std::string& dedicated_handler) {
  auto it = dedicated_transports_.find(dedicated_handler);
  if (it == dedicated_transports_.end()) {
    assert(CreateDedicatedChannel(dedicated_handler));
    it = dedicated_transports_.find(dedicated_handler);
    assert(it != dedicated_transports_.end());
  }

  return SendCommand(command, it->second);
}

std::shared_ptr<RDCPProcessedRequest> XBDMContext::SendCommandSync(
    const std::shared_ptr<RDCPProcessedRequest>& command,
    const std::string& dedicated_handler) {
  auto future = SendCommand(command, dedicated_handler);
  future.get();
  return command;
}

std::future<std::shared_ptr<RDCPProcessedRequest>> XBDMContext::SendCommand(
    const std::shared_ptr<RDCPProcessedRequest>& command,
    const std::shared_ptr<XBDMTransport>& transport) {
  assert(xbdm_control_executor_ && "SendCommand called before Start.");
  assert(transport && "transport must not be null");
  std::promise<std::shared_ptr<RDCPProcessedRequest>> promise;
  auto future = promise.get_future();

  boost::asio::dispatch(
      *xbdm_control_executor_,
      [this, promise = std::move(promise), command, transport]() mutable {
        this->ExecuteXBDMPromise(promise, command, transport);
      });
  return future;
}

bool XBDMContext::CreateDedicatedChannel(const std::string& command_handler) {
  if (dedicated_transports_.find(command_handler) !=
      dedicated_transports_.end()) {
    return false;
  }

  std::string tag = logging::kLoggingTagXBDM;
  tag += "_";
  tag += command_handler;
  auto transport = std::make_shared<XBDMTransport>(tag);
  select_thread_->AddConnection(transport);

  if (!transport->Connect(xbox_address_)) {
    return false;
  }

  dedicated_transports_[command_handler] = transport;
  return true;
}

void XBDMContext::DestroyDedicatedChannel(const std::string& command_handler) {
  auto it = dedicated_transports_.find(command_handler);
  if (it == dedicated_transports_.end()) {
    return;
  }

  it->second->Close();
  dedicated_transports_.erase(it);
}

void XBDMContext::ExecuteXBDMPromise(
    std::promise<std::shared_ptr<RDCPProcessedRequest>>& promise,
    const std::shared_ptr<RDCPProcessedRequest>& request,
    const std::shared_ptr<XBDMTransport>& transport) {
  assert(transport && "Invalid transport during ExecuteXBDMPromise");
  if (!XBDMConnect(transport)) {
    request->status = StatusCode::ERR_NOT_CONNECTED;
  } else {
    LOG_XBDM(trace) << "Send " << *request;
    transport->Send(request);
    request->WaitUntilCompleted();
  }

  promise.set_value(request);
}

bool XBDMContext::XBDMConnect(const std::shared_ptr<XBDMTransport>& transport,
                              int max_wait_millis) {
  assert(transport && "Invalid transport during XBDMConnect");
  if (transport->CanProcessCommands()) {
    return true;
  }

  if (!transport->IsConnected() && !transport->Connect(xbox_address_)) {
    return false;
  }

  static constexpr int busywait_millis = 5;
  while (max_wait_millis > 0) {
    if (transport->CanProcessCommands()) {
      return true;
    }

    WaitMilliseconds(busywait_millis);
    max_wait_millis -= busywait_millis;
  }

  LOG_XBDM(warning)
      << "Timeout waiting for command processing to become available.";
  return false;
}
int XBDMContext::RegisterNotificationHandler(
    XBDMContext::NotificationHandler handler) {
  const std::lock_guard lock(notification_handler_lock_);
  int id = next_notification_handler_id_++;
  notification_handlers_[id] = std::move(handler);
  return id;
}
void XBDMContext::UnregisterNotificationHandler(int id) {
  if (id <= 0) {
    return;
  }

  const std::lock_guard lock(notification_handler_lock_);
  notification_handlers_.erase(id);
}

void XBDMContext::DispatchNotification(
    const std::shared_ptr<XBDMNotification>& notification) {
  std::map<int, NotificationHandler> handlers;

  {
    const std::lock_guard lock(notification_handler_lock_);
    handlers = notification_handlers_;
  }

  for (auto& handler : handlers) {
    handler.second(notification, *this);
  }
}
