#ifndef XBDM_GDB_BRIDGE_TRACER_COMMANDS_H
#define XBDM_GDB_BRIDGE_TRACER_COMMANDS_H

#include <iostream>

#include "shell/command.h"

class XBOXInterface;

struct TracerCommandInit : Command {
  TracerCommandInit()
      : Command("Load the ntrc nv2a tracer DynamicDXT.",
                "\n"
                "Load the ntrc tracer.\n"
                "\n"
                "This will avoid an on-demand load when interacting with the "
                "tracer via other commands, enabling more accurate start "
                "timing.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
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
                "[local_artifact_path] [num_frames]\n"
                "\n"
                "Retrieves PGRAPH and graphics tracing from the XBOX.\n"
                "\n"
                "[local_artifact_path] - Local filesystem path into which "
                "artifacts should be written.\n"
                "    If multiple frames are captured, a subdirectory will be "
                "created for each frame. Defaults to the the current working "
                "directory.\n"
                "[num_frames] - The number of consecutive frames to capture. "
                "Defaults to 1.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

#endif  // XBDM_GDB_BRIDGE_TRACER_COMMANDS_H
