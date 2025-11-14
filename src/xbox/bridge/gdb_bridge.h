#ifndef XBDM_GDB_BRIDGE_GDB_BRIDGE_H
#define XBDM_GDB_BRIDGE_GDB_BRIDGE_H

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

class GDBPacket;
class GDBTransport;
class NotificationExecutionStateChanged;
class NotificationBreakpoint;
class NotificationWatchpoint;
class NotificationSingleStep;
class NotificationException;
class Thread;
class XBDMContext;
class XBDMDebugger;
class XBDMNotification;

class GDBBridge {
 public:
  enum BreakpointType {
    BP_INVALID = -1,
    BP_SOFTWARE = 0,
    BP_HARDWARE,
    BP_WRITE,
    BP_READ,
    BP_ACCESS,
  };

 public:
  explicit GDBBridge(std::shared_ptr<XBDMContext> xbdm_context,
                     std::shared_ptr<XBDMDebugger> debugger);

  void Stop();

  bool HandlePacket(const GDBPacket& packet);

  bool AddTransport(std::shared_ptr<GDBTransport> transport);
  [[nodiscard]] bool HasGDBClient() const;

 private:
  void HandleDeprecatedCommand(const GDBPacket& packet);

  void HandleInterruptRequest(const GDBPacket& packet);
  void HandleEnableExtendedMode(const GDBPacket& packet);
  void HandleQueryHaltReason(const GDBPacket& packet);
  void HandleArgv(const GDBPacket& packet);
  void HandleBCommandGroup(const GDBPacket& packet);
  void HandleBackwardContinue(const GDBPacket& packet);
  void HandleBackwardStep(const GDBPacket& packet);
  void HandleContinue(const GDBPacket& packet);
  void HandleContinueWithSignal(const GDBPacket& packet);
  void HandleDetach(const GDBPacket& packet);
  void HandleFileIO(const GDBPacket& packet);
  void HandleReadGeneralRegisters(const GDBPacket& packet);
  void HandleWriteGeneralRegisters(const GDBPacket& packet);
  void HandleSelectThreadForCommandGroup(const GDBPacket& packet);
  void HandleStepInstruction(const GDBPacket& packet);
  void HandleSignalStep(const GDBPacket& packet);
  void HandleKill(const GDBPacket& packet);
  void HandleReadMemory(const GDBPacket& packet);
  void HandleWriteMemory(const GDBPacket& packet);
  void HandleReadRegister(const GDBPacket& packet);
  void HandleWriteRegister(const GDBPacket& packet);
  void HandleReadQuery(const GDBPacket& packet);
  void HandleWriteQuery(const GDBPacket& packet);
  void HandleRestartSystem(const GDBPacket& packet);
  void HandleSingleStep(const GDBPacket& packet);
  void HandleSingleStepWithSignal(const GDBPacket& packet);
  void HandleSearchBackward(const GDBPacket& packet);
  void HandleCheckThreadAlive(const GDBPacket& packet);
  void HandleExtendedVCommand(const GDBPacket& packet);
  void HandleWriteMemoryBinary(const GDBPacket& packet);
  void HandleRemoveBreakpointType(const GDBPacket& packet);
  void HandleInsertBreakpointType(const GDBPacket& packet);

  void SendOK() const;
  void SendEmpty() const;
  void SendError(uint8_t error) const;

  bool SendThreadStopPacket(const std::shared_ptr<Thread>& thread);
  [[nodiscard]] int32_t GetThreadIDForCommand(char command) const;
  [[nodiscard]] std::shared_ptr<Thread> GetThreadForCommand(char command) const;

  void HandleQueryAttached(const GDBPacket& packet);
  void HandleQuerySupported(const GDBPacket& packet);
  void HandleQueryThreadExtraInfo(const GDBPacket& packet);
  void HandleThreadInfoStart();
  void HandleThreadInfoContinue();
  void SendThreadInfoBuffer(bool send_all = true);
  void HandleQueryTraceStatus();
  void HandleQueryCurrentThreadID();
  void HandleFeaturesRead(const GDBPacket& packet);

  void HandleVContQuery();
  void HandleVCont(const std::string& args);

  void OnNotification(const std::shared_ptr<XBDMNotification>&);
  void OnExecutionStateChanged(
      const std::shared_ptr<NotificationExecutionStateChanged>&);
  void OnBreakpoint(const std::shared_ptr<NotificationBreakpoint>&);
  void OnWatchpoint(const std::shared_ptr<NotificationWatchpoint>&);
  void OnSingleStep(const std::shared_ptr<NotificationSingleStep>&);
  void OnException(const std::shared_ptr<NotificationException>&);

  void MarkWaitingForStopPacket();

 private:
  std::shared_ptr<GDBTransport> gdb_;
  std::shared_ptr<XBDMDebugger> debugger_;
  std::shared_ptr<XBDMContext> xbdm_;

  std::map<char, int> thread_id_for_command_;

  std::vector<int32_t> thread_info_buffer_;

  int notification_handler_id_{0};

  bool send_thread_events_{false};
  std::atomic<bool> waiting_on_stop_packet_{false};
};

#endif  // XBDM_GDB_BRIDGE_GDB_BRIDGE_H
