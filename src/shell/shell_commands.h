#ifndef XBDM_GDB_BRIDGE_SHELL_COMMANDS_H
#define XBDM_GDB_BRIDGE_SHELL_COMMANDS_H

#include <iostream>

#include "shell/command.h"

struct ShellCommandQuit : Command {
  ShellCommandQuit() : Command("Terminate the connection and exit.") {}
  virtual Result operator()(XBOXInterface &interface, const std::vector<std::string> &) { return Result::EXIT_REQUESTED; }
};

struct ShellCommandReconnect : Command {
  ShellCommandReconnect() : Command("Attempt to disconnect and reconnect from XBDM.") {}
  virtual Result operator()(XBOXInterface &interface, const std::vector<std::string> &) {
    if (interface.ReconnectXBDM()) {
      std::cout << "Connected." << std::endl;
    } else {
      std::cout << "Failed to connect." << std::endl;
    }
    return Result::HANDLED;
  }
};

#endif  // XBDM_GDB_BRIDGE_SHELL_COMMANDS_H
