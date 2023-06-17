#ifndef XBDM_GDB_BRIDGE_HANDLER_COMMANDS_H
#define XBDM_GDB_BRIDGE_HANDLER_COMMANDS_H

#include <iostream>

#include "shell/command.h"

class XBOXInterface;

struct DynDXTCommandLoadBootstrap : Command {
  DynDXTCommandLoadBootstrap()
      : Command(
            "\n"
            "Load the XBDM handler injector.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct DynDXTCommandHello : Command {
  DynDXTCommandHello()
      : Command(
            "\n"
            "Verifies that the XBDM handler injector is available.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct DynDXTCommandInvokeSimple : Command {
  DynDXTCommandInvokeSimple()
      : Command(
            "<processor>!<command> [args]\n"
            "\n"
            "Invokes an arbitrary debug command processor with the given "
            "arguments.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct DynDXTCommandInvokeMultiline : Command {
  DynDXTCommandInvokeMultiline()
      : Command(
            "<processor>!<command> [args]\n"
            "\n"
            "Invokes an arbitrary debug command processor with the given "
            "arguments, expecting a multiline response.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct DynDXTCommandInvokeSendBinary : Command {
  DynDXTCommandInvokeSendBinary()
      : Command(
            "<processor>!<command> <binary_path> [args]\n"
            "\n"
            "Invokes an arbitrary debug command processor with the given "
            "arguments, sending the contents of `binary_path` as a binary "
            "attachment.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct DynDXTCommandInvokeReceiveSizePrefixedBinary : Command {
  DynDXTCommandInvokeReceiveSizePrefixedBinary()
      : Command(
            "<processor>!<command> <save_path> [args]\n"
            "\n"
            "Invokes an arbitrary debug command processor with the given "
            "arguments, expecting a binary response which is prefixed with a "
            "4-byte length, which will be saved into a file at the given "
            "path.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct DynDXTCommandInvokeReceiveKnownSizedBinary : Command {
  DynDXTCommandInvokeReceiveKnownSizedBinary()
      : Command(
            "<processor>!<command> <save_path> <size_in_bytes> [args]\n"
            "\n"
            "Invokes an arbitrary debug command processor with the given "
            "arguments, expecting a binary response that is `size_in_bytes` "
            "bytes in length, which will be saved into a file at the given "
            "path.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct DynDXTCommandLoad : Command {
  DynDXTCommandLoad()
      : Command(
            "<dll_path>\n"
            "\n"
            "Load the given DXT DLL.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

#endif  // XBDM_GDB_BRIDGE_HANDLER_COMMANDS_H
