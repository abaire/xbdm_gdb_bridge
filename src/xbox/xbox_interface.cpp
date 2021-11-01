#include "xbox_interface.h"

#include <cassert>
#include <utility>

XBOXInterface::XBOXInterface(std::string name, IPAddress xbox_address) : name_(std::move(name)), xbox_address_(std::move(xbox_address)) {
}

void XBOXInterface::Start() {
  Stop();

  select_thread_ = std::make_shared<SelectThread>();
  select_thread_->Start();

  notification_server_ = std::make_shared<DelegatingServer>(name_, [this](int sock, IPAddress& address) { this->OnNotificationChannelConnected(sock, address); } );
  select_thread_->AddConnection(notification_server_);
}

void XBOXInterface::Stop() {
  if (!select_thread_.get()) {
    return;
  }

  select_thread_->Stop();
  select_thread_.reset();
}

bool XBOXInterface::ReconnectXBDM() {
  if (xbdm_transport_) {
    xbdm_transport_->Close();
    xbdm_transport_.reset();
  }

  xbdm_transport_ = std::make_shared<XBDMTransport>(name_);
  return xbdm_transport_->Connect(xbox_address_);
}

void XBOXInterface::StartGDBServer(const IPAddress&address) {
  if (gdb_server_) {
    gdb_server_->Close();
    gdb_server_.reset();
  }

  gdb_server_ = std::make_shared<DelegatingServer>(
      name_,
      [this](int sock, IPAddress& address) {
        this->OnGDBClientConnected(sock, address);
      });
  select_thread_->AddConnection(gdb_server_);
}

void XBOXInterface::StopGDBServer() {
  if (!gdb_server_) {
    return;
  }

  gdb_server_->Close();
  gdb_server_.reset();
}


void XBOXInterface::OnNotificationChannelConnected(int sock,
                                                   IPAddress& address) {
  auto transport = std::make_shared<XBDMNotificationTransport>(
      name_,
      [this](XBDMNotification& notification) {
        this->OnNotificationReceived(notification);
      });

  select_thread_->AddConnection(transport);
}

void XBOXInterface::OnNotificationReceived(XBDMNotification& notification) {

}

void XBOXInterface::OnGDBClientConnected(int sock, IPAddress& address) {

}
