#include "xbox_interface.h"

#include <unistd.h>

#include <boost/asio/dispatch.hpp>
#include <cassert>
#include <iostream>
#include <utility>

#include "gdb/gdb_transport.h"
#include "net/delegating_server.h"
#include "net/select_thread.h"
#include "notification/xbdm_notification.h"
#include "util/logging.h"
#include "util/timer.h"
#include "xbox/bridge/gdb_bridge.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"

constexpr int kMaxDebuggerAttachAttempts = 5;

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

bool XBOXInterface::StartGDBServer(const IPAddress& address) {
  StopGDBServer();
  gdb_executor_ = std::make_shared<boost::asio::thread_pool>(1);
  if (!xbdm_debugger_) {
    xbdm_debugger_ = std::make_shared<XBDMDebugger>(xbdm_context_);
  }
  gdb_bridge_ = std::make_shared<GDBBridge>(xbdm_context_, xbdm_debugger_);

  gdb_server_ = std::make_shared<DelegatingServer>(
      name_ + "__gdb_server", [this](int sock, IPAddress& address) {
        this->OnGDBClientConnected(sock, address);
      });
  select_thread_->AddConnection(gdb_server_);

  return gdb_server_->Listen(address);
}

void XBOXInterface::StopGDBServer() {
  if (gdb_server_) {
    gdb_server_->Close();
    gdb_server_.reset();
  }
  if (gdb_bridge_) {
    gdb_bridge_->Stop();
    gdb_bridge_.reset();
  }
  if (gdb_executor_) {
    gdb_executor_->stop();
    gdb_executor_->join();
    gdb_executor_.reset();
  }
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

void XBOXInterface::AttachDebugNotificationHandler() {
  if (debug_notification_handler_id_ > 0) {
    return;
  }

  debug_notification_handler_id_ = xbdm_context_->RegisterNotificationHandler(
      [](const std::shared_ptr<XBDMNotification>& notification) {
        std::cout << *notification << std::endl;
      });
}

void XBOXInterface::DetachDebugNotificationHandler() {
  if (debug_notification_handler_id_ <= 0) {
    return;
  }

  xbdm_context_->UnregisterNotificationHandler(debug_notification_handler_id_);
  debug_notification_handler_id_ = 0;
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

void XBOXInterface::OnGDBClientConnected(int sock, IPAddress& address) {
  assert(gdb_bridge_);
  if (gdb_bridge_->HasGDBClient()) {
    LOG_XBDM(warning) << "Disallowing additional GDB connection from "
                      << address;
    shutdown(sock, SHUT_RDWR);
    close(sock);
    return;
  }

  LOG_XBDM(trace) << "GDB channel established from " << address;
  auto transport = std::make_shared<GDBTransport>(
      "GDB", sock, address, [this](const std::shared_ptr<GDBPacket>& packet) {
        this->OnGDBPacketReceived(packet);
      });

  // Bounce to the executor so the select thread is not blocked by any attempt
  // to communicate with XBDM.
  assert(gdb_executor_);
  boost::asio::dispatch(*gdb_executor_, [this, transport]() mutable {
    if (!this->xbdm_debugger_->IsAttached()) {
      auto i = 0;
      for (; i < kMaxDebuggerAttachAttempts && !this->xbdm_debugger_->Attach();
           ++i) {
        LOG_XBDM(trace) << "GDB: Debugger attach failed. Attempt " << (i + 1);
        WaitMilliseconds(50);
      }
      if (i == kMaxDebuggerAttachAttempts) {
        LOG_XBDM(error) << "GDB: Failed to attach debugger.";
      } else {
        this->xbdm_debugger_->HaltAll();
      }
    }
    this->select_thread_->AddConnection(transport);
    this->gdb_bridge_->AddTransport(transport);
  });
}

void XBOXInterface::OnGDBPacketReceived(
    const std::shared_ptr<GDBPacket>& packet) {
  assert(gdb_executor_);
  boost::asio::dispatch(*gdb_executor_, [this, packet]() mutable {
    this->DispatchGDBPacket(packet);
  });
}

void XBOXInterface::DispatchGDBPacket(
    const std::shared_ptr<GDBPacket>& packet) {
  gdb_bridge_->HandlePacket(*packet);
}
