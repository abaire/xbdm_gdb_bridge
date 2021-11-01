#ifndef XBDM_GDB_BRIDGE_SHELL_COMMANDS_H
#define XBDM_GDB_BRIDGE_SHELL_COMMANDS_H

#include "shell/command.h"

struct ShellCommandQuit : Command {
  ShellCommandQuit() : Command("Terminate the connection and exit.") {}
  virtual Result operator()(XBOXInterface &interface, const std::vector<std::string> &) { return Result::EXIT_REQUESTED; }
};

#endif  // XBDM_GDB_BRIDGE_SHELL_COMMANDS_H
