#ifndef XBDM_GDB_BRIDGE_COMMANDS_H
#define XBDM_GDB_BRIDGE_COMMANDS_H

#include <iostream>

#include "rdcp/xbdm_requests.h"
#include "shell/command.h"

struct CommandAltAddr : Command {
  CommandAltAddr() : Command("Prints 'Game Configuration' IP information.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    auto request = std::make_shared<AltAddr>();
    interface.SendCommandSync(request);
    if (request->IsOK()) {
      std::cout << "Alternate IP address: " << request->address_string
                << std::endl;
    }

    return Result::HANDLED;
  }
};

struct CommandBreak : Command {
  CommandBreak()
      : Command(
            "<subcommand> [subcommand_args]\n"
            "\n"
            "Adds or removes a breakpoint.\n"
            "\n"
            "subcommands:\n"
            "  clearall - Clears all breakpoints\n"
            "  start - Sets a breakpoint at program entry. Only valid if "
            "the remote is in state \"execution pending\".\n"
            "  [-]addr <address> - Breaks on execution at the given "
            "address.\n"
            "  [-]read <address> <length> - Breaks on read access to the "
            "given memory range.\n"
            "  [-]write <address> <length> - Breaks on write access to the "
            "given memory range.\n"
            "  [-]execute <address> <length> - Breaks on execution within "
            "the given memory range.\n"
            "\n"
            "Subcommands with [-] can be prefixed with '-' to disable a "
            "previously set breakpoint.\n"
            "    E.g., addr 0x12345   # sets a breakpoint at 0x12345\n"
            "          -addr 0x12345  # clears the breakpoint") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandBye : Command {
  CommandBye() : Command("Closes the connection gracefully.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct CommandContinue : Command {
  CommandContinue()
      : Command(
            "<thread_id> [break_on_exception=false]\n"
            "\n"
            "Continues execution of the given thread.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandDebugOptions : Command {
  CommandDebugOptions()
      : Command(
            "[crashdump | dpctrace] [...]\n"
            "\n"
            "If no arguments are given, retrieves the currently active debug "
            "options.\n"
            "If at least one argument is given, enables that debug option and "
            "disables any options that are not given.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandDebugger : Command {
  CommandDebugger()
      : Command(
            "[disable]\n"
            "\n"
            "If no args are given, notifies XBDM that this channel intends to "
            "act as a debugger.\n"
            "If 'disable' is given, disables the previously set debugger "
            "flag.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandDelete : Command {
  CommandDelete()
      : Command(
            "<path> [-r]\n"
            "\n"
            "Deletes the given path.\n"
            "If -r is given, deletes a the given directory recursively.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandDirList : Command {
  CommandDirList()
      : Command(
            "<path>\n"
            "\n"
            "Returns a list of items in `path`.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

#endif  // XBDM_GDB_BRIDGE_COMMANDS_H
