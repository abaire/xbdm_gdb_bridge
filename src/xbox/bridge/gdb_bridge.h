#ifndef XBDM_GDB_BRIDGE_GDB_BRIDGE_H
#define XBDM_GDB_BRIDGE_GDB_BRIDGE_H

#include <cstdint>
#include <memory>

class GDBPacket;
class GDBTransport;
class Thread;
class XBDMContext;
class XBDMDebugger;

class GDBBridge {
 public:
  explicit GDBBridge(std::shared_ptr<XBDMContext> xbdm_context,
                     std::shared_ptr<XBDMDebugger> debugger);

  void Stop();

  bool HandlePacket(const GDBPacket &packet);

  bool AddTransport(std::shared_ptr<GDBTransport> transport);
  [[nodiscard]] bool HasGDBClient() const;

 private:
  void HandleDeprecatedCommand(const GDBPacket &packet);

  void HandleInterruptRequest(const GDBPacket &packet);
  void HandleEnableExtendedMode(const GDBPacket &packet);
  void HandleQueryHaltReason(const GDBPacket &packet);
  void HandleArgv(const GDBPacket &packet);
  void HandleBCommandGroup(const GDBPacket &packet);
  void HandleBackwardContinue(const GDBPacket &packet);
  void HandleBackwardStep(const GDBPacket &packet);
  void HandleContinue(const GDBPacket &packet);
  void HandleContinueWithSignal(const GDBPacket &packet);
  void HandleDetach(const GDBPacket &packet);
  void HandleFileIO(const GDBPacket &packet);
  void HandleReadGeneralRegisters(const GDBPacket &packet);
  void HandleWriteGeneralRegisters(const GDBPacket &packet);
  void HandleSelectThreadForCommandGroup(const GDBPacket &packet);
  void HandleStepInstruction(const GDBPacket &packet);
  void HandleSignalStep(const GDBPacket &packet);
  void HandleKill(const GDBPacket &packet);
  void HandleReadMemory(const GDBPacket &packet);
  void HandleWriteMemory(const GDBPacket &packet);
  void HandleReadRegister(const GDBPacket &packet);
  void HandleWriteRegister(const GDBPacket &packet);
  void HandleReadQuery(const GDBPacket &packet);
  void HandleWriteQuery(const GDBPacket &packet);
  void HandleRestartSystem(const GDBPacket &packet);
  void HandleSingleStep(const GDBPacket &packet);
  void HandleSingleStepWithSignal(const GDBPacket &packet);
  void HandleSearchBackward(const GDBPacket &packet);
  void HandleCheckThreadAlive(const GDBPacket &packet);
  void HandleExtendedVCommand(const GDBPacket &packet);
  void HandleWriteMemoryBinary(const GDBPacket &packet);
  void HandleRemoveBreakpointType(const GDBPacket &packet);
  void HandleInsertBreakpointType(const GDBPacket &packet);

  void SendOK() const;
  void SendEmpty() const;
  void SendError(uint8_t error) const;

  bool SendThreadStopPacket(std::shared_ptr<Thread> thread);

 private:
  std::shared_ptr<GDBTransport> gdb_;
  std::shared_ptr<XBDMDebugger> debugger_;
  std::shared_ptr<XBDMContext> xbdm_;
};

#endif  // XBDM_GDB_BRIDGE_GDB_BRIDGE_H
