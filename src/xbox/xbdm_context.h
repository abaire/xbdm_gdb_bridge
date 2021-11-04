#ifndef XBDM_GDB_BRIDGE_XBDM_CONTEXT_H
#define XBDM_GDB_BRIDGE_XBDM_CONTEXT_H

#include <boost/asio/thread_pool.hpp>
#include <future>
#include <list>
#include <memory>
#include <mutex>

#include "net/ip_address.h"
#include "notification/xbdm_notification.h"

class DelegatingServer;
class RDCPProcessedRequest;
class SelectThread;
class XBDMTransport;

class XBDMContext {
 public:
  typedef std::function<void(const XBDMNotification &)> NotificationHandler;

 public:
  XBDMContext(std::string name, IPAddress xbox_address,
              std::shared_ptr<SelectThread> select_thread);

  void Shutdown();

  bool Reconnect();

  bool StartNotificationListener(const IPAddress &address);
  bool GetNotificationServerAddress(IPAddress &address) const;

  int RegisterNotificationHandler(NotificationHandler);
  void UnregisterNotificationHandler(int);

  std::shared_ptr<RDCPProcessedRequest> SendCommandSync(
      std::shared_ptr<RDCPProcessedRequest> command);
  std::future<std::shared_ptr<RDCPProcessedRequest>> SendCommand(
      std::shared_ptr<RDCPProcessedRequest> command);

 private:
  void OnNotificationChannelConnected(int sock, IPAddress &address);
  void OnNotificationReceived(XBDMNotification &notification);

  void ExecuteXBDMPromise(
      std::promise<std::shared_ptr<RDCPProcessedRequest>> &promise,
      std::shared_ptr<RDCPProcessedRequest> &request);
  bool XBDMConnect(int max_wait_millis = 5000);

 private:
  std::string name_;
  IPAddress xbox_address_;

  std::shared_ptr<SelectThread> select_thread_;
  std::shared_ptr<XBDMTransport> xbdm_transport_;
  std::shared_ptr<DelegatingServer> notification_server_;

  std::recursive_mutex notification_queue_lock_;
  std::list<XBDMNotification> notification_queue_;

  std::shared_ptr<boost::asio::thread_pool> xbdm_control_executor_;
  std::shared_ptr<boost::asio::thread_pool> notification_executor_;

  std::recursive_mutex notification_handler_lock_;
  int next_notification_handler_id_{1};
  std::map<int, NotificationHandler> notification_handlers_;
};

#endif  // XBDM_GDB_BRIDGE_XBDM_CONTEXT_H
