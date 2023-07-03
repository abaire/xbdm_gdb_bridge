#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_XBOX_INTERFACE_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_XBOX_INTERFACE_H_

#include <boost/asio/thread_pool.hpp>
#include <future>
#include <list>
#include <memory>
#include <string>

#include "gdb/gdb_packet.h"
#include "net/ip_address.h"

class DelegatingServer;
class GDBBridge;
class GDBPacket;
class RDCPProcessedRequest;
class SelectThread;
class XBDMContext;
class XBDMDebugger;

//! Provides various functions to interface with a remote XBDM processor.
class XBOXInterface {
 public:
  XBOXInterface(std::string name, IPAddress xbox_address);

  void Start();
  void Stop();

  bool ReconnectXBDM();

  bool AttachDebugger();
  void DetachDebugger();
  [[nodiscard]] std::shared_ptr<XBDMDebugger> Debugger() const {
    return xbdm_debugger_;
  }

  [[nodiscard]] std::shared_ptr<XBDMContext> Context() const {
    return xbdm_context_;
  }

  bool StartGDBServer(const IPAddress &address);
  void StopGDBServer();
  bool GetGDBListenAddress(IPAddress &ret) const;

  //! Sets the XBOX path of a target to be launched the first time a GDB
  //! debugger connects to the GDB server.
  //!
  //! This prevents timing issues where a launch may be attempting to reboot the
  //! XBOX at the same time as the debugger is attempting to halt and retrieve
  //! thread information.
  inline void SetGDBLaunchTarget(const std::string &path) {
    gdb_launch_target_ = path;
  }

  //! Clears a previously set post-connect launch target.
  inline void ClearGDBLaunchTarget() { gdb_launch_target_.clear(); }

  bool StartNotificationListener(const IPAddress &address);
  void AttachDebugNotificationHandler();
  void DetachDebugNotificationHandler();

  std::shared_ptr<RDCPProcessedRequest> SendCommandSync(
      std::shared_ptr<RDCPProcessedRequest> command);
  std::future<std::shared_ptr<RDCPProcessedRequest>> SendCommand(
      std::shared_ptr<RDCPProcessedRequest> command);

 private:
  void OnGDBClientConnected(int sock, IPAddress &address);
  void OnGDBPacketReceived(const std::shared_ptr<GDBPacket> &packet);

  void DispatchGDBPacket(const std::shared_ptr<GDBPacket> &packet);

 private:
  std::string name_;
  IPAddress xbox_address_;

  std::shared_ptr<SelectThread> select_thread_;
  std::shared_ptr<XBDMContext> xbdm_context_;
  std::shared_ptr<XBDMDebugger> xbdm_debugger_;

  std::shared_ptr<DelegatingServer> gdb_server_;
  std::shared_ptr<GDBBridge> gdb_bridge_;
  std::shared_ptr<boost::asio::thread_pool> gdb_executor_;
  std::string gdb_launch_target_;

  int debug_notification_handler_id_{0};
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_XBOX_INTERFACE_H_
