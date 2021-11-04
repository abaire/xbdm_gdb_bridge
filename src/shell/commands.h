#ifndef XBDM_GDB_BRIDGE_COMMANDS_H
#define XBDM_GDB_BRIDGE_COMMANDS_H

#include <iostream>

#include "rdcp/xbdm_requests.h"
#include "shell/command.h"

struct CommandAltAddr : Command {
  CommandAltAddr() : Command("Prints 'Game Configuration' IP information.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    auto request = std::make_shared<AltAddr>();
    interface.SendCommandSync(request);
    if (request->IsOK()) {
      std::cout << "Alternate IP address: " << request->address_string
                << std::endl;
    }

    return Result::HANDLED;
  }
};

struct CommandBreak : Command {
  CommandBreak()
      : Command(
            "<subcommand> [subcommand_args]\n"
            "\n"
            "Adds or removes a breakpoint.\n"
            "\n"
            "subcommands:\n"
            "  clearall - Clears all breakpoints\n"
            "  start - Sets a breakpoint at program entry. Only valid if "
            "the remote is in state \"execution pending\".\n"
            "  [-]addr <address> - Breaks on execution at the given "
            "address.\n"
            "  [-]read <address> <length> - Breaks on read access to the "
            "given memory range.\n"
            "  [-]write <address> <length> - Breaks on write access to the "
            "given memory range.\n"
            "  [-]execute <address> <length> - Breaks on execution within "
            "the given memory range.\n"
            "\n"
            "Subcommands with [-] can be prefixed with '-' to disable a "
            "previously set breakpoint.\n"
            "    E.g., addr 0x12345   # sets a breakpoint at 0x12345\n"
            "          -addr 0x12345  # clears the breakpoint") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandBye : Command {
  CommandBye() : Command("Closes the connection gracefully.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override;
};

struct CommandContinue : Command {
  CommandContinue()
      : Command(
            "<thread_id> [break_on_exception=false]\n"
            "\n"
            "Continues execution of the given thread.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandDebugOptions : Command {
  CommandDebugOptions()
      : Command(
            "[crashdump | dpctrace] [...]\n"
            "\n"
            "If no arguments are given, retrieves the currently active debug "
            "options.\n"
            "If at least one argument is given, enables that debug option and "
            "disables any options that are not given.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandDebugger : Command {
  CommandDebugger()
      : Command(
            "[disable]\n"
            "\n"
            "If no args are given, notifies XBDM that this channel intends to "
            "act as a debugger.\n"
            "If 'disable' is given, disables the previously set debugger "
            "flag.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandDebugMode : Command {
  CommandDebugMode()
      : Command(
            "[disable]\n"
            "\n"
            "If no args are given, notifies XBDM that this channel intends to "
            "act as a debugger.\n"
            "If 'disable' is given, disables the previously set debugger "
            "flag.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandDelete : Command {
  CommandDelete()
      : Command(
            "<path> [-r]\n"
            "\n"
            "Deletes the given path.\n"
            "If -r is given, deletes a the given directory recursively.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandDirList : Command {
  CommandDirList()
      : Command(
            "<path>\n"
            "\n"
            "Returns a list of items in `path`.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandDebugMonitorVersion : Command {
  CommandDebugMonitorVersion()
      : Command("Returns the debug monitor version.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandDriveFreeSpace : Command {
  CommandDriveFreeSpace()
      : Command(
            "[drive_letter]\n"
            "\n"
            "Returns the amount of free space on a drive.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandDriveList : Command {
  CommandDriveList() : Command("Lists mounted drives.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandGetChecksum : Command {
  CommandGetChecksum()
      : Command(
            "<address> <length> <blocksize>\n"
            "\n"
            "Calculates a checksum for the given memory region.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandGetContext : Command {
  CommandGetContext()
      : Command(
            "<thread_id> [<mode> [...]]\n"
            "\n"
            "Retrieves information about the registers for `thread_id`\n"
            "\n"
            "thread_id - The thread to retrieve information about\n"
            "mode - [control | int | fp | full]\n"
            " Optional list of one or more registry sets to query. If no "
            "`mode` is passed,\n"
            " all types are queried.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandGetExtContext : Command {
  CommandGetExtContext()
      : Command(
            "<thread_id>\n"
            "\n"
            "Retrieves information about the floating point registers for "
            "`thread_id`") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandGetFileAttributes : Command {
  CommandGetFileAttributes()
      : Command(
            "<path>\n"
            "\n"
            "Retrieves information about the file at `path`.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandGetMem : Command {
  CommandGetMem()
      : Command(
            "<address> <size>\n"
            "\n"
            "Fetches the context of the given block of memory.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandGetProcessID : Command {
  CommandGetProcessID()
      : Command(
            "Retrieves the ID of the currently running process (must be "
            "debuggable).") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandGetUtilityDriveInfo : Command {
  CommandGetUtilityDriveInfo()
      : Command("Gets information about the mounted utility partitions.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandGo : Command {
  CommandGo() : Command("Resumes execution of all threads.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandHalt : Command {
  CommandHalt()
      : Command(
            "[thread_id]\n"
            "\n"
            "Halts the given thread, or all threads if no `thread_id` is "
            "given.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandIsBreak : Command {
  CommandIsBreak()
      : Command(
            "<address>\n"
            "\n"
            "Checks to see if a breakpoint is set at `address`.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandIsDebugger : Command {
  CommandIsDebugger() : Command("Checks to see if the debugger is attached.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandIsStopped : Command {
  CommandIsStopped()
      : Command(
            "<thread_id>\n"
            "\n"
            "Checks to see if the given thread is stopped.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandMagicBoot : Command {
  CommandMagicBoot()
      : Command(
            "<path> [noxbdm] [cold]\n"
            "\n"
            "Boots the given XBE.\n"
            "noxbdm - Disables XBDM (no further commands will be accepted "
            "until the target is rebooted)\n"
            "cold - Performs a cold-reboot.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandMemoryMap : Command {
  CommandMemoryMap() : Command("Returns global memory information.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandMakeDirectory : Command {
  CommandMakeDirectory()
      : Command(
            "<path>\n"
            "\n"
            "Creates a new directory at the given path.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandModuleSections : Command {
  CommandModuleSections()
      : Command(
            "<path>\n"
            "\n"
            "Returns information about the sections in the given executable "
            "Module.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandModules : Command {
  CommandModules()
      : Command("Returns the currently running executable modules.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandNoStopOn : Command {
  CommandNoStopOn()
      : Command(
            "<flag> [<flag> [...]]\n"
            "\n"
            "Disables automatic stop on events.\n"
            "flag:\n"
            "  all          - All events\n"
            "  fce          - First chance exceptions\n"
            "  debugstr     - debugstr() invocations\n"
            "  createthread - Thread creation\n"
            "  stacktrace   - Stack traces") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandNotifyAt : Command {
  CommandNotifyAt()
      : Command(
            "<port> [drop] [debug]\n"
            "\n"
            "Instructs the target to open a notification connection on the "
            "given port.\n"
            "drop - Instructs the target to close an existing connection "
            "instead of connecting.\n"
            "debug - Sets the notification channel to debug mode.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandRename : Command {
  CommandRename()
      : Command(
            "<path> <new_path>\n"
            "\n"
            "Moves the file at `path` to `new_path`.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandReboot : Command {
  CommandReboot()
      : Command(
            "[<flag> [...]]\n"
            "\n"
            "Reboots the target machine.\n"
            "\n"
            "flags:\n"
            "  wait - Wait for the debugger to attach on restart.\n"
            "  stop - Stop at entry into the launch XBE.\n"
            "  nodebug - Do not start XBDM when rebooting.\n"
            "  warm - Do a warm reboot.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandResume : Command {
  CommandResume()
      : Command(
            "<thread_id>\n"
            "\n"
            "Resumes the given thread.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandSetMem : Command {
  CommandSetMem()
      : Command(
            "<address> <hex_string>\n"
            "\n"
            "Sets the content of memory starting at `address` to the given "
            "`hex_string`.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandStop : Command {
  CommandStop() : Command("Breaks the target into the debugger.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandStopOn : Command {
  CommandStopOn()
      : Command(
            "<flag> [<flag> [...]]\n"
            "\n"
            "Enables automatic stop on events.\n"
            "flag:\n"
            "  all          - All events\n"
            "  fce          - First chance exceptions\n"
            "  debugstr     - debugstr() invocations\n"
            "  createthread - Thread creation\n"
            "  stacktrace   - Stack traces") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandSuspend : Command {
  CommandSuspend()
      : Command(
            "<thread_id>\n"
            "\n"
            "Suspends (or increments the suspend count of) the given thread.") {
  }
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandThreadInfo : Command {
  CommandThreadInfo()
      : Command(
            "<thread_id>\n"
            "\n"
            "Retrieves info about the specified thread.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandThreads : Command {
  CommandThreads() : Command("Lists running threads.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandWalkMem : Command {
  CommandWalkMem() : Command("Prints info on mapped memory.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandXBEInfo : Command {
  CommandXBEInfo()
      : Command(
            "[path [disk_only]]\n"
            "\n"
            "Prints info on XBEs.\n"
            "If no path is given, gets info on the currently running XBE.\n"
            "If a path is given, gets info on that XBE.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

struct CommandXTLInfo : Command {
  CommandXTLInfo() : Command("Prints last error info") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override;
};

#endif  // XBDM_GDB_BRIDGE_COMMANDS_H
