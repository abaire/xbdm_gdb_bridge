#include "xbox_interface.h"

#include <unistd.h>

#include <cassert>
#include <utility>

XBOXInterface::XBOXInterface(std::string name, IPAddress xbox_address)
    : name_(std::move(name)), xbox_address_(std::move(xbox_address)) {}

void XBOXInterface::Start() {
  Stop();

  select_thread_ = std::make_shared<SelectThread>();
  select_thread_->Start();

  notification_server_ = std::make_shared<DelegatingServer>(
      name_, [this](int sock, IPAddress& address) {
        this->OnNotificationChannelConnected(sock, address);
      });
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

void XBOXInterface::StartGDBServer(const IPAddress& address) {
  if (gdb_server_) {
    gdb_server_->Close();
    gdb_server_.reset();
  }

  gdb_server_ = std::make_shared<DelegatingServer>(
      name_, [this](int sock, IPAddress& address) {
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
      name_, [this](XBDMNotification& notification) {
        this->OnNotificationReceived(notification);
      });

  select_thread_->AddConnection(transport);
}

void XBOXInterface::OnNotificationReceived(XBDMNotification& notification) {
  const std::lock_guard<std::mutex> lock(notification_queue_lock_);
  notification_queue_.push_back(notification);

  // TODO: Add condition variable and notify it that new data can be processed.
}

void XBOXInterface::OnGDBClientConnected(int sock, IPAddress& address) {
  if (gdb_transport_ && gdb_transport_->IsConnected()) {
    shutdown(sock, SHUT_RDWR);
    close(sock);
    return;
  }
  gdb_transport_ = std::make_shared<GDBTransport>(
      name_, sock, address,
      [this](GDBPacket& packet) { this->OnGDBPacketReceived(packet); });
  select_thread_->AddConnection(gdb_transport_);
}

void XBOXInterface::OnGDBPacketReceived(GDBPacket& packet) {
  const std::lock_guard<std::mutex> lock(gdb_queue_lock_);
  gdb_queue_.push_back(packet);

  // TODO: Add condition variable and notify it that new data can be processed.
}
