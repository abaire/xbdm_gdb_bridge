#ifndef XBDM_GDB_BRIDGE_TRACER_COMMANDS_H
#define XBDM_GDB_BRIDGE_TRACER_COMMANDS_H

#include <iostream>

#include "shell/command.h"

class XBOXInterface;

struct TracerCommandInit : Command {
  TracerCommandInit()
      : Command("Load the ntrc nv2a tracer DynamicDXT.",
                "[<config> <value>] ...\n"
                "\n"
                "Initializes the NTRC tracer on the remote.\n"
                "\n"
                "Configuration options:\n"
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
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct TracerCommandDetach : Command {
  TracerCommandDetach()
      : Command("Detaches from the ntrc nv2a tracer DynamicDXT.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct TracerCommandBreakOnNextFlip : Command {
  TracerCommandBreakOnNextFlip()
      : Command(
            "Asks the tracer to break at the start of a frame.",
            "[require_flip]\n"
            "\n"
            "Asks the tracer to break at the start of a frame.\n"
            "\n"
            "[require_flip] - Forces discard until the next frame, even if the "
            "tracer is already at the start of a frame.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct TracerCommandTraceFrames : Command {
  TracerCommandTraceFrames()
      : Command("Retrieves hardware interaction trace for one or more frames.",
                "[<config> <value>] ...\n"
                "\n"
                "Retrieves PGRAPH and graphics tracing from the XBOX.\n"
                "\n"
                "Configuration options:\n"
                "  path <path> - Local directory into which trace artifacts "
                "should be saved. Each frame will create a separate subdir of "
                "the form 'frame_X'. Default: <current working dir>.\n"
                "  frames <int> - Number of consecutive frames to capture. "
                "Default: 1.\n"
                "  verbose - Emits more verbose information into the capture "
                "log.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

#endif  // XBDM_GDB_BRIDGE_TRACER_COMMANDS_H
