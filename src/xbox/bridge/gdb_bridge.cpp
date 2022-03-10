// See
// https://sourceware.org/gdb/onlinedocs/gdb/Remote-Protocol.html#Remote-Protocol

#include "gdb_bridge.h"

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/log/trivial.hpp>
#include <cstdio>

#include "configure.h"
#include "gdb/gdb_packet.h"
#include "gdb/gdb_transport.h"
#include "gdb_registers.h"
#include "notification/xbdm_notification.h"
#include "util/logging.h"
#include "util/parsing.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"

GDBBridge::GDBBridge(std::shared_ptr<XBDMContext> xbdm_context,
                     std::shared_ptr<XBDMDebugger> debugger)
    : xbdm_(std::move(xbdm_context)), debugger_(std::move(debugger)) {}

bool GDBBridge::AddTransport(std::shared_ptr<GDBTransport> transport) {
  if (HasGDBClient()) {
    return false;
  }
  gdb_ = std::move(transport);

  // TODO: Either chain notifications off the debugger or add "go last" flag.
  // Currently the debugger receives the same notifications as the bridge and
  // does some work to update thread states/etc... This class assumes this has
  // all happened by the time it receives notifications, but it is not enforced
  // anywhere. The context could be extended to allow a "last" handler or the
  // debugger could hold a weak_ptr to a notification listener (this class) to
  // be notified once it has finished processing notifications from the context.
  xbdm_->UnregisterNotificationHandler(notification_handler_id_);
  notification_handler_id_ = xbdm_->RegisterNotificationHandler(
      [this](const std::shared_ptr<XBDMNotification>& notification) {
        this->OnNotification(notification);
      });

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
#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
  LOG_GDB(trace) << "Received packet: " << packet.DataString();
#endif

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
  LOG_GDB(error) << "IMPLEMENT ME";
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

#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
  LOG_GDB(error) << "Sending error response " << std::hex << error;
#endif

  char buffer[8] = {0};
  snprintf(buffer, 4, "E%02x", error);
  gdb_->Send(GDBPacket(buffer));
}

void GDBBridge::HandleDeprecatedCommand(const GDBPacket& packet) {
  LOG_GDB(info) << "Ignoring deprecated command: " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleInterruptRequest(const GDBPacket& packet) {
#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
  LOG_GDB(trace) << "Processing GDB interrupt request";
#endif

  if (!debugger_->Stop()) {
    LOG_GDB(error) << "Failed to stop on GDB interrupt request";
    SendError(EBADMSG);
    return;
  }

  if (!debugger_->HaltAll()) {
    LOG_GDB(error) << "Failed to halt on GDB interrupt request";
    SendError(EBADMSG);

    if (!debugger_->Go()) {
      LOG_GDB(error) << "Failed to Go after failing to halt all";
    }
    return;
  }

  auto thread = debugger_->ActiveThread();
  assert(thread);
  if (!SendThreadStopPacket(thread)) {
    LOG_GDB(error) << "Failed to send stop reason on GDB interrupt request";
    SendOK();
  }
}

void GDBBridge::HandleEnableExtendedMode(const GDBPacket& packet) {
  LOG_GDB(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleQueryHaltReason(const GDBPacket& packet) {
  auto thread = debugger_->GetFirstStoppedThread();
  if (!thread || !thread->stopped) {
    SendOK();
    return;
  }

  if (!SendThreadStopPacket(thread)) {
    SendEmpty();
  }
}

void GDBBridge::HandleArgv(const GDBPacket& packet) {
  LOG_GDB(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleBCommandGroup(const GDBPacket& packet) {
  char subcommand;
  if (!packet.GetFirstDataChar(subcommand)) {
    LOG_GDB(error) << "Unexpected truncated b packet.";
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

void GDBBridge::HandleBackwardContinue(const GDBPacket& packet) {
  LOG_GDB(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleBackwardStep(const GDBPacket& packet) {
  LOG_GDB(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleContinue(const GDBPacket& packet) {
  LOG_GDB(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleContinueWithSignal(const GDBPacket& packet) {
  LOG_GDB(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleDetach(const GDBPacket& packet) {
  debugger_->ContinueAll(true);
  if (!debugger_->Go()) {
    LOG_GDB(error) << "Go failed during debugger detach.";
  }
  SendOK();
}

void GDBBridge::HandleFileIO(const GDBPacket& packet) {
  LOG_GDB(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleReadGeneralRegisters(const GDBPacket& packet) {
  int32_t thread_id = GetThreadIDForCommand(packet.Command());
  if (thread_id < 0) {
    LOG_GDB(error)
        << "Unsupported read general registers query for all threads: "
        << packet.DataString();
    SendEmpty();
    return;
  }

  if (!thread_id) {
    thread_id = debugger_->AnyThreadID();
  }
  auto thread = debugger_->GetThread(thread_id);
  if (!thread) {
    LOG_GDB(error)
        << "Attempt to read general registers for non-existent thread "
        << thread_id;
    SendError(EBADMSG);
    return;
  }
  if (!thread->FetchContextSync(*xbdm_)) {
    LOG_GDB(error) << "Failed to retrieve registers for thread " << thread_id;
    SendError(EBUSY);
    return;
  }
  thread->FetchFloatContextSync(*xbdm_);

  std::string response =
      SerializeRegisters(thread->context, thread->float_context);
  gdb_->Send(GDBPacket(response));
}

void GDBBridge::HandleWriteGeneralRegisters(const GDBPacket& packet) {
  LOG_GDB(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleSelectThreadForCommandGroup(const GDBPacket& packet) {
  if (packet.Data().size() < 3) {
    LOG_GDB(error) << "Command missing parameters: " << packet.DataString();
    SendError(EBADMSG);
    return;
  }

  char operation = static_cast<char>(packet.Data()[1]);
  int32_t thread_id;
  if (!MaybeParseHexInt(thread_id, packet.Data(), 2)) {
    LOG_GDB(error) << "Invalid thread_id parameter: " << packet.DataString();
    SendError(EBADMSG);
    return;
  }
  thread_id_for_command_[operation] = thread_id;
  SendOK();
}

void GDBBridge::HandleStepInstruction(const GDBPacket& packet) {
  LOG_GDB(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleSignalStep(const GDBPacket& packet) {
  LOG_GDB(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleKill(const GDBPacket& packet) {
  LOG_GDB(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleReadMemory(const GDBPacket& packet) {
  auto split = packet.FindFirst(',');

  std::string address_str(packet.Data().begin() + 1, split);
  std::string length_str(split + 1, packet.Data().end());

  uint32_t address;
  if (!MaybeParseHexInt(address, address_str)) {
    LOG_GDB(error) << "Invalid address parameter: " << packet.DataString();
    SendError(EBADMSG);
    return;
  }

  uint32_t length;
  if (!MaybeParseHexInt(length, length_str)) {
    LOG_GDB(error) << "Invalid address parameter: " << packet.DataString();
    SendError(EBADMSG);
    return;
  }

  auto memory = debugger_->GetMemory(address, length);
  if (memory.has_value()) {
    std::vector<uint8_t> data;
    boost::algorithm::hex(memory->begin(), memory->end(), back_inserter(data));
    gdb_->Send(GDBPacket(data));
  } else {
    SendError(EFAULT);
  }
}

void GDBBridge::HandleWriteMemory(const GDBPacket& packet) {
  auto place_data_split = packet.FindFirst(':');
  auto address_length_split = packet.FindFirst(',');

  std::string length_str(address_length_split + 1, place_data_split);

  uint32_t length;
  if (!MaybeParseHexInt(length, length_str)) {
    // GDB sometimes sends 0-length writes, possibly to see if writes are
    // allowed?
    // TODO: Verify that the address is writable and return an error?
    SendOK();
    return;
  }

  std::string address_str(packet.Data().begin() + 1, address_length_split);
  uint32_t address;
  if (!MaybeParseHexInt(address, address_str)) {
    LOG_GDB(error) << "Invalid address parameter: " << packet.DataString();
    SendError(EBADMSG);
    return;
  }

  std::string data_str(place_data_split + 1, packet.Data().end());
  std::vector<uint8_t> data;
  boost::algorithm::unhex(place_data_split + 1, packet.Data().end(),
                          data.begin());

  if (data.size() != length) {
    LOG_GDB(error) << "Failed to unpack " << length
                   << " bytes from hex data. Got " << data.size() << " bytes. "
                   << packet.DataString();
    SendError(EBADMSG);
    return;
  }

  if (!debugger_->SetMemory(address, data)) {
    SendError(EFAULT);
    return;
  }

  SendOK();
}

void GDBBridge::HandleReadRegister(const GDBPacket& packet) {
  int32_t register_index;
  if (!MaybeParseHexInt(register_index, packet.Data(), 1)) {
    LOG_GDB(error) << "Invalid read register message " << packet.DataString();
    SendError(EINVAL);
    return;
  }

  int32_t thread_id = GetThreadIDForCommand(packet.Command());
  if (thread_id < 0) {
    LOG_GDB(error) << "Unsupported read register query for all threads: "
                   << packet.DataString();
    SendEmpty();
    return;
  }

  if (!thread_id) {
    thread_id = debugger_->AnyThreadID();
  }
  auto thread = debugger_->GetThread(thread_id);
  if (!thread) {
    LOG_GDB(error) << "Attempt to read register for non-existent thread "
                   << thread_id;
    SendError(EBADMSG);
    return;
  }

  if (register_index < FLOAT_REGISTER_OFFSET) {
    if (!thread->FetchContextSync(*xbdm_)) {
      LOG_GDB(error) << "Failed to retrieve register for thread " << thread_id;
      SendError(EBUSY);
      return;
    }
  } else {
    thread->FetchFloatContextSync(*xbdm_);
  }

  auto value =
      GetRegister(register_index, thread->context, thread->float_context);
  if (!value.has_value()) {
    SendEmpty();
    return;
  }
  char response[32] = {0};
  snprintf(response, 31, "%lx", value.value());
  gdb_->Send(GDBPacket(response));
}

void GDBBridge::HandleWriteRegister(const GDBPacket& packet) {
  // P0=10270000
  auto index_value_split = packet.FindFirst('=');
  if (index_value_split == packet.Data().end()) {
    LOG_GDB(error) << "Invalid write register message " << packet.DataString();
    SendError(EINVAL);
    return;
  }

  int32_t register_index;
  if (!MaybeParseHexInt(register_index, packet.Data(), 1)) {
    LOG_GDB(error) << "Failed to parse register index from "
                   << packet.DataString();
    SendError(EINVAL);
    return;
  }

  uint64_t value;
  if (!MaybeParseHexInt(value, packet.Data(),
                        index_value_split - packet.Data().begin() + 1)) {
    LOG_GDB(error) << "Failed to parse value from " << packet.DataString();
    SendError(EINVAL);
    return;
  }

  int32_t thread_id = GetThreadIDForCommand(packet.Command());
  if (thread_id < 0) {
    LOG_GDB(error) << "Unsupported read register query for all threads: "
                   << packet.DataString();
    SendEmpty();
    return;
  }

  if (!thread_id) {
    thread_id = debugger_->AnyThreadID();
  }
  auto thread = debugger_->GetThread(thread_id);
  if (!thread) {
    LOG_GDB(error) << "Attempt to read register for non-existent thread "
                   << thread_id;
    SendError(EBADMSG);
    return;
  }

  if (register_index < FLOAT_REGISTER_OFFSET) {
    if (!thread->FetchContextSync(*xbdm_)) {
      LOG_GDB(error) << "Failed to retrieve register for thread " << thread_id;
      SendError(EBUSY);
      return;
    }
    if (!SetRegister(register_index, value & 0xFFFFFFFF, thread->context)) {
      LOG_GDB(error) << "Failed to update context for register "
                     << register_index << " for thread " << thread_id;
      SendError(EBUSY);
      return;
    }
    if (!thread->PushContextSync(*xbdm_)) {
      LOG_GDB(error) << "Failed to push context for thread " << thread_id;
      SendError(EBUSY);
      return;
    }
  } else {
    thread->FetchFloatContextSync(*xbdm_);
    if (!SetRegister(register_index, value, thread->float_context)) {
      LOG_GDB(error) << "Failed to update context for register "
                     << register_index << " for thread " << thread_id;
      SendError(EBUSY);
      return;
    }
    if (!thread->PushFloatContextSync(*xbdm_)) {
      LOG_GDB(error) << "Failed to push context for thread " << thread_id;
      SendError(EBUSY);
      return;
    }
  }

  SendOK();
}

void GDBBridge::HandleReadQuery(const GDBPacket& packet) {
  std::string query(packet.Data().begin() + 1, packet.Data().end());

  if (boost::algorithm::starts_with(query, "Attached")) {
    HandleQueryAttached(packet);
    return;
  }

  if (boost::algorithm::starts_with(query, "Supported")) {
    HandleQuerySupported(packet);
    return;
  }

  if (boost::algorithm::starts_with(query, "ThreadExtraInfo")) {
    HandleQueryThreadExtraInfo(packet);
    return;
  }

  if (query == "fThreadInfo") {
    HandleThreadInfoStart();
    return;
  }

  if (query == "sThreadInfo") {
    HandleThreadInfoContinue();
    return;
  }

  if (query == "TStatus") {
    HandleQueryTraceStatus();
    return;
  }

  if (query == "C") {
    HandleQueryCurrentThreadID();
    return;
  }

  if (boost::algorithm::starts_with(query, "Xfer:features:read:")) {
    HandleFeaturesRead(packet);
    return;
  }

  LOG_GDB(error) << "Unsupported query read packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleWriteQuery(const GDBPacket& packet) {
  if (packet.DataString() == "QStartNoAckMode") {
    gdb_->SetNoAckMode(true);
    SendOK();
    return;
  }

  LOG_GDB(error) << "Unsupported query write packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleRestartSystem(const GDBPacket& packet) {
  LOG_GDB(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleSingleStep(const GDBPacket& packet) {
  LOG_GDB(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleSingleStepWithSignal(const GDBPacket& packet) {
  LOG_GDB(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleSearchBackward(const GDBPacket& packet) {
  LOG_GDB(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleCheckThreadAlive(const GDBPacket& packet) {
  LOG_GDB(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleExtendedVCommand(const GDBPacket& packet) {
  std::string data = packet.DataString();
  if (data == "vCont?") {
    HandleVContQuery();
    return;
  }

  if (boost::algorithm::starts_with(data, "vCont;")) {
    HandleVCont(data.substr(6));
    return;
  }

  if (data != "vMustReplyEmpty") {
    LOG_GDB(error) << "Unsupported v packet: " << data;
  }
  SendEmpty();
}

void GDBBridge::HandleWriteMemoryBinary(const GDBPacket& packet) {
  auto place_data_split = packet.FindFirst(':');
  auto address_length_split = packet.FindFirst(',');

  std::string length_str(address_length_split + 1, place_data_split);

  uint32_t length;
  if (!MaybeParseHexInt(length, length_str)) {
    // GDB sometimes sends 0-length writes, possibly to see if writes are
    // allowed?
    // TODO: Verify that the address is writable and return an error?
    SendOK();
    return;
  }

  std::string address_str(packet.Data().begin() + 1, address_length_split);
  uint32_t address;
  if (!MaybeParseHexInt(address, address_str)) {
    LOG_GDB(error) << "Invalid address parameter: " << packet.DataString();
    SendError(EBADMSG);
    return;
  }

  std::vector<uint8_t> data(place_data_split + 1, packet.Data().end());
  if (length != data.size()) {
    LOG_GDB(error) << "Packet size mismatch, got " << data.size()
                   << " bytes but expected " << length;
    SendError(EBADMSG);
    return;
  }

  if (!debugger_->SetMemory(address, data)) {
    SendError(EFAULT);
  } else {
    SendOK();
  }
}

static bool ExtractBreakpointCommandParams(
    const GDBPacket& packet, int32_t& type, uint32_t& address, int32_t& kind,
    std::vector<std::vector<uint8_t>>& args) {
  {
    static constexpr uint8_t delim[] = {';'};
    boost::split(args, packet.Data(), boost::is_any_of(delim));
    if (args.empty()) {
      return false;
    }
  }

  static constexpr uint8_t delim[] = {','};
  std::vector<std::vector<uint8_t>> type_address_kind;
  boost::split(type_address_kind, args.front(), boost::is_any_of(delim));

  if (type_address_kind.size() != 3) {
    return false;
  }

  // Drop the breakpoint command argument.
  type_address_kind[0].erase(type_address_kind[0].begin());
  if (!MaybeParseHexInt(type, type_address_kind[0])) {
    return false;
  }
  if (!MaybeParseHexInt(address, type_address_kind[1])) {
    return false;
  }
  if (!MaybeParseHexInt(kind, type_address_kind[2])) {
    return false;
  }

  args.erase(args.begin());

  return true;
}

void GDBBridge::HandleRemoveBreakpointType(const GDBPacket& packet) {
  int32_t type;
  uint32_t address;
  int32_t int_arg;
  std::vector<std::vector<uint8_t>> args;
  if (!ExtractBreakpointCommandParams(packet, type, address, int_arg, args)) {
    LOG_GDB(error) << "Invalid Remove Breakpoint message "
                   << packet.DataString();
    SendError(EBADMSG);
    return;
  }

  switch (type) {
    case BP_SOFTWARE:
      if (int_arg != 1) {
        LOG_GDB(warning) << "Remove swbreak at " << std::hex << std::setw(8)
                         << std::setfill('0') << address << "with kind "
                         << std::dec << int_arg;
      }
      debugger_->RemoveBreakpoint(address);
      SendOK();
      return;

    case BP_HARDWARE:
      SendEmpty();
      break;

    case BP_WRITE:
      debugger_->RemoveWriteWatch(address, int_arg);
      SendOK();
      return;

    case BP_READ:
      debugger_->RemoveReadWatch(address, int_arg);
      SendOK();
      return;

    case BP_ACCESS:
      debugger_->RemoveReadWatch(address, int_arg);
      debugger_->RemoveWriteWatch(address, int_arg);
      SendOK();
      return;

    default:
      LOG_GDB(error) << "Unsupported remove breakpoint type "
                     << packet.DataString();
      break;
  }

  SendEmpty();
}

void GDBBridge::HandleInsertBreakpointType(const GDBPacket& packet) {
  int32_t type;
  uint32_t address;
  int32_t int_arg;
  std::vector<std::vector<uint8_t>> args;
  if (!ExtractBreakpointCommandParams(packet, type, address, int_arg, args)) {
    LOG_GDB(error) << "Invalid Add Breakpoint message " << packet.DataString();
    SendError(EBADMSG);
    return;
  }

  switch (type) {
    case BP_SOFTWARE:
      if (int_arg != 1 || !args.empty()) {
        LOG_GDB(warning) << "Partially supported insert swbreak " << std::hex
                         << std::setw(8) << std::setfill('0') << address
                         << "with kind " << std::dec << int_arg;
      }
      debugger_->AddBreakpoint(address);
      SendOK();
      return;

    case BP_HARDWARE:
      SendEmpty();
      break;

    case BP_WRITE:
      debugger_->AddWriteWatch(address, int_arg);
      SendOK();
      return;

    case BP_READ:
      debugger_->AddReadWatch(address, int_arg);
      SendOK();
      return;

    case BP_ACCESS:
      debugger_->AddReadWatch(address, int_arg);
      debugger_->AddWriteWatch(address, int_arg);
      SendOK();
      return;

    default:
      LOG_GDB(error) << "Unsupported add breakpoint type "
                     << packet.DataString();
      break;
  }

  SendEmpty();
}

bool GDBBridge::SendThreadStopPacket(const std::shared_ptr<Thread>& thread) {
  if (!gdb_ || !gdb_->IsConnected()) {
    return false;
  }

  if (!thread) {
    LOG_GDB(error) << "Attempting to send stop packet for a null thread.";
    return false;
  }
  if (!thread->stopped) {
    LOG_GDB(error) << "Attempting to send stop packet for a running thread.";
    return false;
  }

  auto stop_reason = thread->last_stop_reason;
  if (not stop_reason) {
    return false;
  }

  // TODO: send detailed information.
  // see
  // https://sourceware.org/gdb/onlinedocs/gdb/Stop-Reply-Packets.html#Stop-Reply-Packets
  char buffer[64] = {0};
  int len = snprintf(buffer, 63, "T%02xthread:%x;", stop_reason->signal,
                     thread->thread_id);

  switch (stop_reason->type) {
    case SRT_UNKNOWN:
      // In practice this should only be hit for the case where we've broken
      // because this thread was just created and we're in StopOn thread_create
      // mode.
      //      LOG_GDB(error)
      //          << "Attempting to send stop notification for unknown stop
      //          reason";
      break;

    case SRT_THREAD_CREATED:
      if (send_thread_events_) {
        LOG_GDB(error) << "TODO: Send thread created events.";
      }
      break;

    case SRT_THREAD_TERMINATED:
      if (send_thread_events_) {
        LOG_GDB(error) << "TODO: Send thread terminated events.";
      }
      break;

    case SRT_WATCHPOINT: {
      auto reason =
          std::dynamic_pointer_cast<StopReasonDataBreakpoint>(stop_reason);
      std::string access_type;
      switch (reason->access_type) {
        case StopReasonDataBreakpoint::AT_READ:
          access_type = "rwatch";
          break;

        case StopReasonDataBreakpoint::AT_WRITE:
          access_type = "watch";
          break;

        case StopReasonDataBreakpoint::AT_EXECUTE:
          LOG_GDB(warning)
              << "Watchpoint of type Execute not supported by GDB.";

        case StopReasonDataBreakpoint::AT_UNKNOWN:
          break;
      }
      if (!access_type.empty()) {
        snprintf(buffer + len, 63 - len, "%s:%08x;", access_type.c_str(),
                 reason->access_address);
      }
    } break;

    case SRT_ASSERTION:
    case SRT_DEBUGSTR:
    case SRT_BREAKPOINT:
    case SRT_SINGLE_STEP:
    case SRT_EXECUTION_STATE_CHANGED:
    case SRT_EXCEPTION:
    case SRT_MODULE_LOADED:
    case SRT_SECTION_LOADED:
    case SRT_SECTION_UNLOADED:
    case SRT_RIP:
    case SRT_RIP_STOP:
      break;
  }

  waiting_on_stop_packet_.store(false);
  gdb_->Send(GDBPacket(buffer));
  return true;
}

int32_t GDBBridge::GetThreadIDForCommand(char command) const {
  auto registered = thread_id_for_command_.find(command);
  int32_t thread_id;
  if (registered == thread_id_for_command_.end()) {
    LOG_GDB(warning) << "Request for registered thread with command '"
                     << command << "' but no thread is set!";
    thread_id = 0;
  } else {
    thread_id = registered->second;
  }

  return thread_id;
}

std::shared_ptr<Thread> GDBBridge::GetThreadForCommand(char command) const {
  int32_t thread_id = GetThreadIDForCommand(command);
  if (thread_id > 0) {
    return debugger_->GetThread(thread_id);
  }

  return debugger_->GetAnyThread();
}

void GDBBridge::HandleQueryAttached(const GDBPacket& packet) {
  // Always report that the debugger is attached to an existing process.
  gdb_->Send(GDBPacket("1"));
}

void GDBBridge::HandleQuerySupported(const GDBPacket& packet) {
  // TODO: Disallow multiple qSupported queries on the same connection.

  auto split = packet.FindFirst(':');
  if (split == packet.Data().end()) {
    LOG_GDB(error) << "Invalid qSupported message " << packet.DataString();
    SendEmpty();
    return;
  }

  std::vector<std::string> features;
  {
    std::string feature_str(split + 1, packet.Data().end());
    static constexpr uint8_t delim[] = {';'};
    boost::split(features, feature_str, boost::is_any_of(delim));
  }

  std::string response = "PacketSize=4096;qXfer:features:read+;";
  for (auto& feature : features) {
    if (feature == "multiprocess+") {
      response += "multiprocess-;";
      continue;
    }
    if (feature == "swbreak+") {
      response += "swbreak+;";
      continue;
    }
    if (feature == "hwbreak+") {
      response += "hwbreak-;";
      continue;
    }
    if (feature == "qRelocInsn+") {
      response += "qRelocInsn-;";
      continue;
    }
    if (feature == "fork-events+") {
      response += "fork-events-;";
      continue;
    }
    if (feature == "vfork-events+") {
      response += "vfork-events-;";
      continue;
    }
    if (feature == "exec-events+") {
      response += "exec-events-;";
      continue;
    }
    if (feature == "vContSupported+") {
      response += "vContSupported+;";
      continue;
    }
    if (feature == "QThreadEvents+") {
      response += "QThreadEvents-;";
      continue;
    }
    if (feature == "no-resumed+") {
      response += "no-resumed-;";
      continue;
    }
    if (feature == "xmlRegisters=i386") {
      continue;
    }

    LOG_GDB(trace) << "Unknown feature " << feature;
  }

  gdb_->Send(GDBPacket(response));
}

void GDBBridge::HandleQueryThreadExtraInfo(const GDBPacket& packet) {
  auto split = packet.FindFirst(',');
  int32_t thread_id;
  if (!MaybeParseHexInt(thread_id, packet.Data(),
                        split - packet.Data().begin() + 1)) {
    LOG_GDB(error) << "Invalid thread_id parameter: " << packet.DataString();
    SendError(EBADMSG);
    return;
  }

  auto thread = debugger_->GetThread(thread_id);
  if (!thread) {
    LOG_GDB(error) << "ThreadExtraInfo query for non-existent thread id: "
                   << thread_id;
    SendError(EBADMSG);
    return;
  }

  char buffer[128] = {0};
  std::string stop_reason;
  if (!thread->stopped) {
    stop_reason = "Running";
  } else {
    switch (thread->last_stop_reason->type) {
      case SRT_DEBUGSTR:
        stop_reason = "debugstr";
        break;

      case SRT_ASSERTION:
        stop_reason = "assert";
        break;

      case SRT_BREAKPOINT:
        stop_reason = "breakpoint";
        break;

      case SRT_SINGLE_STEP:
        stop_reason = "single_step";
        break;

      case SRT_WATCHPOINT:
        stop_reason = "watchpoint";
        break;

      case SRT_EXECUTION_STATE_CHANGED:
        stop_reason = "execution_state_changed";
        break;

      case SRT_EXCEPTION:
        stop_reason = "exception";
        break;

      case SRT_THREAD_CREATED:
        stop_reason = "thread_created";
        break;

      case SRT_THREAD_TERMINATED:
        stop_reason = "thread_terminated";
        break;

      case SRT_MODULE_LOADED:
        stop_reason = "module_loaded";
        break;

      case SRT_SECTION_LOADED:
        stop_reason = "section_loaded";
        break;

      case SRT_SECTION_UNLOADED:
        stop_reason = "section_unloaded";
        break;

      case SRT_RIP:
        stop_reason = "RIP";
        break;

      case SRT_RIP_STOP:
        stop_reason = "RIP_Stop";
        break;

      case SRT_UNKNOWN:
        stop_reason = "UNKNOWN_STATE";
        break;
    }
  }
  snprintf(buffer, 127, "%d %s", thread_id, stop_reason.c_str());

  std::vector<uint8_t> data;
  boost::algorithm::hex(buffer, std::back_inserter(data));
  gdb_->Send(GDBPacket(data));
}

void GDBBridge::HandleThreadInfoStart() {
  if (!debugger_->FetchThreads()) {
    SendError(EFAULT);
    return;
  }

  thread_info_buffer_ = debugger_->GetThreadIDs();
  SendThreadInfoBuffer(false);
}

void GDBBridge::HandleThreadInfoContinue() { SendThreadInfoBuffer(); }

void GDBBridge::SendThreadInfoBuffer(bool send_all) {
  if (thread_info_buffer_.empty()) {
    gdb_->Send(GDBPacket("l"));
    return;
  }

  std::string send_buffer = "m";

  char buffer[16] = {0};
  auto it = thread_info_buffer_.begin();
  snprintf(buffer, 15, "%x", *it++);
  send_buffer += buffer;

  if (send_all) {
    for (; it != thread_info_buffer_.end(); ++it) {
      snprintf(buffer, 15, ",%x", *it);
      send_buffer += buffer;
    }
  }

  if (it == thread_info_buffer_.end()) {
    thread_info_buffer_.clear();
  } else {
    thread_info_buffer_.erase(thread_info_buffer_.begin(), it);
  }

  gdb_->Send(GDBPacket(send_buffer));
}

void GDBBridge::HandleQueryTraceStatus() { SendEmpty(); }

void GDBBridge::HandleQueryCurrentThreadID() {
  int32_t thread_id = debugger_->AnyThreadID();
  if (thread_id >= 0) {
    char buffer[32] = {0};
    snprintf(buffer, 31, "QC%x", thread_id);
    gdb_->Send(GDBPacket(buffer));
    return;
  }

  SendEmpty();
}

void GDBBridge::HandleFeaturesRead(const GDBPacket& packet) {
  std::string command = packet.DataString();

  size_t body_start = command.find("read:");
  if (body_start == std::string::npos) {
    LOG_GDB(error) << "Invalid feature read packet " << command;
    SendError(EBADMSG);
    return;
  }
  body_start += 5;

  size_t target_region_delim = command.find(':', body_start);
  if (target_region_delim == std::string::npos) {
    LOG_GDB(error) << "Invalid feature read packet, missing region " << command;
    SendError(EBADMSG);
    return;
  }

  std::string target_file =
      command.substr(body_start, target_region_delim - body_start);
  if (target_file != "target.xml") {
    LOG_GDB(error) << "Request for unknown resource " << target_file;
    SendError(EBADMSG);
    return;
  }

  size_t start_length_delim = command.find(',', target_region_delim + 1);
  if (start_length_delim == std::string::npos) {
    LOG_GDB(error) << "Invalid feature read packet, missing offset,length "
                   << command;
    SendError(EBADMSG);
    return;
  }

  std::string start_str(
      std::next(command.begin(), static_cast<long>(target_region_delim) + 1),
      std::next(command.begin(), static_cast<long>(start_length_delim)));
  uint32_t start;
  if (!MaybeParseHexInt(start, start_str)) {
    LOG_GDB(error) << "Invalid feature read packet, bad offset " << command;
    SendError(EBADMSG);
    return;
  }

  std::string length_str(
      std::next(command.begin(), static_cast<long>(start_length_delim) + 1),
      command.end());
  uint32_t length;
  if (!MaybeParseHexInt(length, length_str)) {
    LOG_GDB(error) << "Invalid feature read packet, bad length " << command;
    SendError(EBADMSG);
    return;
  }

  uint32_t end = start + length;

  LOG_GDB(trace) << "Feature read " << target_file << " [" << start << " - "
                 << end << "]";

  uint32_t available = kTargetXML.size();
  if (start >= available) {
    gdb_->Send(GDBPacket("l"));
    return;
  }

  std::string buffer;
  if (end >= available) {
    buffer = "l";
    length = available - start;
  } else {
    buffer = "m";
  }

  buffer += kTargetXML.substr(start, length);

  gdb_->Send(GDBPacket(buffer));
}

void GDBBridge::HandleVContQuery() {
  // c - continue
  // s - step
  gdb_->Send(GDBPacket("vCont;c;C;s;S"));
}

void GDBBridge::HandleVCont(const std::string& args) {
  if (args.empty()) {
    LOG_GDB(error) << "Unexpected empty vcont packet.";
    SendError(EBADMSG);
  }

  std::vector<std::string> commands;
  boost::split(commands, args, boost::is_any_of(";"));

  std::set<uint32_t> processed_threads;

  for (auto& command : commands) {
    int thread_id = -1;
    std::vector<std::string> command_params;
    boost::split(command_params, command, boost::is_any_of(":"));
    if (command_params.size() > 1) {
      if (!MaybeParseHexInt(thread_id, command_params[1])) {
        LOG_GDB(error) << "Failed to extract thread id from  vCont " << command;
        SendError(EBADMSG);
        continue;
      }
    }

    if (thread_id > 0) {
      processed_threads.insert(thread_id);
    }

    char command_code = command.front();
    if (command_code == 'c') {
      if (processed_threads.empty() && thread_id <= 0) {
        if (!debugger_->ContinueAll()) {
          LOG_GDB(warning) << "Failed to continue after vCont;c";
        }
        continue;
      }

      if (thread_id > 0) {
        if (!debugger_->ContinueThread(thread_id)) {
          LOG_GDB(warning) << "Failed to continue thread " << command;
        }
        continue;
      }

      for (auto& thread : debugger_->Threads()) {
        if (processed_threads.find(thread->thread_id) !=
            processed_threads.end()) {
          continue;
        }

        if (!debugger_->ContinueThread(thread_id)) {
          LOG_GDB(warning) << "Failed to continue thread " << thread_id << " "
                           << command;
        }
      }
      continue;
    }

    if (command_code == 's') {
      if (thread_id > 0) {
        debugger_->SetActiveThread(thread_id);
        processed_threads.insert(thread_id);
      } else {
        LOG_GDB(error) << "TODO: Implement vCont s with no thread arg. "
                       << command;
        SendError(EBADMSG);
        continue;
      }

      if (!debugger_->StepInstruction()) {
        SendError(EFAULT);
        continue;
      }
      continue;
    }

    LOG_GDB(error) << "TODO: Implement vCont subcommand " << command;
    SendEmpty();
  }

  MarkWaitingForStopPacket();
  if (!debugger_->Go()) {
    LOG_GDB(warning) << "Go failed in vCont handler.";
  }
}

void GDBBridge::MarkWaitingForStopPacket() {
  auto thread = debugger_->GetFirstStoppedThread();
  if (thread) {
    SendThreadStopPacket(thread);
    waiting_on_stop_packet_.store(false);
  } else {
    waiting_on_stop_packet_.store(true);
  }
}

void GDBBridge::OnNotification(
    const std::shared_ptr<XBDMNotification>& notification) {
  switch (notification->Type()) {
      //    case NT_MODULE_LOADED:
      //      OnModuleLoaded(
      //          std::dynamic_pointer_cast<NotificationModuleLoaded>(notification));
      //      break;
      //
      //    case NT_SECTION_LOADED:
      //      OnSectionLoaded(
      //          std::dynamic_pointer_cast<NotificationSectionLoaded>(notification));
      //      break;
      //
      //    case NT_SECTION_UNLOADED:
      //      OnSectionUnloaded(
      //          std::dynamic_pointer_cast<NotificationSectionUnloaded>(notification));
      //      break;
      //
      //    case NT_THREAD_CREATED:
      //      OnThreadCreated(
      //          std::dynamic_pointer_cast<NotificationThreadCreated>(notification));
      //      break;
      //
      //    case NT_THREAD_TERMINATED:
      //      OnThreadTerminated(
      //          std::dynamic_pointer_cast<NotificationThreadTerminated>(
      //              notification));
      //      break;

    case NT_EXECUTION_STATE_CHANGED:
      OnExecutionStateChanged(
          std::dynamic_pointer_cast<NotificationExecutionStateChanged>(
              notification));
      break;

    case NT_BREAKPOINT:
      OnBreakpoint(
          std::dynamic_pointer_cast<NotificationBreakpoint>(notification));
      break;

    case NT_WATCHPOINT:
      OnWatchpoint(
          std::dynamic_pointer_cast<NotificationWatchpoint>(notification));
      break;

    case NT_SINGLE_STEP:
      OnSingleStep(
          std::dynamic_pointer_cast<NotificationSingleStep>(notification));
      break;

    case NT_EXCEPTION:
      OnException(
          std::dynamic_pointer_cast<NotificationException>(notification));
      break;

    case NT_INVALID:
      LOG_GDB(error) << "XBDMNotif: Received invalid notification type.";
      break;

    default:
      // Other types are not interesting to GDB.
      break;
  }
}

void GDBBridge::OnExecutionStateChanged(
    const std::shared_ptr<NotificationExecutionStateChanged>& msg) {
  // TODO: Report reboots as well?
  if (msg->state != ExecutionState::S_STOPPED) {
    return;
  }

  if (!waiting_on_stop_packet_) {
    return;
  }

  SendThreadStopPacket(debugger_->ActiveThread());
}

void GDBBridge::OnBreakpoint(
    const std::shared_ptr<NotificationBreakpoint>& msg) {
  if (!waiting_on_stop_packet_) {
    return;
  }

  SendThreadStopPacket(debugger_->ActiveThread());
}

void GDBBridge::OnWatchpoint(
    const std::shared_ptr<NotificationWatchpoint>& msg) {
  if (!waiting_on_stop_packet_) {
    return;
  }
  SendThreadStopPacket(debugger_->ActiveThread());
}

void GDBBridge::OnSingleStep(
    const std::shared_ptr<NotificationSingleStep>& msg) {
  LOG_GDB(warning) << "SingleStep: " << *msg;
  if (!waiting_on_stop_packet_) {
    return;
  }
  SendThreadStopPacket(debugger_->ActiveThread());
}

void GDBBridge::OnException(const std::shared_ptr<NotificationException>& msg) {
  LOG_GDB(warning) << "Received exception: " << *msg;
  if (!waiting_on_stop_packet_) {
    return;
  }
  SendThreadStopPacket(debugger_->ActiveThread());
}