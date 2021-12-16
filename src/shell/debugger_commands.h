#ifndef XBDM_GDB_BRIDGE_DEBUGGER_COMMANDS_H
#define XBDM_GDB_BRIDGE_DEBUGGER_COMMANDS_H

#include <iostream>

#include "shell/command.h"

class XBOXInterface;

struct DebuggerCommandRun : Command {
  DebuggerCommandRun()
      : Command(
            "<path_to_xbe> [commandline_args]\n"
            "\n"
            "Launches the XBE at the given path, passing any remaining "
            "parameters as "
            "command line arguments. Does not set any breakpoints.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct DebuggerCommandLaunch : Command {
  DebuggerCommandLaunch()
      : Command(
            "<path_to_xbe> [commandline_args]\n"
            "\n"
            "Launches the given path, passing any remaining parameters as "
            "command line arguments.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct DebuggerCommandLaunchWait : Command {
  DebuggerCommandLaunchWait()
      : Command(
            "<path_to_xbe> [commandline_args]\n"
            "\n"
            "Launches the given path, passing any remaining parameters as "
            "command line arguments.\n"
            "A breakpoint will be set on the XBE entrypoint and execution will "
            "wait for a `go` command.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct DebuggerCommandAttach : Command {
  DebuggerCommandAttach()
      : Command(
            "\n"
            "Attach the debugger to the currently running process.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct DebuggerCommandDetach : Command {
  DebuggerCommandDetach()
      : Command(
            "\n"
            "Detach the debugger from the currently running process.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct DebuggerCommandRestart : Command {
  DebuggerCommandRestart()
      : Command(
            "\n"
            "Restarts the currently running process and breaks at the "
            "entrypoint.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct DebuggerCommandSetActiveThread : Command {
  DebuggerCommandSetActiveThread()
      : Command(
            "<thread_id>"
            "\n"
            "Sets the current thread context for the debugger.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

// struct DebuggerCommandStepInstruction : Command {
//   DebuggerCommandStepInstruction()
//       : Command(
//             "\n"
//             "Step one instruction in the current thread.") {}
//   Result operator()(XBOXInterface &interface,
//                     const std::vector<std::string> &) override {
//     // TODO: Implement me.
//     return HANDLED;
//   }
// };

struct DebuggerCommandStepFunction : Command {
  DebuggerCommandStepFunction()
      : Command(
            "\n"
            "Step one C function call in the current thread.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct DebuggerCommandGetThreads : Command {
  DebuggerCommandGetThreads()
      : Command(
            "\n"
            "Print basic information about all threads.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct DebuggerCommandGetThreadInfo : Command {
  DebuggerCommandGetThreadInfo()
      : Command(
            "\n"
            "Print detailed information about the active thread.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct DebuggerCommandHaltAll : Command {
  DebuggerCommandHaltAll()
      : Command(
            "\n"
            "Halts all threads.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct DebuggerCommandHalt : Command {
  DebuggerCommandHalt()
      : Command(
            "\n"
            "Halts active thread.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct DebuggerCommandContinueAll : Command {
  DebuggerCommandContinueAll()
      : Command(
            "['n'o_break_on_exceptions]\n"
            "\n"
            "Continues all halted threads in the debugger.\n"
            "\n"
            "no_break_on_exceptions - if 'n', do not break on exceptions when "
            "continuing.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct DebuggerCommandContinue : Command {
  DebuggerCommandContinue()
      : Command(
            "['n'o_break_on_exceptions]\n"
            "\n"
            "Continues the active thread.\n"
            "\n"
            "no_break_on_exceptions - if 'n', do not break on exceptions when "
            "continuing.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct DebuggerCommandSuspend : Command {
  DebuggerCommandSuspend()
      : Command(
            "\n"
            "Suspends (or raises the suspend count on) the active thread.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct DebuggerCommandResume : Command {
  DebuggerCommandResume()
      : Command(
            "\n"
            "Suspends (or raises the suspend count on) the active thread.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct DebuggerCommandGetModules : Command {
  DebuggerCommandGetModules()
      : Command(
            "\n"
            "Print basic information about loaded modules.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct DebuggerCommandGetSections : Command {
  DebuggerCommandGetSections()
      : Command(
            "\n"
            "Print basic information about loaded sections.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

#endif  // XBDM_GDB_BRIDGE_DEBUGGER_COMMANDS_H
