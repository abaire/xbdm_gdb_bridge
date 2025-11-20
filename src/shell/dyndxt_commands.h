#ifndef XBDM_GDB_BRIDGE_HANDLER_COMMANDS_H
#define XBDM_GDB_BRIDGE_HANDLER_COMMANDS_H

#include <iostream>

#include "shell/command.h"

class XBOXInterface;

struct DynDXTCommandLoadBootstrap : Command {
  DynDXTCommandLoadBootstrap() : Command("Load the XBDM handler injector.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DynDXTCommandHello : Command {
  DynDXTCommandHello()
      : Command("Verify that the XBDM handler injector is available.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct DynDXTCommandInvokeSimple : Command {
  DynDXTCommandInvokeSimple()
      : Command(
            "Invoke a debug command processor, expecting a single response.",
            "<processor>!<command> [args]\n"
            "\n"
            "Invokes an arbitrary debug command processor with the given "
            "arguments.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct DynDXTCommandInvokeMultiline : Command {
  DynDXTCommandInvokeMultiline()
      : Command(
            "Invoke a debug command processor, expecting a multiline response.",
            "<processor>!<command> [args]\n"
            "\n"
            "Invoke an arbitrary debug command processor with the given "
            "arguments, expecting a multiline response.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct DynDXTCommandInvokeSendBinary : Command {
  DynDXTCommandInvokeSendBinary()
      : Command("Send a binary to a debug command processor.",
                "<processor>!<command> <binary_path> [args]\n"
                "\n"
                "Invoke an arbitrary debug command processor with the given "
                "arguments, sending the contents of `binary_path` as a binary "
                "attachment.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct DynDXTCommandInvokeReceiveSizePrefixedBinary : Command {
  DynDXTCommandInvokeReceiveSizePrefixedBinary()
      : Command(
            "Receive a size-prefixed binary from a debug command processor.",
            "<processor>!<command> <save_path> [args]\n"
            "\n"
            "Invoke an arbitrary debug command processor with the given "
            "arguments, expecting a binary response which is prefixed with a "
            "4-byte length, which will be saved into a file at the given "
            "path.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct DynDXTCommandInvokeReceiveKnownSizedBinary : Command {
  DynDXTCommandInvokeReceiveKnownSizedBinary()
      : Command(
            "Receive a binary with the provided size from a debug command "
            "processor.",
            "<processor>!<command> <save_path> <size_in_bytes> [args]\n"
            "\n"
            "Invoke an arbitrary debug command processor with the given "
            "arguments, expecting a binary response that is `size_in_bytes` "
            "bytes in length, which will be saved into a file at the given "
            "path.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct DynDXTCommandLoad : Command {
  DynDXTCommandLoad()
      : Command("Load a DynamicDXT library onto the remote host.",
                "<dll_path>\n"
                "\n"
                "Load the given DXT DLL.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

#endif  // XBDM_GDB_BRIDGE_HANDLER_COMMANDS_H
