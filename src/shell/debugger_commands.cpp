#include "debugger_commands.h"

#include "rdcp/xbdm_requests.h"
#include "util/parsing.h"
#include "xbox/debugger/xbdm_debugger.h"

Command::Result DebuggerCommandLaunch::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    std::cout << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  std::string command_line_args;
  parser.Parse(1, command_line_args);

  auto debugger = interface.Debugger();
  if (!debugger) {
    if (!interface.AttachDebugger()) {
      std::cout << "Failed to attach debugger." << std::endl;
      return HANDLED;
    }
    debugger = interface.Debugger();
    assert(debugger);
  }

  debugger->DebugXBE(path, command_line_args);
  return HANDLED;
}

Command::Result DebuggerCommandLaunchWait::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    std::cout << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  std::string command_line_args;
  parser.Parse(1, command_line_args);

  auto debugger = interface.Debugger();
  if (!debugger) {
    if (!interface.AttachDebugger()) {
      std::cout << "Failed to attach debugger." << std::endl;
    }
    return HANDLED;
  }

  debugger->DebugXBE(path, command_line_args, true);
  return HANDLED;
}

Command::Result DebuggerCommandAttach::operator()(
    XBOXInterface &interface, const std::vector<std::string> &) {
  if (!interface.AttachDebugger()) {
    std::cout << "Failed to attach debugger." << std::endl;
  }
  return HANDLED;
}

Command::Result DebuggerCommandDetach::operator()(
    XBOXInterface &interface, const std::vector<std::string> &) {
  interface.DetachDebugger();
  return HANDLED;
}

Command::Result DebuggerCommandRestart::operator()(
    XBOXInterface &interface, const std::vector<std::string> &) {
  auto debugger = interface.Debugger();
  if (!debugger) {
    std::cout << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  debugger->RestartAndAttach();

  return HANDLED;
}

Command::Result DebuggerCommandSetActiveThread::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  auto debugger = interface.Debugger();
  if (!debugger) {
    std::cout << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    std::cout << "Missing required thread_id argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  if (!debugger->SetActiveThread(thread_id)) {
    std::cout << "Invalid thread " << thread_id << std::endl;
    return HANDLED;
  }

  return HANDLED;
}

Command::Result DebuggerCommandStepFunction::operator()(
    XBOXInterface &interface, const std::vector<std::string> &) {
  auto debugger = interface.Debugger();
  if (!debugger) {
    std::cout << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  if (!debugger->StepFunction()) {
    std::cout << "Failed to step function" << std::endl;
    return HANDLED;
  }

  return HANDLED;
}

Command::Result DebuggerCommandGetThreads::operator()(
    XBOXInterface &interface, const std::vector<std::string> &) {
  auto debugger = interface.Debugger();
  if (!debugger) {
    std::cout << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  if (!debugger->FetchThreads()) {
    std::cout << "Failed to fetch threads." << std::endl;
    return HANDLED;
  }

  for (auto &thread : debugger->Threads()) {
    std::cout << *thread << std::endl;
  }

  return HANDLED;
}

Command::Result DebuggerCommandGetThreadInfo::operator()(
    XBOXInterface &interface, const std::vector<std::string> &) {
  auto debugger = interface.Debugger();
  if (!debugger) {
    std::cout << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  auto thread = debugger->ActiveThread();
  if (!thread) {
    std::cout << "No active thread." << std::endl;
    return HANDLED;
  }

  auto context = interface.Context();
  thread->FetchInfoSync(*context);
  thread->FetchContextSync(*context);
  thread->FetchFloatContextSync(*context);

  std::cout << *thread << std::endl;
  return HANDLED;
}

Command::Result DebuggerCommandHaltAll::operator()(
    XBOXInterface &interface, const std::vector<std::string> &) {
  auto debugger = interface.Debugger();
  if (!debugger) {
    std::cout << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  if (!debugger->FetchThreads()) {
    std::cout << "Failed to fetch threads." << std::endl;
    return HANDLED;
  }

  auto context = interface.Context();
  for (auto &thread : debugger->Threads()) {
    if (!thread->Halt(*context)) {
      std::cout << "Failed to halt thread " << thread->thread_id << std::endl;
    }
  }
  return HANDLED;
}

Command::Result DebuggerCommandHalt::operator()(
    XBOXInterface &interface, const std::vector<std::string> &) {
  auto debugger = interface.Debugger();
  if (!debugger) {
    std::cout << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  auto thread = debugger->ActiveThread();
  if (!thread) {
    std::cout << "No active thread." << std::endl;
    return HANDLED;
  }

  auto context = interface.Context();
  if (!thread->Halt(*context)) {
    std::cout << "Failed to halt thread " << thread->thread_id << std::endl;
  }

  return HANDLED;
}

Command::Result DebuggerCommandContinueAll::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  bool no_break_on_exception = parser.ArgExists("nobreak", "n", "false", "no");

  auto debugger = interface.Debugger();
  if (!debugger) {
    std::cout << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  if (!debugger->FetchThreads()) {
    std::cout << "Failed to fetch threads." << std::endl;
    return HANDLED;
  }

  auto context = interface.Context();
  for (auto &thread : debugger->Threads()) {
    if (!thread->Continue(*context, no_break_on_exception)) {
      std::cout << "Failed to continue thread " << thread->thread_id
                << std::endl;
    }
  }

  return HANDLED;
}

Command::Result DebuggerCommandContinue::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  bool no_break_on_exception = parser.ArgExists("nobreak", "n", "false", "no");

  auto debugger = interface.Debugger();
  if (!debugger) {
    std::cout << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  auto thread = debugger->ActiveThread();
  if (!thread) {
    std::cout << "No active thread." << std::endl;
    return HANDLED;
  }

  auto context = interface.Context();
  if (!thread->Continue(*context, no_break_on_exception)) {
    std::cout << "Failed to continue thread " << thread->thread_id << std::endl;
  }

  return HANDLED;
}

Command::Result DebuggerCommandSuspend::operator()(
    XBOXInterface &interface, const std::vector<std::string> &) {
  auto debugger = interface.Debugger();
  if (!debugger) {
    std::cout << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  auto thread = debugger->ActiveThread();
  if (!thread) {
    std::cout << "No active thread." << std::endl;
    return HANDLED;
  }

  auto context = interface.Context();
  if (!thread->Suspend(*context)) {
    std::cout << "Failed to continue thread " << thread->thread_id << std::endl;
  }

  return HANDLED;
}

Command::Result DebuggerCommandResume::operator()(
    XBOXInterface &interface, const std::vector<std::string> &) {
  auto debugger = interface.Debugger();
  if (!debugger) {
    std::cout << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  auto thread = debugger->ActiveThread();
  if (!thread) {
    std::cout << "No active thread." << std::endl;
    return HANDLED;
  }

  auto context = interface.Context();
  if (!thread->Resume(*context)) {
    std::cout << "Failed to continue thread " << thread->thread_id << std::endl;
  }

  return HANDLED;
}
