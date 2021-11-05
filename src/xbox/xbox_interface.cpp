#include "xbox_interface.h"

#include <unistd.h>

#include <boost/asio/dispatch.hpp>
#include <boost/log/trivial.hpp>
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

bool XBOXInterface::StartGDBServer(const IPAddress& address) {
  StopGDBServer();
  gdb_executor_ = std::make_shared<boost::asio::thread_pool>(1);

  gdb_server_ = std::make_shared<DelegatingServer>(
      name_, [this](int sock, IPAddress& address) {
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
  if (gdb_) {
    gdb_->Close();
    gdb_.reset();
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
  if (gdb_ && gdb_->IsConnected()) {
    BOOST_LOG_TRIVIAL(warning)
        << "Disallowing additional GDB connection from " << address;
    shutdown(sock, SHUT_RDWR);
    close(sock);
    return;
  }

  BOOST_LOG_TRIVIAL(trace) << "GDB channel established from " << address;
  // TODO: Hold on to the transport so it can be shut down gracefully.
  gdb_ = std::make_shared<GDBTransport>(
      name_, sock, address, [this](const std::shared_ptr<GDBPacket>& packet) {
        this->OnGDBPacketReceived(packet);
      });
  select_thread_->AddConnection(gdb_);
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
  switch (packet->Command()) {
    case 0x03:
      break;
    case '!':
      break;
    case '?':
      break;
    case 'A':
      break;
    case 'b':
      break;
    case 'B':
      break;
    case 'c':
      break;
    case 'C':
      break;
    case 'd':
      break;
    case 'D':
      break;
    case 'F':
      break;
    case 'g':
      break;
    case 'G':
      break;
    case 'H':
      break;
    case 'i':
      break;
    case 'I':
      break;
    case 'k':
      break;
    case 'm':
      break;
    case 'M':
      break;
    case 'p':
      break;
    case 'P':
      break;
    case 'q':
      break;
    case 'Q':
      break;
    case 'r':
      break;
    case 'R':
      break;
    case 's':
      break;
    case 'S':
      break;
    case 't':
      break;
    case 'T':
      break;
    case 'v':
      break;
    case 'X':
      break;
    case 'z':
      break;
    case 'Z':
      break;

    default:

      break;
  }

  // TODO: FINISH ME
  BOOST_LOG_TRIVIAL(error) << "IMPLEMENT ME";
  SendGDBEmpty();
}

void XBOXInterface::SendGDBOK() const {
  if (!gdb_ || !gdb_->IsConnected()) {
    return;
  }
  gdb_->Send(GDBPacket("OK"));
}

void XBOXInterface::SendGDBEmpty() const {
  if (!gdb_ || !gdb_->IsConnected()) {
    return;
  }
  gdb_->Send(GDBPacket());
}

void XBOXInterface::SendGDBError(int error) const {
  if (!gdb_ || !gdb_->IsConnected()) {
    return;
  }

  char buffer[4] = {0};
  snprintf(buffer, 3, "E%02x", error & 0xFF);
  gdb_->Send(GDBPacket(buffer));
}
