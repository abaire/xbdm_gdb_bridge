#ifndef XBDM_GDB_BRIDGE_MOCK_XBDM_SERVER_H
#define XBDM_GDB_BRIDGE_MOCK_XBDM_SERVER_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "mock_xbox_state.h"
#include "net/ip_address.h"
#include "rdcp/rdcp_status_code.h"

class DelegatingServer;
class SelectThread;

namespace xbdm_gdb_bridge::testing {

class ClientTransport;

using CommandHandler =
    std::function<bool(ClientTransport&, const std::string& params)>;

enum class DebugEventType {
  kBreakpoint,
  kException,
  kModuleLoad,
  kModuleUnload,
  kThreadCreate,
  kThreadExit,
  kDebugString
};

struct DebugEvent {
  DebugEventType type;
  uint32_t thread_id;
  uint32_t address;
  std::string message;
};

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

  void AddBreakpoint(uint32_t address, bool hardware = false);
  void RemoveBreakpoint(uint32_t address);
  bool HasBreakpoint(uint32_t address) const;

  void AddModule(const std::string& name, uint32_t base_address, uint32_t size);
  void RemoveModule(const std::string& name);

  void SetExecutionState(bool running) {
    const std::lock_guard lock(state_mutex_);
    state_.execution_running = running;
  }
  bool IsExecutionRunning() const {
    const std::lock_guard lock(state_mutex_);
    return state_.execution_running;
  }

  void TriggerBreakpoint(uint32_t thread_id, uint32_t address);
  void TriggerException(uint32_t thread_id, uint32_t exception_code,
                        uint32_t address);
  void SendDebugString(const std::string& message);

  void SetCommandHandler(const std::string& command, CommandHandler handler);
  void RemoveCommandHandler(const std::string& command);

 private:
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

  bool ProcessCommand(ClientTransport& client, const std::string& params);

  bool HandleNotifyAt(ClientTransport& client, const std::string& params);
  bool HandleDebugger(ClientTransport& client, const std::string& params);
  bool HandleThreads(ClientTransport& client, const std::string& params);
  bool HandleThreadInfo(ClientTransport& client, const std::string& params);
  bool HandleModules(ClientTransport& client, const std::string& params);
  bool HandleWalkMemory(ClientTransport& client, const std::string& params);

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

  bool notifications_enabled_ = false;

  mutable std::mutex events_mutex_;
  std::condition_variable events_cv_;
  std::vector<DebugEvent> pending_events_;
};

}  // namespace xbdm_gdb_bridge::testing

#endif  // XBDM_GDB_BRIDGE_MOCK_XBDM_SERVER_H
