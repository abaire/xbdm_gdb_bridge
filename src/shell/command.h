#ifndef XBDM_GDB_BRIDGE_COMMAND_H
#define XBDM_GDB_BRIDGE_COMMAND_H

#include <string>
#include <vector>

#include "xbox/xbox_interface.h"

struct Command {
  enum Result {
    UNHANDLED,
    HANDLED,
    EXIT_REQUESTED,
  };

  explicit Command(std::string help) : help(std::move(help)) {}

  virtual Result operator()(XBOXInterface &interface,
                            const std::vector<std::string> &args) = 0;

  std::string name;
  std::string help;
};

#endif  // XBDM_GDB_BRIDGE_COMMAND_H
