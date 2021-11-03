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

#endif  // XBDM_GDB_BRIDGE_COMMANDS_H
