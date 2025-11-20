#include "xbox_interface.h"

#include <unistd.h>

#include <cassert>
#include <utility>

#include "net/select_thread.h"
#include "util/logging.h"
#include "xbox/xbdm_context.h"

XBOXInterface::XBOXInterface(std::string name, IPAddress xbox_address)
    : name_(std::move(name)), xbox_address_(std::move(xbox_address)) {}

void XBOXInterface::Start() {
  Stop();

  select_thread_ = std::make_shared<SelectThread>("ST_XboxIface");
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

void XBOXInterface::AwaitQuiescence() {
  assert(select_thread_ && "May not be called when not running");
  select_thread_->AwaitQuiescence();
}

bool XBOXInterface::ReconnectXBDM() {
  if (!xbdm_context_) {
    return false;
  }
  return xbdm_context_->Reconnect();
}

bool XBOXInterface::StartNotificationListener(const IPAddress& address) {
  if (!xbdm_context_) {
    return false;
  }
  return xbdm_context_->StartNotificationListener(address);
}

std::shared_ptr<RDCPProcessedRequest> XBOXInterface::SendCommandSync(
    const std::shared_ptr<RDCPProcessedRequest>& command) {
  assert(xbdm_context_);
  return xbdm_context_->SendCommandSync(command);
}

std::future<std::shared_ptr<RDCPProcessedRequest>> XBOXInterface::SendCommand(
    const std::shared_ptr<RDCPProcessedRequest>& command) {
  assert(xbdm_context_);
  return xbdm_context_->SendCommand(command);
}

std::shared_ptr<RDCPProcessedRequest> XBOXInterface::SendCommandSync(
    const std::shared_ptr<RDCPProcessedRequest>& command,
    const std::string& dedicated_handler) {
  assert(xbdm_context_);
  return xbdm_context_->SendCommandSync(command, dedicated_handler);
}

std::future<std::shared_ptr<RDCPProcessedRequest>> XBOXInterface::SendCommand(
    const std::shared_ptr<RDCPProcessedRequest>& command,
    const std::string& dedicated_handler) {
  assert(xbdm_context_);
  return xbdm_context_->SendCommand(command, dedicated_handler);
}
