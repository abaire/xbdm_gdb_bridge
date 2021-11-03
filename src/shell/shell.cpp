#include "shell.h"

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <string>

#include "commands.h"
#include "debugger_commands.h"
#include "shell_commands.h"

Shell::Shell(std::shared_ptr<XBOXInterface> &interface)
    : interface_(interface), prompt_("> ") {
  auto quit = std::make_shared<ShellCommandQuit>();
  commands_["exit"] = quit;
  commands_["gdb"] = std::make_shared<ShellCommandGDB>();
  commands_["help"] = nullptr;
  commands_["?"] = nullptr;
  commands_["reconnect"] = std::make_shared<ShellCommandReconnect>();
  commands_["quit"] = quit;

  commands_["/launch"] = std::make_shared<DebuggerCommandLaunch>();
  commands_["/launchwait"] = std::make_shared<DebuggerCommandLaunchWait>();
  commands_["/attach"] = std::make_shared<DebuggerCommandAttach>();
  commands_["/detach"] = std::make_shared<DebuggerCommandDetach>();
  commands_["/restart"] = std::make_shared<DebuggerCommandRestart>();
  commands_["/switch"] = std::make_shared<DebuggerCommandSetActiveThread>();
  commands_["/threads"] = std::make_shared<DebuggerCommandGetThreads>();
  commands_["/info"] = std::make_shared<DebuggerCommandGetThreadInfo>();
  commands_["/stepi"] = std::make_shared<DebuggerCommandStepInstruction>();
  auto step_function = std::make_shared<DebuggerCommandStepFunction>();
  commands_["/stepf"] = step_function;
  commands_["/stepfun"] = step_function;
  commands_["/info"] = std::make_shared<DebuggerCommandGetThreadInfo>();
  commands_["/context"] = std::make_shared<DebuggerCommandGetContext>();
  commands_["/fullcontext"] = std::make_shared<DebuggerCommandGetFullContext>();
  commands_["/haltall"] = std::make_shared<DebuggerCommandHaltAll>();
  commands_["/halt"] = std::make_shared<DebuggerCommandHalt>();
  commands_["/continueall"] = std::make_shared<DebuggerCommandContinueAll>();
  commands_["/continue"] = std::make_shared<DebuggerCommandContinue>();
  commands_["/suspend"] = std::make_shared<DebuggerCommandSuspend>();
  commands_["/resume"] = std::make_shared<DebuggerCommandResume>();

  commands_["altaddr"] = std::make_shared<CommandAltAddr>();
  commands_["break"] = std::make_shared<CommandBreak>();
  /*
   "bye": lambda _: rdcp_command.Bye(handler=print),
    "continue": _continue,
    "debugoptions": _debug_options,
    "debugger": _debugger,
    "debugmode": lambda _: rdcp_command.DebugMode(handler=print),
    "dedicate": _dedicate,
    "rm": lambda rest: rdcp_command.Delete(rest[0], rest[0][-1] == "/",
   handler=print), "ls": _dirlist, "dmversion": lambda _:
   rdcp_command.DMVersion(handler=print), "df": lambda rest:
   rdcp_command.DriveFreeSpace(rest[0][0], handler=print), "drivelist": lambda
   _: rdcp_command.DriveList(handler=print), "funccall": lambda rest:
   rdcp_command.FuncCall(rest[0], handler=print), "getcontext": _get_context,
    "getd3dstate": lambda _: rdcp_command.GetD3DState(handler=print),
    "getextcontext": lambda rest: rdcp_command.GetExtContext(
        int(rest[0], 0), handler=print
    ),
    # GetFile
    "getfileattr": lambda rest: rdcp_command.GetFileAttributes(rest[0],
   handler=print), "getgamma": lambda _: rdcp_command.GetGamma(handler=print),
    "getmem": _get_mem,
    # GetMemBinary
    "getpalette": lambda rest: rdcp_command.GetPalette(rest[0], handler=print),
    "getpid": lambda _: rdcp_command.GetProcessID(handler=print),
    "getchecksum": lambda rest: rdcp_command.GetChecksum(
        int(rest[0], 0), int(rest[1], 0), int(rest[2], 0), handler=print
    ),
    "getsurface": lambda rest: rdcp_command.GetSurface(rest[0], handler=print),
    # GetUserPrivileges
    "getutilitydriveinfo": lambda _:
   rdcp_command.GetUtilityDriveInfo(handler=print), "go": lambda _:
   rdcp_command.Go(handler=print), # GPUCounter "halt": _halt, "isbreak": lambda
   rest: rdcp_command.IsBreak(int(rest[0], 0), handler=print), "isdebugger":
   lambda _: rdcp_command.IsDebugger(handler=print), "isstopped": lambda rest:
   rdcp_command.IsStopped(int(rest[0], 0), handler=print), "irtsweep": lambda _:
   rdcp_command.IRTSweep(handler=print), "kd": _kernel_debug, # LOP "run":
   _magic_boot, # MemTrack "memorymap": lambda _:
   rdcp_command.MemoryMapGlobal(handler=print), "mkdir": lambda rest:
   rdcp_command.Mkdir(rest[0], handler=print), "modlongname": lambda rest:
   rdcp_command.ModLongName(rest[0], handler=print), "modsections": lambda rest:
   rdcp_command.ModSections(rest[0], handler=print), "modules": _modules,
    "nostopon": _no_stop_on,

    "notifyat": _notifyat,

    # PBSnap
    "performancecounterlist": lambda _: rdcp_command.PerformanceCounterList(
        handler=print
    ),
    # PDBInfo
    # PSSnap
    # QueryPerformanceCounter
    "mv": lambda rest: rdcp_command.Rename(rest[0], rest[1], handler=print),
    "reboot": _reboot,
    "resume": lambda rest: rdcp_command.Resume(rest[0], handler=print),
    # Screenshot
    # SendFile
    # SetConfig
    "setcontext": _set_context,
    # SetFileAttributes
    "setmem": _set_mem,
    # SetSystemTime
    "stop": lambda _: rdcp_command.Stop(handler=print),
    "stopon": _stop_on,
    "suspend": lambda rest: rdcp_command.Suspend(int(rest[0], 0),
   handler=print), "systemtime": lambda _:
   rdcp_command.SystemTime(handler=print), "threadinfo": lambda rest:
   rdcp_command.ThreadInfo(int(rest[0], 0), handler=print), "threads": lambda _:
   rdcp_command.Threads(handler=print), # UserList # VSSnap "memwalk":
   _walk_memory, "walkmem": _walk_memory, # WriteFile "xbeinfo": _xbe_info,
    "xtlinfo": lambda _: rdcp_command.XTLInfo(handler=print),
   */
}

void Shell::Run() {
  std::string line;
  std::vector<std::string> args;

  while (true) {
    std::cout << prompt_;
    std::getline(std::cin, line);

    boost::algorithm::trim(line);
    if (line.empty()) {
      continue;
    }

    boost::algorithm::split(args, line, boost::algorithm::is_space(),
                            boost::algorithm::token_compress_on);

    Command::Result result = ProcessCommand(args);
    if (result == Command::EXIT_REQUESTED) {
      break;
    }

    if (result == Command::UNHANDLED) {
      std::cout << "Unknown command." << std::endl;
    }
  }
}

Command::Result Shell::ProcessCommand(std::vector<std::string> &args) {
  std::string command = args.front();
  boost::algorithm::to_lower(command);
  args.erase(args.begin());

  if (command == "help" || command == "?") {
    PrintHelp(args);
    return Command::HANDLED;
  }

  auto it = commands_.find(command);
  if (it != commands_.end()) {
    Command &handler = *it->second;
    return handler(*interface_, args);
  }

  return Command::UNHANDLED;
}

void Shell::PrintHelp(std::vector<std::string> &args) const {
  if (args.empty()) {
    std::cout << "Commands:" << std::endl;

    for (auto &it : commands_) {
      std::cout << it.first << std::endl;
    }
    return;
  }

  std::string target = args.front();
  boost::algorithm::to_lower(target);

  if (target == "help") {
    std::cout << "[command]" << std::endl;
    std::cout << "With no argument: print all commands." << std::endl;
    std::cout << "With argument: print detailed help about `command`."
              << std::endl;
    return;
  }

  auto it = commands_.find(target);
  if (it != commands_.end()) {
    Command &handler = *it->second;
    handler.PrintUsage();
    return;
  }

  std::cout << "Unknown command '" << target << "'" << std::endl;
}