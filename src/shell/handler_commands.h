#ifndef XBDM_GDB_BRIDGE_HANDLER_COMMANDS_H
#define XBDM_GDB_BRIDGE_HANDLER_COMMANDS_H

#include <iostream>

#include "shell/command.h"

class XBOXInterface;

struct HandlerCommandLoadBootstrap : Command {
  HandlerCommandLoadBootstrap()
      : Command(
            "\n"
            "Load the XBDM handler injector.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct HandlerCommandHello : Command {
  HandlerCommandHello()
      : Command("Verifies that the handler loader is running properly.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

#endif  // XBDM_GDB_BRIDGE_HANDLER_COMMANDS_H
