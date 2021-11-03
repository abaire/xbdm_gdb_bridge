#include "xbox_interface.h"

#include <unistd.h>

#include <boost/asio/dispatch.hpp>
#include <boost/log/trivial.hpp>
#include <cassert>
#include <utility>

XBOXInterface::XBOXInterface(std::string name, IPAddress xbox_address)
    : name_(std::move(name)), xbox_address_(std::move(xbox_address)) {}

void XBOXInterface::Start() {
  Stop();

  select_thread_ = std::make_shared<SelectThread>();

  notification_server_ = std::make_shared<DelegatingServer>(
      name_, [this](int sock, IPAddress& address) {
        this->OnNotificationChannelConnected(sock, address);
      });
  select_thread_->AddConnection(notification_server_);

  xbdm_transport_ = std::make_shared<XBDMTransport>(name_);
  select_thread_->AddConnection(xbdm_transport_);

  xbdm_control_executor_ = std::make_shared<boost::asio::thread_pool>(1);
  select_thread_->Start();
}

void XBOXInterface::Stop() {
  if (!select_thread_.get()) {
    return;
  }

  select_thread_->Stop();
  select_thread_.reset();

  if (xbdm_control_executor_) {
    xbdm_control_executor_->stop();
    xbdm_control_executor_->join();
  }
}

bool XBOXInterface::ReconnectXBDM() {
  if (xbdm_transport_) {
    xbdm_transport_->Close();
    xbdm_transport_.reset();
  }

  xbdm_transport_ = std::make_shared<XBDMTransport>(name_);
  select_thread_->AddConnection(xbdm_transport_);
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
  const std::lock_guard<std::recursive_mutex> lock(notification_queue_lock_);
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
  const std::lock_guard<std::recursive_mutex> lock(gdb_queue_lock_);
  gdb_queue_.push_back(packet);

  // TODO: Add condition variable and notify it that new data can be processed.
}

std::shared_ptr<RDCPProcessedRequest> XBOXInterface::SendCommandSync(
    std::shared_ptr<RDCPProcessedRequest> command) {
  auto future = SendCommand(command);
  future.get();
  return command;
}

std::future<std::shared_ptr<RDCPProcessedRequest>> XBOXInterface::SendCommand(
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

void XBOXInterface::ExecuteXBDMPromise(
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

bool XBOXInterface::XBDMConnect(int max_wait_millis) {
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
