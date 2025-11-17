#ifndef XBDM_GDB_BRIDGE_GDB_XBOX_INTERFACE_H
#define XBDM_GDB_BRIDGE_GDB_XBOX_INTERFACE_H

#include <boost/asio/thread_pool.hpp>
#include <future>
#include <memory>
#include <string>
#include <utility>

#include "gdb/gdb_packet.h"
#include "net/ip_address.h"
#include "xbox/debugger/debugger_xbox_interface.h"

class GDBBridge;
class GDBPacket;

#define GET_GDBXBOXINTERFACE(__xboxinterface_var, __cast_var)          \
  auto* __gdb_xbox_interface =                                         \
      dynamic_cast<GDBXBOXInterface*>(&__xboxinterface_var);           \
  assert(__gdb_xbox_interface && "Interface is not GDBXBOXInterface"); \
  GDBXBOXInterface& __cast_var = *__gdb_xbox_interface

//! Provides various functions to interface with a remote XBDM processor.
class GDBXBOXInterface : public DebuggerXBOXInterface {
 public:
  GDBXBOXInterface(std::string name, IPAddress xbox_address)
      : DebuggerXBOXInterface(std::move(name), std::move(xbox_address)) {}

  bool StartGDBServer(const IPAddress& address);
  void StopGDBServer();
  bool GetGDBListenAddress(IPAddress& ret) const;

  //! Sets the XBOX path of a target to be launched the first time a GDB
  //! debugger connects to the GDB server.
  //!
  //! This prevents timing issues where a launch may be attempting to reboot the
  //! XBOX at the same time as the debugger is attempting to halt and retrieve
  //! thread information.
  inline void SetGDBLaunchTarget(const std::string& path) {
    gdb_launch_target_ = path;
  }

  //! Clears a previously set post-connect launch target.
  inline void ClearGDBLaunchTarget() { gdb_launch_target_.clear(); }

 private:
  void OnGDBClientConnected(int sock, IPAddress& address);
  void OnGDBPacketReceived(const std::shared_ptr<GDBPacket>& packet);

  void DispatchGDBPacket(const std::shared_ptr<GDBPacket>& packet);

 private:
  std::shared_ptr<DelegatingServer> gdb_server_;
  std::shared_ptr<GDBBridge> gdb_bridge_;
  std::shared_ptr<boost::asio::thread_pool> gdb_executor_;
  std::string gdb_launch_target_;
};

#endif  // XBDM_GDB_BRIDGE_GDB_XBOX_INTERFACE_H
