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
      : Command(
            "\n"
            "Verifies that the XBDM handler injector is available.") {}
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

struct HandlerCommandInvokeMultiline : Command {
  HandlerCommandInvokeMultiline()
      : Command(
            "<processor>!<command> [args]\n"
            "\n"
            "Invokes an arbitrary debug command processor with the given "
            "arguments, expecting a multiline response.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct HandlerCommandInvokeSendBinary : Command {
  HandlerCommandInvokeSendBinary()
      : Command(
            "<processor>!<command> <binary_path> [args]\n"
            "\n"
            "Invokes an arbitrary debug command processor with the given "
            "arguments, sending the contents of `binary_path` as a binary "
            "attachment.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct HandlerCommandInvokeReceiveSizePrefixedBinary : Command {
  HandlerCommandInvokeReceiveSizePrefixedBinary()
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

struct HandlerCommandInvokeReceiveKnownSizedBinary : Command {
  HandlerCommandInvokeReceiveKnownSizedBinary()
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
