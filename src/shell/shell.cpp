#include "shell.h"

#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <iostream>
#include <string>

#include "commands.h"
#include "debugger_commands.h"
#include "shell_commands.h"

Shell::Shell(std::shared_ptr<XBOXInterface> &interface)
    : interface_(interface), prompt_("> ") {
#define REGISTER(command, handler) \
  commands_[command] = std::make_shared<handler>()

  auto quit = std::make_shared<ShellCommandQuit>();
  commands_["exit"] = quit;
  REGISTER("gdb", ShellCommandGDB);
  commands_["help"] = nullptr;
  commands_["?"] = nullptr;
  REGISTER("reconnect", ShellCommandReconnect);
  commands_["quit"] = quit;

  REGISTER("/launch", DebuggerCommandLaunch);
  REGISTER("/launchwait", DebuggerCommandLaunchWait);
  REGISTER("/attach", DebuggerCommandAttach);
  REGISTER("/detach", DebuggerCommandDetach);
  REGISTER("/restart", DebuggerCommandRestart);
  REGISTER("/switch", DebuggerCommandSetActiveThread);
  REGISTER("/threads", DebuggerCommandGetThreads);
  REGISTER("/info", DebuggerCommandGetThreadInfo);
  REGISTER("/stepi", DebuggerCommandStepInstruction);
  REGISTER("/info", DebuggerCommandGetThreadInfo);
  REGISTER("/context", DebuggerCommandGetContext);
  REGISTER("/fullcontext", DebuggerCommandGetFullContext);
  REGISTER("/haltall", DebuggerCommandHaltAll);
  REGISTER("/halt", DebuggerCommandHalt);
  REGISTER("/continueall", DebuggerCommandContinueAll);
  REGISTER("/continue", DebuggerCommandContinue);
  REGISTER("/suspend", DebuggerCommandSuspend);
  REGISTER("/resume", DebuggerCommandResume);
  auto step_function = std::make_shared<DebuggerCommandStepFunction>();
  commands_["/stepf"] = step_function;
  commands_["/stepfun"] = step_function;

  REGISTER("altaddr", CommandAltAddr);
  REGISTER("break", CommandBreak);
  REGISTER("bye", CommandBye);
  REGISTER("continue", CommandContinue);
  REGISTER("debugoptions", CommandDebugOptions);
  REGISTER("debugger", CommandDebugger);
  REGISTER("rm", CommandDelete);
  /*
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
#undef REGISTER
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

    boost::escaped_list_separator<char> separator('\\', ' ', '\"');
    typedef boost::tokenizer<boost::escaped_list_separator<char>>
        SpaceTokenizer;
    SpaceTokenizer keyvals(line, separator);
    for (auto it = keyvals.begin(); it != keyvals.end(); ++it) {
      const std::string &token = *it;
      if (token[0] == '\"') {
        // Insert the unescaped contents of the string.
        std::string value = token.substr(1, token.size() - 1);
        boost::replace_all(value, "\\\"", "\"");
        args.push_back(value);
      } else {
        args.push_back(token);
      }
    }

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
