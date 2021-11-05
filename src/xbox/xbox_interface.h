#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_XBOX_INTERFACE_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_XBOX_INTERFACE_H_

#include <future>
#include <list>
#include <memory>
#include <string>

#include "gdb/gdb_packet.h"
#include "net/ip_address.h"

class DelegatingServer;
class GDBPacket;
class GDBTransport;
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

  void StartGDBServer(const IPAddress &address);
  void StopGDBServer();
  bool GetGDBListenAddress(IPAddress &ret) const;

  bool StartNotificationListener(const IPAddress &address);

  std::shared_ptr<RDCPProcessedRequest> SendCommandSync(
      std::shared_ptr<RDCPProcessedRequest> command);
  std::future<std::shared_ptr<RDCPProcessedRequest>> SendCommand(
      std::shared_ptr<RDCPProcessedRequest> command);

 private:
  void OnGDBClientConnected(int sock, IPAddress &address);
  void OnGDBPacketReceived(GDBPacket &packet);

 private:
  std::string name_;
  IPAddress xbox_address_;

  std::shared_ptr<SelectThread> select_thread_;
  std::shared_ptr<XBDMContext> xbdm_context_;
  std::shared_ptr<XBDMDebugger> xbdm_debugger_;
  std::shared_ptr<GDBTransport> gdb_transport_;
  std::shared_ptr<DelegatingServer> gdb_server_;
  std::recursive_mutex gdb_queue_lock_;
  std::list<GDBPacket> gdb_queue_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_XBOX_INTERFACE_H_
