#include "xbox_interface.h"

#include <utility>

XBOXInterface::XBOXInterface(std::string name, Address  xbox_address) : name_(std::move(name)), xbox_address_(std::move(xbox_address)) {
}

void XBOXInterface::Start() {
  Stop();

  select_thread_ = std::make_shared<SelectThread>();
  select_thread_->Start();

  notification_server_ = std::make_shared<NotificationServer>(name_, [this](int sock, Address& address) { this->OnNotificationChannelConnected(sock, address); } );
  select_thread_->AddConnection(notification_server_);
}

void XBOXInterface::Stop() {
  if (!select_thread_.get()) {
    return;
  }

  select_thread_->Stop();
  select_thread_.reset();
}

void XBOXInterface::OnNotificationChannelConnected(int sock, Address& address) {
  std::shared_ptr<XBDMNotificationTransport> transport = std::make_shared<XBDMNotificationTransport>(name_, [this](XBDMNotification& notification) { this->OnNotificationReceived(notification); } );
}

void XBOXInterface::OnNotificationReceived(XBDMNotification& notification) {

}
