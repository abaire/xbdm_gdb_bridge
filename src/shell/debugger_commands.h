#ifndef XBDM_GDB_BRIDGE_DEBUGGER_COMMANDS_H
#define XBDM_GDB_BRIDGE_DEBUGGER_COMMANDS_H

#include <iostream>

#include "rdcp/xbdm_requests.h"
#include "shell/command.h"

struct DebuggerCommandLaunch : Command {
  DebuggerCommandLaunch()
      : Command(
            "<path_to_xbe> [commandline_args]\n"
            "\n"
            "Launches the given path, passing any remaining parameters as "
            "launch rest.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandLaunchWait : Command {
  DebuggerCommandLaunchWait()
      : Command(
            "<path_to_xbe> [commandline_args]\n"
            "\n"
            "Launches the given path, passing any remaining parameters as "
            "launch rest.\n"
            "A breakpoint will be set on the XBE entrypoint and execution will "
            "wait for a `go` command.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandAttach : Command {
  DebuggerCommandAttach()
      : Command(
            "\n"
            "Attach the debugger to the currently running process.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandDetach : Command {
  DebuggerCommandDetach()
      : Command(
            "\n"
            "Detach the debugger from the currently running process.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandRestart : Command {
  DebuggerCommandRestart()
      : Command(
            "\n"
            "Restarts the currently running process and breaks at the "
            "entrypoint.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandSetActiveThread : Command {
  DebuggerCommandSetActiveThread()
      : Command(
            "<thread_id>"
            "\n"
            "Sets the current thread context for the debugger.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandStepInstruction : Command {
  DebuggerCommandStepInstruction()
      : Command(
            "\n"
            "Step one instruction in the current thread.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandStepFunction : Command {
  DebuggerCommandStepFunction()
      : Command(
            "\n"
            "Step one C function call in the current thread.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandGetThreads : Command {
  DebuggerCommandGetThreads()
      : Command(
            "\n"
            "Print basic information about all threads.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandGetThreadInfo : Command {
  DebuggerCommandGetThreadInfo()
      : Command(
            "\n"
            "Print detailed information about the active thread.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandGetContext : Command {
  DebuggerCommandGetContext()
      : Command(
            "\n"
            "Print basic set of registers for the active thread context.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandGetFullContext : Command {
  DebuggerCommandGetFullContext()
      : Command(
            "\n"
            "Print all available registers for the active thread context.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandHaltAll : Command {
  DebuggerCommandHaltAll()
      : Command(
            "\n"
            "Halts all threads.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandHalt : Command {
  DebuggerCommandHalt()
      : Command(
            "\n"
            "Halts active thread.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandContinueAll : Command {
  DebuggerCommandContinueAll()
      : Command(
            "[no_break_on_exceptions]\n"
            "\n"
            "Continues all halted threads in the debugger.\n"
            "\n"
            "no_break_on_exceptions - if true, do not break on exceptions when "
            "continuing.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandContinue : Command {
  DebuggerCommandContinue()
      : Command(
            "[no_break_on_exceptions]\n"
            "\n"
            "Continues the active thread.\n"
            "\n"
            "no_break_on_exceptions - if true, do not break on exceptions when "
            "continuing.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandSuspend : Command {
  DebuggerCommandSuspend()
      : Command(
            "\n"
            "Suspends (or raises the suspend count on) the active thread.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

struct DebuggerCommandResume : Command {
  DebuggerCommandResume()
      : Command(
            "\n"
            "Suspends (or raises the suspend count on) the active thread.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    // TODO: Implement me.
    return Result::EXIT_REQUESTED;
  }
};

#endif  // XBDM_GDB_BRIDGE_DEBUGGER_COMMANDS_H
