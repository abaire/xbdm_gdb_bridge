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

  bool StartNotificationListener(const IPAddress &address);

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
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_XBOX_INTERFACE_H_
