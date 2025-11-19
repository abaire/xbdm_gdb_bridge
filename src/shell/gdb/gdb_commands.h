#ifndef XBDM_GDB_BRIDGE_GDB_COMMANDS_H
#define XBDM_GDB_BRIDGE_GDB_COMMANDS_H

#include <iostream>

#include "shell/command.h"

class Shell;

/**
 * Registers GDB-related commands to the given Shell.
 * @param shell Shell instance to which GDB commands should be added.
 */
void RegisterGDBCommands(Shell& shell);

struct ShellCommandGDB : Command {
  ShellCommandGDB()
      : Command(
            "Start GDB <-> XBDM service.",
            "[IP]:port [xbe_launch_path]\n"
            "\n"
            "Start a GDB server, allowing GDB to communicate with the XBDM "
            "target.\n"
            "\n"
            "[IP]:port - The IP and port at which GDB can connect.\n"
            "              The IP is optional, where the default behavior is "
            "bind to all local interfaces.\n"
            "[xbe_launch_path] - An XBOX path to an XBE (or directory "
            "containing a default.xbe) that should be launched "
            "when a GDB debugger first connects.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

#endif  // XBDM_GDB_BRIDGE_GDB_COMMANDS_H
