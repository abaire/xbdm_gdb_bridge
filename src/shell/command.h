#ifndef XBDM_GDB_BRIDGE_COMMAND_H
#define XBDM_GDB_BRIDGE_COMMAND_H

#include <iostream>
#include <string>
#include <vector>

#include "xbox/xbox_interface.h"

struct Command {
  virtual ~Command() = default;
  enum Result {
    UNHANDLED,
    HANDLED,
    EXIT_REQUESTED,
  };

  explicit Command(std::string short_help, std::string long_help = "")
      : short_help(std::move(short_help)) {
    if (!long_help.empty()) {
      help = std::move(long_help);
    } else {
      help = "\n" + this->short_help;
    }
  }

  virtual Result operator()(XBOXInterface &interface,
                            const std::vector<std::string> &args) = 0;

  void PrintUsage() const { std::cout << help << std::endl; }

  std::string short_help;
  std::string help;
};

#endif  // XBDM_GDB_BRIDGE_COMMAND_H
