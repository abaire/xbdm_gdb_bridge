#ifndef XBDM_GDB_BRIDGE_COMMANDS_H
#define XBDM_GDB_BRIDGE_COMMANDS_H

#include <iostream>

#include "rdcp/xbdm_requests.h"
#include "shell/command.h"

struct CommandAltAddr : Command {
  CommandAltAddr() : Command("Print 'Game Configuration' IP information.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override {
    auto request = std::make_shared<AltAddr>();
    interface.SendCommandSync(request);
    if (request->IsOK()) {
      out << "Alternate IP Address: " << request->address_string << std::endl;
    }

    return Result::HANDLED;
  }
};

struct CommandBreak : Command {
  CommandBreak()
      : Command("Manage breakpoints.",
                "<subcommand> [subcommand_args]\n"
                "\n"
                "Add or removes a breakpoint.\n"
                "\n"
                "subcommands:\n"
                "  clearall - Clears all breakpoints\n"
                "  start - Sets a breakpoint at program entry. Only valid if "
                "the target is in state \"execution pending\".\n"
                "  [-]addr <Address> - Breaks on execution at the given "
                "Address.\n"
                "  [-]read <Address> <length> - Breaks on read access to the "
                "given memory range.\n"
                "  [-]write <Address> <length> - Breaks on write access to the "
                "given memory range.\n"
                "  [-]execute <Address> <length> - Breaks on execution within "
                "the given memory range.\n"
                "\n"
                "Subcommands with [-] can be prefixed with '-' to disable a "
                "previously set breakpoint.\n"
                "    E.g., addr 0x12345   # sets a breakpoint at 0x12345\n"
                "          -addr 0x12345  # clears the breakpoint\n"
                "\n"
                "addr/read/write/execute breakpoints may be conditionally "
                "ignored by providing a trailing IF\n"
                "condition. The condition may reference basic registers using "
                "$reg syntax and perform simple\n"
                "C-style comparisons.\n"
                "    E.g., addr 0x12345 IF $eax == 0xFF00DD\n"
                "In addition, the broken thread ID may be checked using 'tid'\n"
                "    E.g., addr 0x12345 IF tid == 28") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandBye : Command {
  CommandBye() : Command("Close the debugger connection gracefully.") {}
  Result operator()(XBOXInterface& interface, const ArgParser&,
                    std::ostream& out) override;
};

struct CommandContinue : Command {
  CommandContinue()
      : Command(
            "Continue execution of a thread.",
            "<thread_id> [break_on_exception=false]\n"
            "\n"
            "Continue execution of the thread with the given `thread_id`.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandDebugOptions : Command {
  CommandDebugOptions()
      : Command(
            "List or modify debug options.",
            "[crashdump | dpctrace] [...]\n"
            "\n"
            "If no arguments are given, retrieves the currently active debug "
            "options.\n"
            "If at least one argument is given, enables that debug option and "
            "disables any options that are not given.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandDebugger : Command {
  CommandDebugger()
      : Command(
            "Enable or disable debugging.",
            "[disable]\n"
            "\n"
            "If no args are given, notifies XBDM that this channel intends to "
            "act as a debugger.\n"
            "If 'disable' is given, disables the previously set debugger "
            "flag.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

// struct CommandDebugMode : Command {
//   CommandDebugMode()
//       : Command(TODO) {}
//   Result operator()(XBOXInterface &interface,
//                     const std::vector<std::string> &args, std::ostream& out)
//                     override;
// };

struct CommandDedicate : Command {
  CommandDedicate()
      : Command(
            "Dedicate the shell connection to a specific handler or reset to "
            "global.",
            "[handler_name]\n"
            "\n"
            "If no args are given, restores the shell connection to the global "
            "command handler. If a name is given, dedicates the shell "
            "connection to talking to only that handler, which must have been "
            "registered via DmCommandProcessorEx with a non-null thread "
            "creation argument.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandDelete : Command {
  CommandDelete()
      : Command("Delete a file or directory.",
                "<path> [-r]\n"
                "\n"
                "Delete the given `path`.\n"
                "If -r is given, deletes a the given directory recursively.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandDirList : Command {
  CommandDirList()
      : Command("List details of a file or directory.",
                "<path>\n"
                "\n"
                "Retrieve a list of items in `path`.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandDebugMonitorVersion : Command {
  CommandDebugMonitorVersion() : Command("Print the debug monitor version.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandDriveFreeSpace : Command {
  CommandDriveFreeSpace()
      : Command(
            "Print free space on a drive.",
            "[drive_letter]\n"
            "\n"
            "Print the amount of free space on `drive_letter` (e.g., 'E').") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandDriveList : Command {
  CommandDriveList() : Command("List mounted drives.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandGetChecksum : Command {
  CommandGetChecksum()
      : Command("Print a checksum for a memory region.",
                "<Address> <length> <blocksize>\n"
                "\n"
                "Calculate a checksum for the memory region starting at "
                "'address'.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandGetContext : Command {
  CommandGetContext()
      : Command("Retrieve register information for a thread.",
                "<thread_id> [<mode> [...]]\n"
                "\n"
                "Retrieve information about the registers for `thread_id`\n"
                "\n"
                "thread_id - The thread to retrieve information about\n"
                "mode - [control | int | fp | full]\n"
                " Optional list of one or more registry sets to query. If no "
                "`mode` is passed,\n"
                " all types are queried.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandGetExtContext : Command {
  CommandGetExtContext()
      : Command("Retrieve floating point register information for a thread.",
                "<thread_id>\n"
                "\n"
                "Retrieve information about the floating point registers for "
                "`thread_id`") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandGetFile : Command {
  CommandGetFile()
      : Command(
            "Copy a file from the device.",
            "<path> [local_path]\n"
            "\n"
            "Retrieve the file at `path`.\n"
            "If `path` is a directory, retrieves the contents recursively.\n"
            "If `local_path` is given, writes the file into `local_path`. If "
            "`local_path` is a directory, the file will retain\n"
            "the original filename, if it does not exist, the file will saved "
            "as the given name.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandGetFileAttributes : Command {
  CommandGetFileAttributes()
      : Command("Print detailed file attributes for a remote path.",
                "<path>\n"
                "\n"
                "Retrieve information about the file at `path`.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandGetMem : Command {
  CommandGetMem()
      : Command("Fetch raw memory content.",
                "<Address> <size> [byte | word | dword | binary <filename>]\n"
                "\n"
                "Fetch the content of the given block of memory.\n"
                "Mathematical arguments in parentheses will be resolved to "
                "integers.\n"
                "An optional layout param may be given to adjust how the data"
                "is displayed:\n"
                "  byte - [default] render each byte in hex\n"
                "  word - render each pair of bytes as little endian words\n"
                "  dword - render every four bytes as little endian dwords\n"
                "  binary <filename> - save raw data to the given file") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandGetProcessID : Command {
  CommandGetProcessID()
      : Command("Print the currently running process ID.",
                "Print the ID of the currently running process (which must be "
                "debuggable).") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandGetUtilityDriveInfo : Command {
  CommandGetUtilityDriveInfo()
      : Command("Print information about the mounted utility partitions.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandGo : Command {
  CommandGo() : Command("Resume execution of all threads.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandHalt : Command {
  CommandHalt()
      : Command("Halt one or more threads.",
                "[thread_id]\n"
                "\n"
                "Halt the given thread, or all threads if no `thread_id` is "
                "given.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandIsBreak : Command {
  CommandIsBreak()
      : Command("Check for the existence of a breakpoint.",
                "<Address>\n"
                "\n"
                "Check to see if a breakpoint is set at `Address`.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandIsDebugger : Command {
  CommandIsDebugger() : Command("Check to see if the debugger is attached.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandIsStopped : Command {
  CommandIsStopped()
      : Command("Check to see if a thread is stopped.",
                "<thread_id>\n"
                "\n"
                "Check to see if the thread `thread_id` is stopped.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandMagicBoot : Command {
  CommandMagicBoot()
      : Command("Reboot into an XBE.",
                "<path> [noxbdm] [cold]\n"
                "\n"
                "Boots the XBE at 'path'.\n"
                "\n"
                "noxbdm - Disables XBDM (no further commands will be accepted "
                "until the target is rebooted).\n"
                "cold - Performs a cold-reboot.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandMemoryMap : Command {
  CommandMemoryMap() : Command("Return global memory information.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandMakeDirectory : Command {
  CommandMakeDirectory()
      : Command("Create a new directory on the target.",
                "<path>\n"
                "\n"
                "Create a new directory at the given path.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandModuleSections : Command {
  CommandModuleSections()
      : Command("Print information about a module on the target.",
                "<name>\n"
                "\n"
                "Return information about the sections in the given executable "
                "Module.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandModuleLongName : Command {
  CommandModuleLongName()
      : Command("Print path information about a module on the target.",
                "<name>\n"
                "\n"
                "Return path information about the given executable Module.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandModules : Command {
  CommandModules()
      : Command("Print the currently running executable modules.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandNoStopOn : Command {
  CommandNoStopOn()
      : Command("Disable automatic breakpoints on certain events.",
                "<flag> [<flag> [...]]\n"
                "\n"
                "Disable automatic stop on the given 'flag' events.\n"
                "\n"
                "flag:\n"
                "  all          - All events\n"
                "  fce          - First chance exceptions\n"
                "  debugstr     - debugstr() invocations\n"
                "  createthread - Thread creation\n"
                "  stacktrace   - Stack traces") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandNotifyAt : Command {
  CommandNotifyAt()
      : Command("Manage a debugger notification channel.",
                "<Port> [drop] [debug]\n"
                "\n"
                "Instruct the target to open a notification connection on the "
                "given Port.\n"
                "\n"
                "drop - Instructs the target to close an existing connection "
                "instead of connecting.\n"
                "debug - Sets the notification channel to debug mode.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandPutFile : Command {
  CommandPutFile()
      : Command(
            "Upload a file to the target.",
            "<local_path> <remote_path> [-f]\n"
            "\n"
            "Upload the file at `local_path` to `remote_path`.\n"
            "\n"
            "If `local_path` is a directory, sends the contents recursively.\n"
            "If the `-f` flag is given, overwrites remote files.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandRename : Command {
  CommandRename()
      : Command("Rename/move a path on the target.",
                "<path> <new_path>\n"
                "\n"
                "Move the file at `path` to `new_path`.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandReboot : Command {
  CommandReboot()
      : Command("Reboot the target.",
                "[<flag> [...]]\n"
                "\n"
                "Reboot the target machine.\n"
                "\n"
                "flags:\n"
                "  wait - Wait for the debugger to attach on restart.\n"
                "  stop - Stop at entry into the launch XBE.\n"
                "  nodebug - Do not start XBDM when rebooting.\n"
                "  warm - Do a warm reboot.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandResume : Command {
  CommandResume()
      : Command("Resume a thread.",
                "<thread_id>\n"
                "\n"
                "Resume thread 'thread_id'.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandScreenshot : Command {
  CommandScreenshot() : Command("Capture a screenshot") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandSetMem : Command {
  CommandSetMem()
      : Command("Set the content of memory on the target.",
                "<address> <hex_string>\n"
                "\n"
                "Set the content of memory starting at `address` to the given "
                "`hex_string`.\n"
                "The hex string is a series of hexadecimal digits without any "
                "prefix. E.g., 9090 would be 0x90 0x90.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandStop : Command {
  CommandStop() : Command("Break into the debugger.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandStopOn : Command {
  CommandStopOn()
      : Command("Enable a debugger breakpoints on events.",
                "<flag> [<flag> [...]]\n"
                "\n"
                "Enables automatic stop on event 'flag'.\n"
                "\n"
                "flag:\n"
                "  all          - All events\n"
                "  fce          - First chance exceptions\n"
                "  debugstr     - debugstr() invocations\n"
                "  createthread - Thread creation\n"
                "  stacktrace   - Stack traces") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandSuspend : Command {
  CommandSuspend()
      : Command("Suspend a thread",
                "<thread_id>\n"
                "\n"
                "Suspend (or increments the suspend count of) the given thread "
                "'thread_id'.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandThreadInfo : Command {
  CommandThreadInfo()
      : Command(
            "Print detailed information about a thread.",
            "<thread_id>\n"
            "\n"
            "Retrieve detailed info about the specified thread 'thread_id'.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandThreads : Command {
  CommandThreads() : Command("List running threads.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandWalkMem : Command {
  CommandWalkMem() : Command("Print info on mapped memory.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandXBEInfo : Command {
  CommandXBEInfo()
      : Command("Print detailed information about an XBE.",
                "[path [disk_only]]\n"
                "\n"
                "Print info on an XBE.\n"
                "\n"
                "If no path is given, gets info on the currently running XBE.\n"
                "If a path is given, gets info on the XBE at 'path'.") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

struct CommandXTLInfo : Command {
  CommandXTLInfo() : Command("Print last error info") {}
  Result operator()(XBOXInterface& interface, const ArgParser& args,
                    std::ostream& out) override;
};

#endif  // XBDM_GDB_BRIDGE_COMMANDS_H
