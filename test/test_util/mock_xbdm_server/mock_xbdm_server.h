#ifndef XBDM_GDB_BRIDGE_MOCK_XBDM_SERVER_H
#define XBDM_GDB_BRIDGE_MOCK_XBDM_SERVER_H

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "mock_xbox_state.h"
#include "net/ip_address.h"
#include "net/task_connection.h"
#include "rdcp/rdcp_status_code.h"

class TCPConnection;
class DelegatingServer;
class SelectThread;

namespace xbdm_gdb_bridge::testing {

class ClientTransport;

/**
 * Callback for custom commands.
 *
 * @returns false if the ClientTransport channel should be closed.
 */
using CommandHandler =
    std::function<bool(ClientTransport&, const std::string& params)>;

using ExecutionStateHandler = std::function<void()>;

/**
 * Callback to be invoked after a command is processed. Receives the argument
 * string passed to the rdcp command.
 */
using AfterCommandHandler = std::function<void(const std::string&)>;

/**
 * Simulates an Xbox running XBDM for testing purposes
 */
class MockXBDMServer {
 public:
  explicit MockXBDMServer(uint16_t port = 0);
  ~MockXBDMServer();

  bool Start();
  void Stop();
  bool IsRunning() const { return running_; }

  const IPAddress& GetAddress() const;

  void SetXboxName(const std::string& name) {
    const std::lock_guard lock(state_mutex_);
    state_.xbox_name = name;
  }
  void SetXboxVersion(const std::string& version) {
    const std::lock_guard lock(state_mutex_);
    state_.xbox_version = version;
  }

  void SetMemoryRegion(uint32_t address, const std::vector<uint8_t>& data);
  void ClearMemoryRegion(uint32_t address);
  std::vector<uint8_t> GetMemoryRegion(uint32_t address, size_t length);

  uint32_t AddThread(const std::string& name, uint32_t eip = 0x80000000);
  void RemoveThread(uint32_t thread_id);
  void SetThreadRegister(uint32_t thread_id, const std::string& reg_name,
                         uint32_t value);
  void SuspendThread(uint32_t thread_id);
  void ResumeThread(uint32_t thread_id);

  void AddBreakpoint(uint32_t address,
                     Breakpoint::Type type = Breakpoint::Type::EXECUTE);
  void RemoveBreakpoint(uint32_t address);
  bool HasBreakpoint(uint32_t address) const;

  void AddModule(const std::string& name, uint32_t base_address, uint32_t size);
  void RemoveModule(const std::string& name);

  void AddXbeSection(const std::string& name, uint32_t base_address,
                     uint32_t size, uint32_t index, uint32_t flags = 1);
  void RemoveXbeSection(const std::string& name);

  void AddRegion(uint32_t base_address, uint32_t size, uint32_t protect);
  void RemoveRegion(uint32_t base_address);

  /**
   * Sets the simulated execution state, possibly spawning notifications.
   * @param state the new state
   * @return The previous execution state.
   */
  ExecutionState SetExecutionState(ExecutionState state);

  /**
   * Registers a custom command handler. This will replace any built in handler
   * for the given command.
   *
   * @param command The base RDCP message string that should be handled. E.g.,
   *                "walkmem"
   * @param handler The handler to execute.
   */
  void SetCommandHandler(const std::string& command, CommandHandler handler);
  void RemoveCommandHandler(const std::string& command);

  /**
   * Registers a callback to be invoked when the ExecutionState becomes `state`.
   *
   * @returns An opaque token to be used with RemoveExecutionStateCallback.
   */
  int SetExecutionStateCallback(ExecutionState state,
                                ExecutionStateHandler handler);
  void RemoveExecutionStateCallback(int token);

  /**
   * Registers a callback to be invoked after the server has responded to the
   * given command. Note that there may still be pending asynchronous state
   * changes at the time this handler is invoked.
   *
   * @param command The base RDCP message string, e.g., "walkmem"
   * @param handler The handler to execute.
   */
  void SetAfterCommandHandler(const std::string& command,
                              AfterCommandHandler handler);
  void RemoveAfterCommandHandler(const std::string& command);

  void OnClientConnected(int sock, IPAddress& address);
  void OnClientBytesReceived(ClientTransport& transport);

  void SendResponse(std::shared_ptr<ClientTransport> transport,
                    StatusCode code) const {
    SendResponse(*transport, code);
  }
  void SendResponse(std::shared_ptr<ClientTransport> transport, StatusCode code,
                    const std::string& message) const {
    SendResponse(*transport, code, message);
  }
  void SendResponse(ClientTransport& transport, StatusCode code) const;
  void SendResponse(ClientTransport& transport, StatusCode code,
                    const std::string& message) const;

  void SendTerminator(ClientTransport& transport) const {
    SendString(transport, "\r\n");
  }

  /** Sends the given string to the client with no additional processing */
  void SendString(ClientTransport& transport, const std::string& str) const;
  /** Sends a string with no additional processing, then a \r\n terminator. */
  void SendStringWithTerminator(ClientTransport& transport,
                                const std::string& str) const {
    SendString(transport, str);
    SendTerminator(transport);
  }
  /** Sends the termination sequence ending a multiline response. */
  void SendMultilineTerminator(ClientTransport& transport) const {
    SendStringWithTerminator(transport, ".");
  }

  void SendKeyRawValue(ClientTransport& transport, const std::string& key,
                       const std::string& value, bool leading_space) const {
    if (leading_space) {
      SendString(transport, " ");
    }
    SendString(transport, key);
    SendString(transport, "=");
    SendString(transport, value);
  }

  void SendKeyValue(ClientTransport& transport, const std::string& key,
                    const std::string& value,
                    bool leading_space = false) const {
    std::string quoted_value = "\"";
    quoted_value += value;
    quoted_value += "\"";

    SendKeyRawValue(transport, key, quoted_value, leading_space);
  }

  void SendKeyValue(ClientTransport& transport, const std::string& key,
                    bool value, bool leading_space = false) const {
    SendKeyRawValue(transport, key, value ? "1" : "0", leading_space);
  }
  void SendKeyValue(ClientTransport& transport, const std::string& key,
                    uint32_t value, bool leading_space = false) const;
  void SendKeyHexValue(ClientTransport& transport, const std::string& key,
                       uint32_t value, bool leading_space = false) const;

  /**
   * Posts notifications simulating a breakpoint being hit within the given
   * thread.
   *
   * @return true if the notifications were posted
   */
  bool SimulateExecutionBreakpoint(uint32_t address, uint32_t thread_id = 0);

 private:
  bool ProcessCommand(ClientTransport& client, const std::string& command,
                      const std::string& params);

  bool HandleBreak(ClientTransport& client, const std::string& parameters);
  bool HandleBye(ClientTransport& client, const std::string& parameters);
  bool HandleContinue(ClientTransport& client, const std::string& parameters);
  bool HandleDebugger(ClientTransport& client, const std::string& parameters);
  bool HandleGo(ClientTransport& client, const std::string& parameters);
  bool HandleIsStopped(ClientTransport& client, const std::string& parameters);
  bool HandleModules(ClientTransport& client, const std::string& parameters);
  bool HandleNoStopOn(ClientTransport& client, const std::string& parameters);
  bool HandleNotifyAt(ClientTransport& client, const std::string& parameters);
  bool HandleReboot(ClientTransport& client, const std::string& parameters);
  bool HandleResume(ClientTransport& client, const std::string& parameters);
  bool HandleStopOn(ClientTransport& client, const std::string& parameters);
  bool HandleSuspend(ClientTransport& client, const std::string& parameters);
  bool HandleThreadInfo(ClientTransport& client, const std::string& parameters);
  bool HandleThreads(ClientTransport& client, const std::string& parameters);
  bool HandleTitle(ClientTransport& client, const std::string& parameters);
  bool HandleWalkMemory(ClientTransport& client, const std::string& parameters);

  /**
   * Invokes the given callback on each registered ClientTransport while locked.
   * @param f callback to invoke. Returning false will mark the client for
   *          removal.
   */
  void ForEachClient(std::function<bool(ClientTransport&)> f);

  std::string GetExecutionStateNotificationMessage() const;

  void PerformReboot();

  void ReconnectNotificationChannels();

  void SendNotification(const std::string& message) const;
  void SendNotificationAndClose(const std::string& message);

  /**
   * Sends the given notification on the next run of the select loop.
   */
  void PostNotification(const std::string& message);

  void PostModuleLoadNotification(const Module& module);
  void PostSectionLoadNotification(const XbeSection& section);
  void PostThreadCreateNotification(const SimulatedThread& section);

  /**
   * Advances the execution phase of the emulated Xbox.
   */
  void AdvancePhase();

  void AdvancePhaseStart();
  void AdvancePhaseLoadModules();
  void AdvancePhaseLoadSections();
  void AdvancePhaseStartFirstThread();
  void AdvancePhaseStartThreads();
  void AdvancePhaseRunning();

 private:
  uint16_t port_;

  bool accept_client_connections_{true};

  std::shared_ptr<SelectThread> select_thread_;
  std::shared_ptr<DelegatingServer> server_;

  std::atomic<bool> running_{false};
  std::recursive_mutex clients_mutex_;
  std::vector<std::shared_ptr<ClientTransport>> clients_;

  mutable std::recursive_mutex state_mutex_;
  MockXboxState state_;

  std::map<std::string, CommandHandler> custom_handlers_;

  mutable std::recursive_mutex execution_state_handlers_mutex_;
  std::map<int, std::pair<ExecutionState, ExecutionStateHandler>>
      execution_state_handlers_;
  int next_execution_state_handler_id_{1};

  mutable std::recursive_mutex after_handlers_mutex_;
  std::map<std::string, AfterCommandHandler> after_handlers_;

  std::shared_ptr<TaskConnection> task_queue_;

  mutable std::recursive_mutex notification_mutex_;
  // Maps <ClientConnectionName, NotificationIPAddress> to TCPConnections
  std::map<std::pair<std::string, IPAddress>, std::shared_ptr<TCPConnection>>
      notification_connections_;
};

}  // namespace xbdm_gdb_bridge::testing

#endif  // XBDM_GDB_BRIDGE_MOCK_XBDM_SERVER_H
