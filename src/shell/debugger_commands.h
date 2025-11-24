#ifndef XBDM_GDB_BRIDGE_DEBUGGER_COMMANDS_H
#define XBDM_GDB_BRIDGE_DEBUGGER_COMMANDS_H

#include <iostream>

#include "shell/command.h"

class XBOXInterface;

struct DebuggerCommandRun : Command {
  DebuggerCommandRun()
      : Command("Run an XBE without debugging.",
                "<path_to_xbe> [commandline_args]\n"
                "\n"
                "Launch the XBE at the given path, passing any remaining "
                "parameters as "
                "command line arguments. Does not set any breakpoints.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct DebuggerCommandLaunch : Command {
  DebuggerCommandLaunch()
      : Command("Launch an XBE with debugging.",
                "<path_to_xbe> [commandline_args]\n"
                "\n"
                "Launch the given path, passing any remaining parameters as "
                "command line arguments.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct DebuggerCommandLaunchWait : Command {
  DebuggerCommandLaunchWait()
      : Command(
            "Launch XBE with debugging and break at the entrypoint.",
            "<path_to_xbe> [commandline_args]\n"
            "\n"
            "Launch the given path, passing any remaining parameters as "
            "command line arguments.\n"
            "A breakpoint will be set on the XBE entrypoint and execution will "
            "wait for a `go` command.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct DebuggerCommandAttach : Command {
  DebuggerCommandAttach()
      : Command("Attach the debugger to the currently running process.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DebuggerCommandDetach : Command {
  DebuggerCommandDetach()
      : Command("Detach the debugger from the currently running process.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DebuggerCommandRestart : Command {
  DebuggerCommandRestart()
      : Command(
            "Restart the currently running process and breaks at the "
            "entrypoint.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DebuggerCommandSetActiveThread : Command {
  DebuggerCommandSetActiveThread()
      : Command(
            "Set active debugger thread.",
            "<thread_id>"
            "\n"
            "Set the current thread context for the debugger to `thread_id`.") {
  }
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct DebuggerCommandStepInstruction : Command {
  DebuggerCommandStepInstruction()
      : Command(
            "\n"
            "Step one instruction in the current thread.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DebuggerCommandStepFunction : Command {
  DebuggerCommandStepFunction()
      : Command("Step one C function call in the current thread.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DebuggerCommandGetThreads : Command {
  DebuggerCommandGetThreads()
      : Command("Print basic information about all threads.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DebuggerCommandGetThreadInfo : Command {
  DebuggerCommandGetThreadInfo()
      : Command("Print detailed information about the active thread.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DebuggerCommandGetThreadInfoAndContext : Command {
  DebuggerCommandGetThreadInfoAndContext()
      : Command(
            "Print detailed information about the active thread including "
            "current register values.\n"
            "\n"
            "See help getcontext for arguments.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DebuggerCommandSetAutoInfo : Command {
  DebuggerCommandSetAutoInfo()
      : Command(
            "Enable/disable automatic output of thread context on "
            "breakpoints.\n"
            "[basic] [off]\n"
            "\n"
            "Modifies automatic output at breakpoints.\n"
            "\n"
            "[basic] (default) - Automatically run `getcontext` when "
            "breakpoints are hit.\n"
            "[off] - Disable automatic info output.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DebuggerCommandHaltAll : Command {
  DebuggerCommandHaltAll() : Command("Halt all threads.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DebuggerCommandHalt : Command {
  DebuggerCommandHalt() : Command("Halt the current debugger thread.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DebuggerCommandContinueAll : Command {
  DebuggerCommandContinueAll()
      : Command(
            "Continue halted threads.",
            "['n'o_break_on_exceptions]\n"
            "\n"
            "Continue all halted threads in the debugger.\n"
            "\n"
            "no_break_on_exceptions - if 'n', do not break on exceptions when "
            "continuing.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct DebuggerCommandContinue : Command {
  DebuggerCommandContinue()
      : Command(
            "Continue the current thread.",
            "['n'o_break_on_exceptions]\n"
            "\n"
            "Continue the debugger's current thread.\n"
            "\n"
            "no_break_on_exceptions - if 'n', do not break on exceptions when "
            "continuing.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct DebuggerCommandSuspend : Command {
  DebuggerCommandSuspend()
      : Command(
            "Suspend (or raises the suspend count on) the current thread.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DebuggerCommandResume : Command {
  DebuggerCommandResume()
      : Command("Resume (or reduce the suspend count on) the current thread.") {
  }
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DebuggerCommandGetModules : Command {
  DebuggerCommandGetModules()
      : Command("Print basic information about loaded modules.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DebuggerCommandGetSections : Command {
  DebuggerCommandGetSections()
      : Command("Print basic information about loaded sections.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DebuggerCommandContinueAllAndGo : Command {
  DebuggerCommandContinueAllAndGo()
      : Command(
            "Combines 'go' and /continueall.",
            "['n'o_break_on_exceptions]\n"
            "\n"
            "'go' and then continue all halted threads in the debugger.\n"
            "\n"
            "no_break_on_exceptions - if 'n', do not break on exceptions when "
            "continuing.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

#endif  // XBDM_GDB_BRIDGE_DEBUGGER_COMMANDS_H
