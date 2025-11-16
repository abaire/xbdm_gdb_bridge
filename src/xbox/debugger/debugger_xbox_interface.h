#ifndef XBDM_GDB_BRIDGE_DEBUGGER_XBOX_INTERFACE_H
#define XBDM_GDB_BRIDGE_DEBUGGER_XBOX_INTERFACE_H

#include <boost/asio/thread_pool.hpp>
#include <future>
#include <list>
#include <memory>
#include <string>

#include "net/ip_address.h"
#include "xbox/xbox_interface.h"

class DelegatingServer;
class RDCPProcessedRequest;
class SelectThread;
class XBDMContext;
class XBDMDebugger;

#define GET_DEBUGGERXBOXINTERFACE(__xboxinterface_var, __cast_var) \
  auto* __debugger_xbox_interface =                                \
      dynamic_cast<DebuggerXBOXInterface*>(&__xboxinterface_var);  \
  assert(__debugger_xbox_interface &&                              \
         "Interface is not DebuggerXBOXInterface");                \
  DebuggerXBOXInterface& __cast_var = *__debugger_xbox_interface

//! Provides various debugger functions to interface with a remote XBDM
//! processor.
class DebuggerXBOXInterface : public XBOXInterface {
 public:
  DebuggerXBOXInterface(std::string name, IPAddress xbox_address)
      : XBOXInterface(name, xbox_address) {}

  bool AttachDebugger();
  void DetachDebugger();
  [[nodiscard]] std::shared_ptr<XBDMDebugger> Debugger() const {
    return xbdm_debugger_;
  }

  void AttachDebugNotificationHandler();
  void DetachDebugNotificationHandler();

 protected:
  std::shared_ptr<XBDMDebugger> xbdm_debugger_;

  int debug_notification_handler_id_{0};
};

#endif  // XBDM_GDB_BRIDGE_DEBUGGER_XBOX_INTERFACE_H
