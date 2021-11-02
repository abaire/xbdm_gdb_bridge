#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_XBOX_INTERFACE_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_XBOX_INTERFACE_H_

#include <deque>
#include <list>
#include <memory>
#include <string>

#include "gdb/gdb_transport.h"
#include "net/delegating_server.h"
#include "net/ip_address.h"
#include "net/select_thread.h"
#include "rdcp/xbdm_transport.h"

class XBOXInterface {
 public:
  XBOXInterface(std::string name, IPAddress xbox_address);

  void Start();
  void Stop();

  bool ReconnectXBDM();
  void StartGDBServer(const IPAddress &address);
  void StopGDBServer();
  bool GetGDBListenAddress(IPAddress &ret) const {
    if (!gdb_server_) {
      return false;
    }

    ret = gdb_server_->Address();
    return true;
  }

 private:
  void OnNotificationChannelConnected(int sock, IPAddress &address);
  void OnNotificationReceived(XBDMNotification &notification);

  void OnGDBClientConnected(int sock, IPAddress &address);
  void OnGDBPacketReceived(GDBPacket &packet);

 private:
  std::string name_;
  IPAddress xbox_address_;

  std::shared_ptr<SelectThread> select_thread_;
  std::shared_ptr<XBDMTransport> xbdm_transport_;
  std::shared_ptr<GDBTransport> gdb_transport_;

  std::shared_ptr<DelegatingServer> gdb_server_;
  std::shared_ptr<DelegatingServer> notification_server_;

  std::mutex notification_queue_lock_;
  std::list<XBDMNotification> notification_queue_;

  std::mutex gdb_queue_lock_;
  std::list<GDBPacket> gdb_queue_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_XBOX_INTERFACE_H_
