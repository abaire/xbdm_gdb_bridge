#ifndef XBDM_GDB_BRIDGE_SHELL_COMMANDS_H
#define XBDM_GDB_BRIDGE_SHELL_COMMANDS_H

#include <iostream>

#include "shell/command.h"

struct ShellCommandQuit : Command {
  ShellCommandQuit() : Command("Terminate the connection and exit.") {}
  Result operator()(XBOXInterface& interface,
                    const std::vector<std::string>&) override {
    return Result::EXIT_REQUESTED;
  }
};

struct ShellCommandReconnect : Command {
  ShellCommandReconnect()
      : Command("Attempt to disconnect and reconnect from XBDM.") {}
  Result operator()(XBOXInterface& interface,
                    const std::vector<std::string>&) override;
};

struct ShellCommandGDB : Command {
  ShellCommandGDB()
      : Command(
            "Start GDB <-> XBDM service.",
            "[IP]:port [xbe_launch_path]\n"
            "\n"
            "Start a GDB server, allowing GDB to communicate with the XBDM "
            "target.\n"
            "\n"
            "[IP]:port - The IP and port at which GDB can connect.\n"
            "              The IP is optional, where the default behavior is "
            "bind to all local interfaces.\n"
            "[xbe_launch_path] - An XBOX path to an XBE (or directory "
            "containing a default.xbe) that should be launched "
            "when a GDB debugger first connects.") {}
  Result operator()(XBOXInterface& interface,
                    const std::vector<std::string>& args) override;
};

struct ShellCommandTrace : Command {
  ShellCommandTrace()
      : Command("Inject the nv2a tracer and capture one or more frames.",
                "[<config> <value>] ...\n"
                "\n"
                "Configuration options:\n"
                "  path <path> - Local directory into which trace artifacts "
                "should be saved. Each frame will create a separate subdir of "
                "the form 'frame_X'. Default: <current working dir>.\n"
                "  frames <int> - Number of consecutive frames to capture. "
                "Default: 1.\n"
                "  tex <on|off> - Enables or disables capture of raw "
                "textures. Default: on.\n"
                "  depth <on|off> - Enables or disables capture of the depth "
                "buffer. Default: off.\n"
                "  color <on|off> - Enables or disables capture of the color "
                "buffer (framebuffer). Default: on.\n"
                "  rdi <on|off> - Enables or disables capture of RDI "
                "regions (vertex shader program, constants). This may have a "
                "significant performance impact. Default: off.\n"
                "  pgraph <on|off> - Enables or disables capture of the raw "
                "PGRAPH region. Default: off.\n"
                "  pfb <on|off> - Enables or disables capture of the raw "
                "PFB region. Default: off.") {}

  Result operator()(XBOXInterface& interface,
                    const std::vector<std::string>& args) override;
};

#endif  // XBDM_GDB_BRIDGE_SHELL_COMMANDS_H
