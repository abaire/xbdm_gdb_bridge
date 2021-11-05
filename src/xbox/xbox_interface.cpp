#include "xbox_interface.h"

#include <unistd.h>

#include <boost/asio/dispatch.hpp>
#include <cassert>
#include <utility>

#include "gdb/gdb_transport.h"
#include "net/delegating_server.h"
#include "net/select_thread.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"

XBOXInterface::XBOXInterface(std::string name, IPAddress xbox_address)
    : name_(std::move(name)), xbox_address_(std::move(xbox_address)) {}

void XBOXInterface::Start() {
  Stop();

  select_thread_ = std::make_shared<SelectThread>();
  xbdm_context_ =
      std::make_shared<XBDMContext>(name_, xbox_address_, select_thread_);
  select_thread_->Start();
}

void XBOXInterface::Stop() {
  if (select_thread_) {
    select_thread_->Stop();
    select_thread_.reset();
  }

  if (xbdm_context_) {
    xbdm_context_->Shutdown();
    xbdm_context_.reset();
  }
}

bool XBOXInterface::ReconnectXBDM() {
  if (!xbdm_context_) {
    return false;
  }
  return xbdm_context_->Reconnect();
}

bool XBOXInterface::AttachDebugger() {
  if (!xbdm_debugger_) {
    xbdm_debugger_ = std::make_shared<XBDMDebugger>(xbdm_context_);
  }

  return xbdm_debugger_->Attach();
}

void XBOXInterface::DetachDebugger() {
  if (!xbdm_debugger_) {
    return;
  }

  xbdm_debugger_->Shutdown();
  xbdm_debugger_.reset();
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

bool XBOXInterface::GetGDBListenAddress(IPAddress& ret) const {
  if (!gdb_server_) {
    return false;
  }

  ret = gdb_server_->Address();
  return true;
}

bool XBOXInterface::StartNotificationListener(const IPAddress& address) {
  if (!xbdm_context_) {
    return false;
  }
  return xbdm_context_->StartNotificationListener(address);
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
  assert(xbdm_context_);
  return xbdm_context_->SendCommandSync(std::move(command));
}

std::future<std::shared_ptr<RDCPProcessedRequest>> XBOXInterface::SendCommand(
    std::shared_ptr<RDCPProcessedRequest> command) {
  assert(xbdm_context_);
  return xbdm_context_->SendCommand(std::move(command));
}
