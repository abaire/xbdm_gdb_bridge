// See
// https://sourceware.org/gdb/onlinedocs/gdb/Remote-Protocol.html#Remote-Protocol

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

void GDBBridge::HandleEnableExtendedMode(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleQueryHaltReason(const GDBPacket& packet) {
  auto thread = debugger_->ActiveThread();
  if (!thread || !thread->FetchStopReasonSync(*xbdm_)) {
    BOOST_LOG_TRIVIAL(error) << "Halt query issued but target is not halted.";
    SendEmpty();
  } else {
    SendThreadStopPacket(thread);
  }
}

void GDBBridge::HandleArgv(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

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

void GDBBridge::HandleBackwardContinue(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleBackwardStep(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleContinue(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleContinueWithSignal(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleDetach(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleFileIO(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleReadGeneralRegisters(const GDBPacket& packet) {
#if 0
  def _handle_read_general_registers(self, pkt: GDBPacket):
        thread_id = self._get_thread_context_for_command("g")
        if thread_id == self.TID_ALL_THREADS:
            logger.error(f"Unsupported 'g' query for all threads.")
            self._send_empty()
            return

        if thread_id == self.TID_ANY_THREAD:
            thread_id = self._debugger.any_thread_id

        thread = self._debugger.get_thread(thread_id)

        context: debugger.Thread.FullContext = thread.get_full_context()
        if not context:
            self._send_error(self.ERR_RETRIEVAL_FAILED)

        reg_info = {k: "0x%08X" % v for k, v in context.registers.items()}
        logger.info(f"Registers:\n{reg_info}\n")

        body = []
        for register in resources.ORDERED_REGISTERS:
            value: Optional[int] = context.registers.get(register, None)
            str_value = resources.format_register_str(register, value)

#logger.gdb(f "{register}: {str_value}")
            body.append(str_value)

        self.send_packet(GDBPacket("".join(body)))
#endif
}

void GDBBridge::HandleWriteGeneralRegisters(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleSelectThreadForCommandGroup(const GDBPacket& packet) {
#if 0
  if len(pkt.data) < 3:
            logger.error(f"Command missing parameters: {pkt.data}")
            self.send_packet(GDBPacket("E"))
            return

        op = pkt.data[1]
        thread_id = int(pkt.data[2:], 16)
        self._command_thread_id_context[op] = thread_id
        self._send_ok()
#endif
}

void GDBBridge::HandleStepInstruction(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleSignalStep(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleKill(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleReadMemory(const GDBPacket& packet) {
#if 0
  addr, length = pkt.data[1:].split(",")
        addr = int(addr, 16)
        length = int(length, 16)
        mem: Optional[bytes] = self._debugger.get_memory(addr, length)
        if mem:
            self.send_packet(GDBPacket(mem.hex()))
        else:
            self._send_error(errno.EFAULT)
#endif
}

void GDBBridge::HandleWriteMemory(const GDBPacket& packet) {
#if 0
  place, data = pkt.data[1:].split(":")
        addr, length = place.split(",")
        addr = int(addr, 16)
        length = int(length, 16)

        if not length:
            self._send_ok()
            return

        data = binascii.unhexlify(data)
        if length != len(data):
            self._send_error(errno.EBADMSG)
            return
        self._set_memory(addr, data)
#endif
}

void GDBBridge::HandleReadRegister(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleWriteRegister(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleReadQuery(const GDBPacket& packet) {
#if 0
  query = pkt.data[1:]

        if query.startswith("Attached"):
            self._handle_query_attached(pkt)
            return

        if query.startswith("Supported"):
            self._handle_query_supported(pkt)
            return

        if query.startswith("ThreadExtraInfo"):
            self._handle_query_thread_extra_info(pkt)
            return

        if query == "fThreadInfo":
            self._handle_thread_info_start()
            return

        if query == "sThreadInfo":
            self._handle_thread_info_continue()
            return

        if query == "TStatus":
            self._handle_query_trace_status()
            return

        if query == "C":
            self._handle_query_current_thread_id()
            return

        if query.startswith("Xfer:features:read:"):
            self._handle_features_read(pkt)
            return

        logger.error(f"Unsupported query read packet {pkt.data}")
        self.send_packet(GDBPacket())
#endif
}

void GDBBridge::HandleWriteQuery(const GDBPacket& packet) {
#if 0
  if pkt.data == "QStartNoAckMode":
            self._start_no_ack_mode()
            return

        logger.error(f"Unsupported query write packet {pkt.data}")
        self.send_packet(GDBPacket())
#endif
}

void GDBBridge::HandleRestartSystem(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleSingleStep(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleSingleStepWithSignal(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleSearchBackward(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleCheckThreadAlive(const GDBPacket& packet) {
  BOOST_LOG_TRIVIAL(error) << "Unsupported packet " << packet.DataString();
  SendEmpty();
}

void GDBBridge::HandleExtendedVCommand(const GDBPacket& packet) {
#if 0
  if pkt.data == "vCont?":
            self._handle_vcont_query()
            return

        if pkt.data.startswith("vCont;"):
            self._handle_vcont(pkt.data[6:])
            return

#Suppress error for vMustReplyEmpty, which intentionally follows the same
#handling as any other unsupported v packet.
        if pkt.data != "vMustReplyEmpty":
            logger.error(f"Unsupported v packet {pkt.data}")
        self._send_empty()
#endif
}

void GDBBridge::HandleWriteMemoryBinary(const GDBPacket& packet) {
#if 0
  place, data = pkt.binary_data[1:].split(b":")

        place = place.decode("utf-8")
        addr, length = place.split(",")
        addr = int(addr, 16)
        length = int(length, 16)

        if not length:
            self._send_ok()
            return

        if length != len(data):
            self._send_error(errno.EBADMSG)
            return
        self._set_memory(addr, data)

    def _set_memory(self, addr: int, data: bytes):
        result = self._debugger.set_memory(addr, data)
        if not result:
            self._send_error(errno.EFAULT)
            return
        self._send_ok()
#endif
}

#if 0
@staticmethod
    def _extract_breakpoint_command_params(
        pkt: GDBPacket,
    ) -> Tuple[int, int, int, Optional[List]]:
        elements = pkt.data[1:].split(";")

        type, addr, kind = elements[0].split(",")
        type = int(type)
        addr = int(addr, 16)
        kind = int(kind, 16)

        args = None
        if len(elements) > 1:
            args = elements[1:]

        return type, addr, kind, args
#endif

void GDBBridge::HandleRemoveBreakpointType(const GDBPacket& packet) {
#if 0
  type, addr, kind, _args = self._extract_breakpoint_command_params(pkt)

        if type == self.BP_SOFTWARE:
            self._handle_remove_software_breakpoint(addr, kind)
            return

        if type == self.BP_HARDWARE:
            self._send_empty()
            return

        if type == self.BP_WRITE:
            self._handle_remove_write_breakpoint(addr, kind)
            return

        if type == self.BP_READ:
            self._handle_remove_read_breakpoint(addr, kind)
            return

        if type == self.BP_ACCESS:
            self._handle_remove_access_breakpoint(addr, kind)
            return

        logger.error(f"Unsupported packet {pkt.data}")
        self.send_packet(GDBPacket())

    def _handle_remove_software_breakpoint(self, addr: int, kind: int):
        if kind != 1:
            logger.warning(f"Remove swbreak at 0x%X with kind {kind}" % addr)
        self._debugger.remove_breakpoint_at_address(addr)
        self._send_ok()

    def _handle_remove_write_breakpoint(self, addr, length):
        if not self._debugger.remove_write_watchpoint(addr, length):
            self._send_error(errno.EBADMSG)
        else:
            self._send_ok()

    def _handle_remove_read_breakpoint(self, addr, length):
        if not self._debugger.remove_read_watchpoint(addr, length):
            self._send_error(errno.EBADMSG)
        else:
            self._send_ok()

    def _handle_remove_access_breakpoint(self, addr, length):
        ret = self._debugger.remove_read_watchpoint(addr, length)
        ret = self._debugger.remove_write_watchpoint(addr, length) and ret

        if not ret:
            self._send_error(errno.EBADMSG)
        else:
            self._send_ok()
#endif
}

void GDBBridge::HandleInsertBreakpointType(const GDBPacket& packet) {
#if 0
  type, addr, kind, args = self._extract_breakpoint_command_params(pkt)

        if type == self.BP_SOFTWARE:
            self._handle_insert_software_breakpoint(addr, kind, args)
            return

        if type == self.BP_HARDWARE:
            self._send_empty()
            return

        if type == self.BP_WRITE:
            self._handle_insert_write_breakpoint(addr, kind)
            return

        if type == self.BP_READ:
            self._handle_insert_read_breakpoint(addr, kind)
            return

        if type == self.BP_ACCESS:
            self._handle_insert_access_breakpoint(addr, kind)
            return

        logger.error(f"Unsupported packet {pkt.data}")
        self._send_empty()

    def _handle_insert_write_breakpoint(self, addr, length):
        if not self._debugger.add_write_watchpoint(addr, length):
            self._send_error(errno.EBADMSG)
        else:
            self._send_ok()

    def _handle_insert_read_breakpoint(self, addr, length):
        if not self._debugger.add_read_watchpoint(addr, length):
            self._send_error(errno.EBADMSG)
        else:
            self._send_ok()

    def _handle_insert_access_breakpoint(self, addr, length):
        if not self._debugger.add_read_watchpoint(addr, length):
            self._send_error(errno.EBADMSG)
            return

        if not self._debugger.add_write_watchpoint(addr, length):
            self._send_error(errno.EBADMSG)
            if not self._debugger.remove_read_watchpoint(addr, length):
                logger.warning(
                    "Failure to add write watchpoint left hanging read watchpoint at 0x%X %d"
                    % (addr, length)
                )
        else:
            self._send_ok()
#endif
}

#if 0
def _handle_query_attached(self, _pkt: GDBPacket):
#If attached to an existing process:
        self.send_packet(GDBPacket("1"))
#elif started new process
#self.send_packet(GDBPacket("0"))

    def _handle_query_supported(self, pkt: GDBPacket):
        if self._state != self.STATE_INIT:
            logger.warning("Ignoring assumed qSupported retransmission")
            return

        request = pkt.data.split(":", 1)
        if len(request) != 2:
            logger.error(f"Unsupported qSupported message {pkt.data}")
            return

        response = []
        features = request[1].split(";")
        for feature in features:
            if feature == "multiprocess+":
#Do not support multiprocess extensions.
                response.append("multiprocess-")
                continue
            if feature == "swbreak+":
                self.features["swbreak"] = True
                response.append("swbreak+")
                continue
            if feature == "hwbreak+":
                response.append("hwbreak-")
                continue
            if feature == "qRelocInsn+":
                response.append("qRelocInsn-")
                continue
            if feature == "fork-events+":
                response.append("fork-events-")
                continue
            if feature == "vfork-events+":
                response.append("vfork-events-")
                continue
            if feature == "exec-events+":
                response.append("exec-events-")
                continue
            if feature == "vContSupported+":
                response.append("vContSupported+")
                continue
            if feature == "QThreadEvents+":
                response.append("QThreadEvents-")
                continue
            if feature == "no-resumed+":
                response.append("no-resumed-")
                continue
            if feature == "xmlRegisters=i386":
#Registers are provided via qXfer : features
                continue

        response.append("PacketSize=4096")

#Disable acks.
        response.append("QStartNoAckMode+")

#Instruct GDB to ask us for CPU features since only a subset of i386
#information is retrievable from XBDM.
        response.append("qXfer:features:read+")

        pkt = GDBPacket(";".join(response))

        self._state = self.STATE_CONNECTED
        self.send_packet(pkt)

    def _handle_query_thread_extra_info(self, pkt: GDBPacket):
        _, thread_id = pkt.data.split(",")
        thread_id = int(thread_id, 16)

        thread = self._debugger.get_thread(thread_id)
        info = f"{thread_id} {thread.last_stop_reason or 'Running'}"
        response = GDBPacket(bytes(info, "utf-8").hex())
        self.send_packet(response)

    def _handle_thread_info_start(self):
        self._thread_info_buffer = [thr.thread_id for thr in self._debugger.threads]

#Move the preferred thread to the front of the list.
        first_id = self._debugger.any_thread_id
        if first_id:
            self._thread_info_buffer.remove(first_id)
            self._thread_info_buffer.insert(0, first_id)

        if not self._thread_info_buffer:
            self.send_packet(GDBPacket("l"))
            return
        self._send_thread_info()

    def _handle_thread_info_continue(self):
        if not self._thread_info_buffer:
            self.send_packet(GDBPacket("l"))
            return
        self._send_thread_info()

    def _send_thread_info(self, send_all: bool = True):
        if send_all:
            threads = ",".join(["%x" % tid for tid in self._thread_info_buffer])
            self.send_packet(GDBPacket(f"m{threads}"))
            self._thread_info_buffer.clear()
            return

        self.send_packet(GDBPacket("m%x" % self._thread_info_buffer.pop()))

    def _handle_query_trace_status(self):
#TODO : Actually support trace experiments.
#GDBPacket("T0")
        self._send_empty()

    def _handle_query_current_thread_id(self):
        thread = self._debugger.active_thread
        if not thread:
            self._send_empty()
            return

        self.send_packet(GDBPacket("QC%x" % thread.thread_id))

    def _handle_features_read(self, pkt: GDBPacket):
        target_file, region = pkt.data[pkt.data.index("read:") + 5 :].split(":")
        start, length = region.split(",")
        start = int(start, 16)
        length = int(length, 16)
        end = start + length

        body = resources.RESOURCES.get(target_file)
        if not body:
            self._send_error(0)
            return

        logger.gdb(f"Read requested from {target_file} {start} - {end}")
        self._send_xfer_response(body, start, end)

    def _send_xfer_response(self, body: bytes, start: int, end: int):
        body_size = len(body)

        if start >= body_size:
            self.send_packet(GDBPacket("l"))
            return

        prefix = b"l" if end >= body_size else b"m"
        if end > body_size:
            end = body_size
        body = body[start:end]

        self.send_packet(GDBBinaryPacket(prefix + body))

    def _handle_vcont_query(self):
#Support
#c - continue
#C - continue with signal
#s - step
#S - step with signal
        self.send_packet(GDBPacket("vcont;c;s"))

    def _handle_vcont(self, args: str):
        if not args:
            logger.error("Unexpected empty vcont packet.")
            self._send_error(1)
            return

        for command in args.split(";"):
            if command == "c":
                self._debugger.continue_all()
                self._debugger.go()
                self._send_ok()
                continue

            if command[0] == "s":
                if ":" in command:
                    thread_id = command.split(":", 1)[1]
                    thread_id = int(thread_id, 16)
                    self._debugger.set_active_thread(thread_id)
                self._debugger.step_instruction()
                self._send_ok()
                continue

            logger.error(f"TODO: IMPLEMENT _handle_vcont {args} - subcommand {command}")
            self._send_error(errno.EBADMSG)
            break
#endif

bool GDBBridge::SendThreadStopPacket(const std::shared_ptr<Thread>& thread) {
  if (!gdb_ || !gdb_->IsConnected()) {
    return false;
  }

  if (!thread || !thread->stopped) {
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
      BOOST_LOG_TRIVIAL(error)
          << "Attempting to send stop notification for unknown stop reason";
      break;

    case SRT_THREAD_CREATED:
      // TODO: Send `create` if requested via QThreadEvents.
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
          BOOST_LOG_TRIVIAL(warning)
              << "Watchpoint of type Execute not supported by GDB.";

        case StopReasonDataBreakpoint::AT_UNKNOWN:
          break;
      }
      if (!access_type.empty()) {
        snprintf(buffer + len, 63 - len, ";%s:%08x;", access_type.c_str(),
                 reason->address);
      }
    } break;

    case SRT_ASSERTION:
    case SRT_DEBUGSTR:
    case SRT_BREAKPOINT:
    case SRT_SINGLE_STEP:
    case SRT_EXECUTION_STATE_CHANGED:
    case SRT_EXCEPTION:
    case SRT_THREAD_TERMINATED:
    case SRT_MODULE_LOADED:
    case SRT_SECTION_LOADED:
    case SRT_SECTION_UNLOADED:
    case SRT_RIP:
    case SRT_RIP_STOP:
      break;
  }

  gdb_->Send(GDBPacket(buffer));
  return true;
}
