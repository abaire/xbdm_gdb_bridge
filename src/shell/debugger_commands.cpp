#include "debugger_commands.h"

#include <capstone/capstone.h>

#include <iomanip>

#include "commands.h"
#include "rdcp/xbdm_requests.h"
#include "shell/file_util.h"
#include "util/parsing.h"
#include "xbox/debugger/debugger_xbox_interface.h"
#include "xbox/debugger/xbdm_debugger.h"

//! Maximum number of bytes in an i386 instruction.
static constexpr auto kMaxInstructionBytes = 15;
//! The number of instructions to disassemble in the disassembly command.
static constexpr uint32_t kDisassemblyOpCount = 16;

static bool DebugXBE(XBOXInterface& base_interface, const ArgParser& args,
                     bool wait_forever, bool break_at_start,
                     std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  std::string path;
  if (!args.Parse(0, path)) {
    out << "Missing required path argument." << std::endl;
    return false;
  }
  std::string command_line_args;
  args.Parse(1, command_line_args);

  if (!interface.AttachDebugger()) {
    out << "Failed to attach debugger." << std::endl;
    return true;
  }
  auto debugger = interface.Debugger();
  assert(debugger);

  if (!debugger->DebugXBE(EnsureXFATStylePath(path), command_line_args,
                          wait_forever, break_at_start)) {
    out << "Failed to launch XBE" << std::endl;
  }
  return true;
}

Command::Result DebuggerCommandRun::operator()(XBOXInterface& interface,
                                               const ArgParser& args,
                                               std::ostream& out) {
  if (!DebugXBE(interface, args, false, false, out)) {
    PrintUsage();
  }
  return HANDLED;
}

Command::Result DebuggerCommandLaunch::operator()(XBOXInterface& interface,
                                                  const ArgParser& args,
                                                  std::ostream& out) {
  if (!DebugXBE(interface, args, false, true, out)) {
    PrintUsage();
  }
  return HANDLED;
}

Command::Result DebuggerCommandLaunchWait::operator()(XBOXInterface& interface,
                                                      const ArgParser& args,
                                                      std::ostream& out) {
  if (!DebugXBE(interface, args, true, true, out)) {
    PrintUsage();
  }
  return HANDLED;
}

Command::Result DebuggerCommandAttach::operator()(XBOXInterface& base_interface,
                                                  const ArgParser&,
                                                  std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  if (!interface.AttachDebugger()) {
    out << "Failed to attach debugger." << std::endl;
  }
  return HANDLED;
}

Command::Result DebuggerCommandDetach::operator()(XBOXInterface& base_interface,
                                                  const ArgParser&,
                                                  std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  interface.DetachDebugger();
  return HANDLED;
}

Command::Result DebuggerCommandRestart::operator()(
    XBOXInterface& base_interface, const ArgParser&, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  debugger->RestartAndAttach();

  return HANDLED;
}

Command::Result DebuggerCommandDisassemble::operator()(
    XBOXInterface& base_interface, const ArgParser& args, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  uint32_t address = 0;

  bool use_eip = true;
  if (args.Parse(0, address)) {
    use_eip = false;
  }

  if (use_eip) {
    auto thread = debugger->ActiveThread();
    if (!thread) {
      out << "No address provided and no active thread." << std::endl;
      return HANDLED;
    }

    auto context = interface.Context();
    if (!thread->FetchContextSync(*context)) {
      out << "Failed to fetch thread context." << std::endl;
      return HANDLED;
    }

    if (!thread->context || !thread->context->eip.has_value()) {
      out << "Thread context incomplete (missing EIP)." << std::endl;
      return HANDLED;
    }
    address = static_cast<uint32_t>(thread->context->eip.value());
  }

  uint32_t bytes_to_read = kDisassemblyOpCount * kMaxInstructionBytes;
  auto memory = debugger->GetMemory(address, bytes_to_read);

  if (!memory) {
    out << "Failed to read memory at 0x" << std::hex << address << std::dec
        << std::endl;
    return HANDLED;
  }

  csh handle;
  if (cs_open(CS_ARCH_X86, CS_MODE_32, &handle) != CS_ERR_OK) {
    out << "Failed to initialize Capstone." << std::endl;
    return HANDLED;
  }

  struct CapstoneGuard {
    csh h;
    ~CapstoneGuard() { cs_close(&h); }
  } guard{handle};

  cs_insn* insn;
  size_t count_dis = cs_disasm(handle, memory->data(), memory->size(), address,
                               kDisassemblyOpCount, &insn);

  if (count_dis > 0) {
    for (size_t i = 0; i < count_dis; i++) {
      out << "0x" << std::hex << insn[i].address << ": ";
      for (int j = 0; j < insn[i].size; ++j) {
        out << std::setw(2) << std::setfill('0') << (int)insn[i].bytes[j]
            << " ";
      }
      for (int j = insn[i].size; j < kMaxInstructionBytes; ++j) {
        out << "   ";
      }
      out << insn[i].mnemonic << " " << insn[i].op_str << std::dec << std::endl;
    }
    cs_free(insn, count_dis);
  } else {
    out << "Failed to disassemble." << std::endl;
  }

  return HANDLED;
}

Command::Result DebuggerCommandSetActiveThread::operator()(
    XBOXInterface& base_interface, const ArgParser& args, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  uint32_t thread_id;
  if (!args.Parse(0, thread_id)) {
    out << "Missing required thread_id argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  if (!debugger->SetActiveThread(thread_id)) {
    out << "Invalid thread " << thread_id << std::endl;
    return HANDLED;
  }

  return HANDLED;
}

Command::Result DebuggerCommandStepInstruction::operator()(
    XBOXInterface& base_interface, const ArgParser&, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  if (!debugger->StepInstruction()) {
    out << "Failed to step instruction" << std::endl;
    return HANDLED;
  }

  return HANDLED;
}

Command::Result DebuggerCommandStepFunction::operator()(
    XBOXInterface& base_interface, const ArgParser&, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  if (!debugger->StepFunction()) {
    out << "Failed to step function" << std::endl;
    return HANDLED;
  }

  return HANDLED;
}

namespace {

Command::Result PrintThreadContext(const std::shared_ptr<Thread>& thread,
                                   DebuggerXBOXInterface& interface,
                                   const ArgParser& args, std::ostream& out) {
  std::vector<std::string> augmented_args;
  augmented_args.push_back(std::to_string(thread->thread_id));
  augmented_args.insert(augmented_args.end(), args.begin(), args.end());

  ArgParser delegate_args("getcontext", augmented_args);
  auto context_command = CommandGetContext();
  return context_command(interface, delegate_args, out);
}

}  // namespace

Command::Result DebuggerCommandGetThreads::operator()(
    XBOXInterface& base_interface, const ArgParser& args, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  if (!debugger->FetchThreads()) {
    out << "Failed to fetch threads." << std::endl;
    return HANDLED;
  }

  bool print_context = args.ArgExists("context", "ctx", "c");

  auto active_thread_id = debugger->ActiveThreadID();
  for (auto& thread : debugger->Threads()) {
    if (active_thread_id && thread->thread_id == *active_thread_id) {
      out << "[Active thread]" << std::endl;
    }
    out << *thread << std::endl;

    if (print_context) {
      PrintThreadContext(thread, interface, args, out);
    }
  }

  return HANDLED;
}

Command::Result DebuggerCommandWhichThread::operator()(
    XBOXInterface& base_interface, const ArgParser& args, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  if (!debugger->FetchThreads()) {
    out << "Failed to fetch threads." << std::endl;
    return HANDLED;
  }

  uint32_t address;
  if (!args.Parse(0, address)) {
    out << "Missing required `address` argument." << std::endl;
    return HANDLED;
  }

  for (auto& thread : debugger->Threads()) {
    if (thread->HasStack(address)) {
      out << *thread << std::endl;
      PrintThreadContext(thread, interface, {}, out);
      return HANDLED;
    }
  }

  out << "Failed to find a thread with a stack containing " << std::hex
      << address << std::dec << std::endl;
  return HANDLED;
}

Command::Result DebuggerCommandGetThreadInfo::operator()(
    XBOXInterface& base_interface, const ArgParser&, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  auto thread = debugger->ActiveThread();
  if (!thread) {
    out << "No active thread." << std::endl;
    return HANDLED;
  }

  auto context = interface.Context();
  thread->FetchInfoSync(*context);
  thread->FetchContextSync(*context);
  thread->FetchFloatContextSync(*context);

  out << *thread << std::endl;
  return HANDLED;
}

Command::Result DebuggerCommandGetThreadInfoAndContext::operator()(
    XBOXInterface& base_interface, const ArgParser& args, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  auto thread = debugger->ActiveThread();
  if (!thread) {
    out << "No active thread." << std::endl;
    return HANDLED;
  }

  auto context = interface.Context();
  thread->FetchInfoSync(*context);
  thread->FetchContextSync(*context);
  thread->FetchFloatContextSync(*context);

  out << *thread << std::endl;

  return PrintThreadContext(thread, interface, args, out);
}

Command::Result DebuggerCommandSetAutoInfo::operator()(
    XBOXInterface& base_interface, const ArgParser& args, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  bool disable = args.ArgExists("d", "disable", "off");

  debugger->SetDisplayExpandedBreakpointOutput(!disable);

  out << "OK" << std::endl;
  return HANDLED;
}

Command::Result DebuggerCommandHaltAll::operator()(
    XBOXInterface& base_interface, const ArgParser&, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  debugger->HaltAll();

  return HANDLED;
}

Command::Result DebuggerCommandHalt::operator()(XBOXInterface& base_interface,
                                                const ArgParser&,
                                                std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  auto thread = debugger->ActiveThread();
  if (!thread) {
    out << "No active thread." << std::endl;
    return HANDLED;
  }

  auto context = interface.Context();
  if (!thread->Halt(*context)) {
    out << "Failed to halt thread " << thread->thread_id << std::endl;
  }

  return HANDLED;
}

Command::Result DebuggerCommandContinueAll::operator()(
    XBOXInterface& base_interface, const ArgParser& args, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  bool no_break_on_exception =
      args.ArgExists("nobreak", "n", "false", "no", "no_break_on_exception");

  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  debugger->ContinueAll(no_break_on_exception);

  return HANDLED;
}

Command::Result DebuggerCommandContinue::operator()(
    XBOXInterface& base_interface, const ArgParser& args, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  bool no_break_on_exception = args.ArgExists("nobreak", "n", "false", "no");

  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  auto thread = debugger->ActiveThread();
  if (!thread) {
    out << "No active thread." << std::endl;
    return HANDLED;
  }

  auto context = interface.Context();
  if (!thread->Continue(*context, no_break_on_exception)) {
    out << "Failed to continue thread " << thread->thread_id << std::endl;
  }

  return HANDLED;
}

Command::Result DebuggerCommandSuspend::operator()(
    XBOXInterface& base_interface, const ArgParser&, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  auto thread = debugger->ActiveThread();
  if (!thread) {
    out << "No active thread." << std::endl;
    return HANDLED;
  }

  auto context = interface.Context();
  if (!thread->Suspend(*context)) {
    out << "Failed to continue thread " << thread->thread_id << std::endl;
  }

  return HANDLED;
}

Command::Result DebuggerCommandResume::operator()(XBOXInterface& base_interface,
                                                  const ArgParser&,
                                                  std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  auto thread = debugger->ActiveThread();
  if (!thread) {
    out << "No active thread." << std::endl;
    return HANDLED;
  }

  auto context = interface.Context();
  if (!thread->Resume(*context)) {
    out << "Failed to continue thread " << thread->thread_id << std::endl;
  }

  return HANDLED;
}

Command::Result DebuggerCommandGetModules::operator()(
    XBOXInterface& base_interface, const ArgParser&, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  auto modules = debugger->Modules();
  for (auto& module : modules) {
    out << *module.second << std::endl;
  }
  return HANDLED;
}

Command::Result DebuggerCommandGetSections::operator()(
    XBOXInterface& base_interface, const ArgParser&, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  auto module_ranges = debugger->ModuleRanges();

  auto sections = debugger->Sections();
  for (auto& section : sections) {
    auto it = module_ranges.upper_bound({section.first, 0});
    if (it != module_ranges.begin()) {
      --it;
      if (section.first < it->first.second) {
        out << it->second->name << ": ";
      }
    }
    out << *section.second << std::endl;
  }
  return HANDLED;
}

Command::Result DebuggerCommandContinueAllAndGo::operator()(
    XBOXInterface& base_interface, const ArgParser& args, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  ArgParser parser(args);
  bool no_break_on_exception =
      parser.ArgExists("nobreak", "n", "false", "no", "no_break_on_exception");

  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  if (!debugger->Go()) {
    out << "'go' command failed." << std::endl;
    return HANDLED;
  }

  debugger->ContinueAll(no_break_on_exception);

  return HANDLED;
}

Command::Result DebuggerCommandGuessBackTrace::operator()(
    XBOXInterface& base_interface, const ArgParser& args, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  uint32_t thread_id;
  if (!args.Parse(0, thread_id)) {
    auto active_thread = debugger->ActiveThreadID();
    if (!active_thread) {
      out << "No active thread and no thread_id provided." << std::endl;
      return HANDLED;
    }
    thread_id = *active_thread;
  }

  auto frames = debugger->GuessBackTrace(thread_id);
  if (frames.empty()) {
    out << "No frames found." << std::endl;
    return HANDLED;
  }

  int index = 0;
  auto sections = debugger->Sections();
  auto module_ranges = debugger->ModuleRanges();

  for (const auto& frame : frames) {
    uint32_t addr = frame.address;
    out << "#" << std::setw(2) << index++ << " 0x" << std::hex << addr
        << std::dec;

    std::string location;
    auto section_it = sections.upper_bound(addr);
    if (section_it != sections.begin()) {
      --section_it;
      const auto& section = section_it->second;
      if (addr >= section->base_address &&
          addr < section->base_address + section->size) {
        location = section->name;

        auto it = module_ranges.upper_bound({section->base_address, 0});
        if (it != module_ranges.begin()) {
          --it;
          if (section->base_address >= it->first.first &&
              section->base_address < it->first.second) {
            location = it->second->name + "!" + location;
          }
        }
      }
    }

    if (!location.empty()) {
      out << " in " << location;
    }

    if (frame.call_target) {
      out << " ";
      if (frame.is_suspicious) {
        out << "? " << "0x" << std::hex << *frame.call_target << " ?"
            << std::dec;
      } else {
        out << "[ 0x" << std::hex << *frame.call_target << " ]" << std::dec;
      }
    }

    if (frame.is_indirect_call) {
      out << " [calc]";
    }
    out << std::endl;
  }

  return HANDLED;
}
