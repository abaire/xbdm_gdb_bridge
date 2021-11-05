#include "gdb_bridge.h"

#include <boost/log/trivial.hpp>
#include <cstdio>

#include "gdb/gdb_packet.h"
#include "gdb/gdb_transport.h"
#include "xbox/debugger/xbdm_debugger.h"

GDBBridge::GDBBridge(std::shared_ptr<XBDMContext> xbdm_context,
                     std::shared_ptr<XBDMDebugger> debugger)
    : xbdm_(std::move(xbdm_context)), debugger_(std::move(debugger)) {}

bool GDBBridge::AddTransport(std::shared_ptr<GDBTransport> transport) {
  if (HasGDBClient()) {
    return false;
  }
  gdb_ = std::move(transport);
  return true;
}

void GDBBridge::Stop() {
  if (HasGDBClient()) {
    gdb_->Close();
    gdb_.reset();
  }
}

bool GDBBridge::HasGDBClient() const { return gdb_ && gdb_->IsConnected(); }

bool GDBBridge::HandlePacket(const GDBPacket& packet) {
  switch (packet.Command()) {
    case 0x03:
      HandleInterruptRequest(packet);
      return true;

    case '!':
      HandleEnableExtendedMode(packet);
      return true;

    case '?':
      HandleQueryHaltReason(packet);
      return true;

    case 'A':
      HandleArgv(packet);
      return true;

    case 'b':
      HandleBCommandGroup(packet);
      return true;

    case 'c':
      HandleContinue(packet);
      return true;

    case 'C':
      HandleContinueWithSignal(packet);
      return true;

    case 'D':
      HandleDetach(packet);
      return true;

    case 'F':
      HandleFileIO(packet);
      return true;

    case 'g':
      HandleReadGeneralRegisters(packet);
      return true;

    case 'G':
      HandleWriteGeneralRegisters(packet);
      return true;

    case 'H':
      HandleSelectThreadForCommandGroup(packet);
      return true;

    case 'i':
      HandleStepInstruction(packet);
      return true;

    case 'I':
      HandleSignalStep(packet);
      return true;

    case 'k':
      HandleKill(packet);
      return true;

    case 'm':
      HandleReadMemory(packet);
      return true;

    case 'M':
      HandleWriteMemory(packet);
      return true;

    case 'p':
      HandleReadRegister(packet);
      return true;

    case 'P':
      HandleWriteRegister(packet);
      return true;

    case 'q':
      HandleReadQuery(packet);
      return true;

    case 'Q':
      HandleWriteQuery(packet);
      return true;

    case 'R':
      HandleRestartSystem(packet);
      return true;

    case 's':
      HandleSingleStep(packet);
      return true;

    case 'S':
      HandleSingleStepWithSignal(packet);
      return true;

    case 't':
      HandleSearchBackward(packet);
      return true;

    case 'T':
      HandleCheckThreadAlive(packet);
      return true;

    case 'v':
      HandleExtendedVCommand(packet);
      return true;

    case 'X':
      HandleWriteMemoryBinary(packet);
      return true;

    case 'z':
      HandleRemoveBreakpointType(packet);
      return true;

    case 'Z':
      HandleInsertBreakpointType(packet);
      return true;

    case 'B':
    case 'd':
    case 'r':
      HandleDeprecatedCommand(packet);
      return true;

    default:
      break;
  }

  // TODO: FINISH ME
  BOOST_LOG_TRIVIAL(error) << "IMPLEMENT ME";
  SendEmpty();
  return true;
}

void GDBBridge::SendOK() const {
  if (!gdb_ || !gdb_->IsConnected()) {
    return;
  }
  gdb_->Send(GDBPacket("OK"));
}

void GDBBridge::SendEmpty() const {
  if (!gdb_ || !gdb_->IsConnected()) {
    return;
  }
  gdb_->Send(GDBPacket());
}

void GDBBridge::SendError(uint8_t error) const {
  if (!gdb_ || !gdb_->IsConnected()) {
    return;
  }

  char buffer[8] = {0};
  snprintf(buffer, 4, "E%02x", error);
  gdb_->Send(GDBPacket(buffer));
}

void GDBBridge::HandleDeprecatedCommand(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(info) << "Ignoring deprecated command: "
                          << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleInterruptRequest(const GDBPacket& packet) {
  if (!debugger_->HaltAll()) {
    SendError(EBADMSG);
    return;
  }

  auto thread = debugger_->ActiveThread();
  assert(thread);
  SendThreadStopPacket(thread);
}

void GDBBridge::HandleEnableExtendedMode(const GDBPacket& packet) {}

void GDBBridge::HandleQueryHaltReason(const GDBPacket& packet) {}

void GDBBridge::HandleArgv(const GDBPacket& packet) {}

void GDBBridge::HandleBCommandGroup(const GDBPacket& packet) {
  char subcommand;
  if (!packet.GetFirstDataChar(subcommand)) {
    BOOST_LOG_TRIVIAL(error) << "Unexpected truncated b packet.";
    SendEmpty();
    return;
  }

  switch (subcommand) {
    case 'c':
      HandleBackwardContinue(packet);
      break;

    case 's':
      HandleBackwardStep(packet);
      break;

    default:
      HandleDeprecatedCommand(packet);
      break;
  }
}

void GDBBridge::HandleBackwardContinue(const GDBPacket& packet) {}

void GDBBridge::HandleBackwardStep(const GDBPacket& packet) {}

void GDBBridge::HandleContinue(const GDBPacket& packet) {}

void GDBBridge::HandleContinueWithSignal(const GDBPacket& packet) {}

void GDBBridge::HandleDetach(const GDBPacket& packet) {}

void GDBBridge::HandleFileIO(const GDBPacket& packet) {}

void GDBBridge::HandleReadGeneralRegisters(const GDBPacket& packet) {}

void GDBBridge::HandleWriteGeneralRegisters(const GDBPacket& packet) {}

void GDBBridge::HandleSelectThreadForCommandGroup(const GDBPacket& packet) {}

void GDBBridge::HandleStepInstruction(const GDBPacket& packet) {}

void GDBBridge::HandleSignalStep(const GDBPacket& packet) {}

void GDBBridge::HandleKill(const GDBPacket& packet) {}

void GDBBridge::HandleReadMemory(const GDBPacket& packet) {}

void GDBBridge::HandleWriteMemory(const GDBPacket& packet) {}

void GDBBridge::HandleReadRegister(const GDBPacket& packet) {}

void GDBBridge::HandleWriteRegister(const GDBPacket& packet) {}

void GDBBridge::HandleReadQuery(const GDBPacket& packet) {}

void GDBBridge::HandleWriteQuery(const GDBPacket& packet) {}

void GDBBridge::HandleRestartSystem(const GDBPacket& packet) {}

void GDBBridge::HandleSingleStep(const GDBPacket& packet) {}

void GDBBridge::HandleSingleStepWithSignal(const GDBPacket& packet) {}

void GDBBridge::HandleSearchBackward(const GDBPacket& packet) {}

void GDBBridge::HandleCheckThreadAlive(const GDBPacket& packet) {}

void GDBBridge::HandleExtendedVCommand(const GDBPacket& packet) {}

void GDBBridge::HandleWriteMemoryBinary(const GDBPacket& packet) {}

void GDBBridge::HandleRemoveBreakpointType(const GDBPacket& packet) {}

void GDBBridge::HandleInsertBreakpointType(const GDBPacket& packet) {}

bool GDBBridge::SendThreadStopPacket(std::shared_ptr<Thread> thread) {
  if (!gdb_ || !gdb_->IsConnected()) {
    return false;
  }

  if (!thread) {
    return false;
  }

  auto stop_reason = thread->last_stop_reason;
  if (not stop_reason) {
    return false;
  }

  // TODO: send detailed information.
  // see
  // https://sourceware.org/gdb/onlinedocs/gdb/Stop-Reply-Packets.html#Stop-Reply-Packets
  char buffer[32] = {0};
  snprintf(buffer, 31, "T%02xthread:%x;", stop_reason->signal,
           thread->thread_id);
  gdb_->Send(GDBPacket(buffer));
  return true;
}
