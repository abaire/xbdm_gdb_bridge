#ifndef XBDM_GDB_BRIDGE_XBDM_CONTEXT_H
#define XBDM_GDB_BRIDGE_XBDM_CONTEXT_H

#include <boost/asio/thread_pool.hpp>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

#include "net/ip_address.h"

class DelegatingServer;
class RDCPProcessedRequest;
class SelectThread;
class XBDMNotification;
class XBDMNotificationTransport;
class XBDMTransport;

class XBDMContext {
 public:
  typedef std::function<void(const std::shared_ptr<XBDMNotification> &,
                             XBDMContext &)>
      NotificationHandler;

 public:
  XBDMContext(std::string name, IPAddress xbox_address,
              std::shared_ptr<SelectThread> select_thread);

  //! Closes all connections.
  void Shutdown();

  //! Closes the XBDM transport socket (the port 731 stream) and any active
  //! notification streams from the Xbox.
  void CloseActiveConnections();

  //! Hard closes any outstanding notification streams.
  void ResetNotificationConnections();

  //! Drops the XBDM transport socket and immediately reconnects.
  bool Reconnect();

  bool StartNotificationListener(const IPAddress &address);
  bool GetNotificationServerAddress(IPAddress &address) const;

  int RegisterNotificationHandler(NotificationHandler);
  void UnregisterNotificationHandler(int);

  std::future<std::shared_ptr<RDCPProcessedRequest>> SendCommand(
      const std::shared_ptr<RDCPProcessedRequest> &command);
  std::shared_ptr<RDCPProcessedRequest> SendCommandSync(
      const std::shared_ptr<RDCPProcessedRequest> &command);
  std::future<std::shared_ptr<RDCPProcessedRequest>> SendCommand(
      const std::shared_ptr<RDCPProcessedRequest> &command,
      const std::string &dedicated_handler);
  std::shared_ptr<RDCPProcessedRequest> SendCommandSync(
      const std::shared_ptr<RDCPProcessedRequest> &command,
      const std::string &dedicated_handler);

  bool CreateDedicatedChannel(const std::string &command_handler);
  void DestroyDedicatedChannel(const std::string &command_handler);

 private:
  std::future<std::shared_ptr<RDCPProcessedRequest>> SendCommand(
      const std::shared_ptr<RDCPProcessedRequest> &command,
      const std::shared_ptr<XBDMTransport> &transport);
  std::shared_ptr<RDCPProcessedRequest> SendCommandSync(
      std::shared_ptr<RDCPProcessedRequest> command,
      const std::shared_ptr<XBDMTransport> &transport);

  void OnNotificationChannelConnected(int sock, IPAddress &address);
  void OnNotificationReceived(std::shared_ptr<XBDMNotification> notification);

  void ExecuteXBDMPromise(
      std::promise<std::shared_ptr<RDCPProcessedRequest>> &promise,
      const std::shared_ptr<RDCPProcessedRequest> &request,
      const std::shared_ptr<XBDMTransport> &transport);
  bool XBDMConnect(const std::shared_ptr<XBDMTransport> &transport,
                   int max_wait_millis = 5000);

  void DispatchNotification(
      const std::shared_ptr<XBDMNotification> &notification);

 private:
  std::string name_;
  IPAddress xbox_address_;

  std::shared_ptr<SelectThread> select_thread_;
  std::shared_ptr<XBDMTransport> xbdm_transport_;
  std::shared_ptr<DelegatingServer> notification_server_;

  //! Set of XBDMNotificationTransport instances managing notification streams
  //! from XBDM to this bridge.
  std::unordered_set<std::shared_ptr<XBDMNotificationTransport>>
      notification_transports_;
  std::recursive_mutex notification_transports_lock_;

  //! Map of command processor name to dedicated transport channel.
  std::map<std::string, std::shared_ptr<XBDMTransport>> dedicated_transports_;

  std::shared_ptr<boost::asio::thread_pool> xbdm_control_executor_;
  std::shared_ptr<boost::asio::thread_pool> notification_executor_;

  std::recursive_mutex notification_handler_lock_;
  int next_notification_handler_id_{1};
  std::map<int, NotificationHandler> notification_handlers_;
};

#endif  // XBDM_GDB_BRIDGE_XBDM_CONTEXT_H
