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

struct HandlerCommandInvokeSimple : Command {
  HandlerCommandInvokeSimple()
      : Command(
            "<processor>!<command> [args]\n"
            "\n"
            "Invokes an arbitrary debug command processor with the given "
            "arguments.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct HandlerCommandLoad : Command {
  HandlerCommandLoad()
      : Command(
            "<dll_path>\n"
            "\n"
            "Load the given DXT DLL.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

#endif  // XBDM_GDB_BRIDGE_HANDLER_COMMANDS_H
