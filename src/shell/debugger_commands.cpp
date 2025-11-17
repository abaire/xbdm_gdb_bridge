#include "debugger_commands.h"

#include "commands.h"
#include "rdcp/xbdm_requests.h"
#include "shell/file_util.h"
#include "util/parsing.h"
#include "xbox/debugger/debugger_xbox_interface.h"
#include "xbox/debugger/xbdm_debugger.h"

static bool DebugXBE(XBOXInterface& base_interface,
                     const std::vector<std::string>& args, bool wait_forever,
                     bool break_at_start, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    out << "Missing required path argument." << std::endl;
    return false;
  }
  std::string command_line_args;
  parser.Parse(1, command_line_args);

  if (!interface.AttachDebugger()) {
    out << "Failed to attach debugger." << std::endl;
    return true;
  }
  auto debugger = interface.Debugger();
  assert(debugger);

  debugger->DebugXBE(EnsureXFATStylePath(path), command_line_args, wait_forever,
                     break_at_start);
  return true;
}

Command::Result DebuggerCommandRun::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  if (!DebugXBE(interface, args, false, false, out)) {
    PrintUsage();
  }
  return HANDLED;
}

Command::Result DebuggerCommandLaunch::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  if (!DebugXBE(interface, args, false, true, out)) {
    PrintUsage();
  }
  return HANDLED;
}

Command::Result DebuggerCommandLaunchWait::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  if (!DebugXBE(interface, args, true, true, out)) {
    PrintUsage();
  }
  return HANDLED;
}

Command::Result DebuggerCommandAttach::operator()(
    XBOXInterface& base_interface, const std::vector<std::string>&,
    std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  if (!interface.AttachDebugger()) {
    out << "Failed to attach debugger." << std::endl;
  }
  return HANDLED;
}

Command::Result DebuggerCommandDetach::operator()(
    XBOXInterface& base_interface, const std::vector<std::string>&,
    std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  interface.DetachDebugger();
  return HANDLED;
}

Command::Result DebuggerCommandRestart::operator()(
    XBOXInterface& base_interface, const std::vector<std::string>&,
    std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  debugger->RestartAndAttach();

  return HANDLED;
}

Command::Result DebuggerCommandSetActiveThread::operator()(
    XBOXInterface& base_interface, const std::vector<std::string>& args,
    std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
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

Command::Result DebuggerCommandStepFunction::operator()(
    XBOXInterface& base_interface, const std::vector<std::string>&,
    std::ostream& out) {
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

Command::Result DebuggerCommandGetThreads::operator()(
    XBOXInterface& base_interface, const std::vector<std::string>&,
    std::ostream& out) {
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

  auto active_thread_id = debugger->ActiveThreadID();
  for (auto& thread : debugger->Threads()) {
    if (thread->thread_id == active_thread_id) {
      out << "[Active thread]" << std::endl;
    }
    out << *thread << std::endl;
  }

  return HANDLED;
}

Command::Result DebuggerCommandGetThreadInfo::operator()(
    XBOXInterface& base_interface, const std::vector<std::string>&,
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
  thread->FetchInfoSync(*context);
  thread->FetchContextSync(*context);
  thread->FetchFloatContextSync(*context);

  out << *thread << std::endl;
  return HANDLED;
}

Command::Result DebuggerCommandGetThreadInfoAndContext::operator()(
    XBOXInterface& base_interface, const std::vector<std::string>& args,
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
  thread->FetchInfoSync(*context);
  thread->FetchContextSync(*context);
  thread->FetchFloatContextSync(*context);

  out << *thread << std::endl;

  std::vector<std::string> augmented_args;
  augmented_args.push_back(std::to_string(thread->thread_id));
  augmented_args.insert(augmented_args.end(), args.begin(), args.end());
  auto context_command = CommandGetContext();
  return context_command(interface, augmented_args, out);
}

Command::Result DebuggerCommandHaltAll::operator()(
    XBOXInterface& base_interface, const std::vector<std::string>&,
    std::ostream& out) {
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
                                                const std::vector<std::string>&,
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
    XBOXInterface& base_interface, const std::vector<std::string>& args,
    std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  ArgParser parser(args);
  bool no_break_on_exception =
      parser.ArgExists("nobreak", "n", "false", "no", "no_break_on_exception");

  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  debugger->ContinueAll(no_break_on_exception);

  return HANDLED;
}

Command::Result DebuggerCommandContinue::operator()(
    XBOXInterface& base_interface, const std::vector<std::string>& args,
    std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  ArgParser parser(args);
  bool no_break_on_exception = parser.ArgExists("nobreak", "n", "false", "no");

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
    XBOXInterface& base_interface, const std::vector<std::string>&,
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
  if (!thread->Suspend(*context)) {
    out << "Failed to continue thread " << thread->thread_id << std::endl;
  }

  return HANDLED;
}

Command::Result DebuggerCommandResume::operator()(
    XBOXInterface& base_interface, const std::vector<std::string>&,
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
    XBOXInterface& base_interface, const std::vector<std::string>&,
    std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  auto modules = debugger->Modules();
  for (auto& module : modules) {
    out << *module << std::endl;
  }
  return HANDLED;
}

Command::Result DebuggerCommandGetSections::operator()(
    XBOXInterface& base_interface, const std::vector<std::string>&,
    std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  auto sections = debugger->Sections();
  for (auto& section : sections) {
    out << *section << std::endl;
  }
  return HANDLED;
}

Command::Result DebuggerCommandContinueAllAndGo::operator()(
    XBOXInterface& base_interface, const std::vector<std::string>& args,
    std::ostream& out) {
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
