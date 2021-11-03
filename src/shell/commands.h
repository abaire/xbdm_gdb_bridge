#ifndef XBDM_GDB_BRIDGE_COMMANDS_H
#define XBDM_GDB_BRIDGE_COMMANDS_H

#include <iostream>

#include "shell/command.h"
#include "rdcp/xbdm_requests.h"

struct CommandAltAddr : Command {
  CommandAltAddr() : Command("Prints 'Game Configuration' IP information.") {}
  Result operator()(XBOXInterface &interface,
                            const std::vector<std::string> &) override {
    auto request = std::make_shared<AltAddr>();
    interface.SendCommandSync(request);
    if (request->IsOK()) {
      std::cout << "Alternate IP address: " << request->address_string << std::endl;
    }

    return Result::HANDLED;
  }
};

struct CommandBreak : Command {
  CommandBreak() : Command(
                       "<subcommand> [subcommand_args]\n"
            "\n"
            "    Adds or removes a breakpoint.\n"
            "\n"
            "    subcommands:\n"
            "      clearall - Clears all breakpoints\n"
            "      start - Sets a breakpoint at program entry. Only valid if the remote is in state \"execution pending\".\n"
            "      [-]addr <address> - Breaks on execution at the given address.\n"
            "      [-]read <address> <length> - Breaks on read access to the given memory range.\n"
            "      [-]write <address> <length> - Breaks on write access to the given memory range.\n"
            "      [-]execute <address> <length> - Breaks on execution within the given memory range.\n"
            "\n"
            "    Subcommands with [-] can be prefixed with '-' to disable a previously set breakpoint.\n"
            "    E.g., addr 0x12345   # sets a breakpoint at 0x12345\n"
            "          -addr 0x12345  # clears the breakpoint") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};


#endif  // XBDM_GDB_BRIDGE_COMMANDS_H
