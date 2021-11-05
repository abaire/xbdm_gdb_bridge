#include "xbdm_context.h"

#include <boost/asio/dispatch.hpp>
#include <boost/log/trivial.hpp>
#include <utility>

#include "net/delegating_server.h"
#include "net/select_thread.h"
#include "notification/xbdm_notification_transport.h"
#include "rdcp/rdcp_processed_request.h"
#include "rdcp/xbdm_transport.h"

XBDMContext::XBDMContext(std::string name, IPAddress xbox_address,
                         std::shared_ptr<SelectThread> select_thread)
    : name_(std::move(name)),
      xbox_address_(std::move(xbox_address)),
      select_thread_(std::move(select_thread)) {
  xbdm_transport_ = std::make_shared<XBDMTransport>(name_);
  select_thread_->AddConnection(xbdm_transport_);

  notification_server_ = std::make_shared<DelegatingServer>(
      name_, [this](int sock, IPAddress& address) {
        this->OnNotificationChannelConnected(sock, address);
      });
  select_thread_->AddConnection(notification_server_);

  xbdm_control_executor_ = std::make_shared<boost::asio::thread_pool>(1);
  notification_executor_ = std::make_shared<boost::asio::thread_pool>(1);
}

void XBDMContext::Shutdown() {
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
    BOOST_LOG_TRIVIAL(trace) << "Notification server may only be started once.";
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
  BOOST_LOG_TRIVIAL(trace) << "Notification channel established from "
                           << address;
  // TODO: Hold on to the transport so it can be shut down gracefully.
  auto transport = std::make_shared<XBDMNotificationTransport>(
      name_, sock, address,
      [this](std::shared_ptr<XBDMNotification> notification) {
        this->OnNotificationReceived(notification);
      });

  select_thread_->AddConnection(transport);
}

void XBDMContext::OnNotificationReceived(
    std::shared_ptr<XBDMNotification> notification) {
  assert(notification_executor_);
  boost::asio::dispatch(*notification_executor_,
                        [this, notification]() mutable {
                          this->DispatchNotification(notification);
                        });
}

bool XBDMContext::Reconnect() {
  if (xbdm_transport_) {
    xbdm_transport_->Close();
    xbdm_transport_.reset();
  }

  xbdm_transport_ = std::make_shared<XBDMTransport>(name_);
  select_thread_->AddConnection(xbdm_transport_);
  return xbdm_transport_->Connect(xbox_address_);
}

std::shared_ptr<RDCPProcessedRequest> XBDMContext::SendCommandSync(
    std::shared_ptr<RDCPProcessedRequest> command) {
  auto future = SendCommand(command);
  future.get();
  return command;
}

std::future<std::shared_ptr<RDCPProcessedRequest>> XBDMContext::SendCommand(
    std::shared_ptr<RDCPProcessedRequest> command) {
  assert(xbdm_control_executor_ && "SendCommand called before Start.");
  std::promise<std::shared_ptr<RDCPProcessedRequest>> promise;
  auto future = promise.get_future();
  boost::asio::dispatch(
      *xbdm_control_executor_,
      [this, promise = std::move(promise), command]() mutable {
        this->ExecuteXBDMPromise(promise, command);
      });
  return future;
}

void XBDMContext::ExecuteXBDMPromise(
    std::promise<std::shared_ptr<RDCPProcessedRequest>>& promise,
    std::shared_ptr<RDCPProcessedRequest>& request) {
  if (!XBDMConnect()) {
    request->status = StatusCode::ERR_NOT_CONNECTED;
  } else {
    xbdm_transport_->Send(request);
    request->WaitUntilCompleted();
  }

  promise.set_value(request);
}

bool XBDMContext::XBDMConnect(int max_wait_millis) {
  assert(xbdm_transport_);
  if (xbdm_transport_->CanProcessCommands()) {
    return true;
  }

  if (!xbdm_transport_->IsConnected() &&
      !xbdm_transport_->Connect(xbox_address_)) {
    return false;
  }

  static constexpr int busywait_millis = 5;
  while (max_wait_millis > 0) {
    if (xbdm_transport_->CanProcessCommands()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(busywait_millis));
    max_wait_millis -= busywait_millis;
  }

  return false;
}
int XBDMContext::RegisterNotificationHandler(
    XBDMContext::NotificationHandler handler) {
  const std::lock_guard<std::recursive_mutex> lock(notification_handler_lock_);
  int id = next_notification_handler_id_++;
  notification_handlers_[id] = std::move(handler);
  return id;
}
void XBDMContext::UnregisterNotificationHandler(int id) {
  if (id <= 0) {
    return;
  }

  const std::lock_guard<std::recursive_mutex> lock(notification_handler_lock_);
  notification_handlers_.erase(id);
}

void XBDMContext::DispatchNotification(
    std::shared_ptr<XBDMNotification> notification) {
  const std::lock_guard<std::recursive_mutex> lock(notification_handler_lock_);
  for (auto& handler : notification_handlers_) {
    handler.second(notification);
  }
}
