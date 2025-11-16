#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_XBOX_INTERFACE_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_XBOX_INTERFACE_H_

#include <future>
#include <memory>
#include <string>

#include "net/ip_address.h"

class DelegatingServer;
class RDCPProcessedRequest;
class SelectThread;
class XBDMContext;

//! Provides various functions to interface with a remote XBDM processor.
class XBOXInterface {
 public:
  XBOXInterface(std::string name, IPAddress xbox_address);
  virtual ~XBOXInterface() = default;

  void Start();
  void Stop();

  bool ReconnectXBDM();

  [[nodiscard]] std::shared_ptr<XBDMContext> Context() const {
    return xbdm_context_;
  }

  bool StartNotificationListener(const IPAddress& address);

  std::shared_ptr<RDCPProcessedRequest> SendCommandSync(
      const std::shared_ptr<RDCPProcessedRequest>& command);
  std::future<std::shared_ptr<RDCPProcessedRequest>> SendCommand(
      const std::shared_ptr<RDCPProcessedRequest>& command);

  std::shared_ptr<RDCPProcessedRequest> SendCommandSync(
      const std::shared_ptr<RDCPProcessedRequest>& command,
      const std::string& dedicated_handler);
  std::future<std::shared_ptr<RDCPProcessedRequest>> SendCommand(
      const std::shared_ptr<RDCPProcessedRequest>& command,
      const std::string& dedicated_handler);

 protected:
  std::string name_;
  IPAddress xbox_address_;

  std::shared_ptr<SelectThread> select_thread_;
  std::shared_ptr<XBDMContext> xbdm_context_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_XBOX_INTERFACE_H_
