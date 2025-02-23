#ifndef XBDM_GDB_BRIDGE_MACRO_COMMANDS_H
#define XBDM_GDB_BRIDGE_MACRO_COMMANDS_H

#include "shell/command.h"

struct MacroCommandSyncFile : Command {
  MacroCommandSyncFile()
      : Command("Upload a new XBE file to the target if needed.",
                "<local_path> <remote_path>\n"
                "\n"
                "Checks the file modification time of `remote_path` and "
                "uploads `local_path` if it is newer.\n") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct MacroCommandSyncDirectory : Command {
  MacroCommandSyncDirectory()
      : Command("Upload new files to the target if needed.",
                "<local_directory> <remote_directory> [-d]\n"
                "\n"
                "Checks the file modification time of each file in "
                "`remote_directory` and uploads the same file\n"
                " from `local_directory` if it is newer.\n"
                "Files that only exist in `remote_directory` will be left "
                "alone unless the `-d` flag is given.\n") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

#endif  // XBDM_GDB_BRIDGE_MACRO_COMMANDS_H
