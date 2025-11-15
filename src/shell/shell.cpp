#include "shell.h"

#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <iostream>
#include <string>

#include "commands.h"
#include "configure.h"
#include "debugger_commands.h"
#include "dyndxt_commands.h"
#include "macro_commands.h"
#include "replxx.hpp"
#include "shell_commands.h"
#include "tracer_commands.h"
#include "util/logging.h"

#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
#include "util/timer.h"
#endif

Shell::Shell(std::shared_ptr<XBOXInterface>& interface)
    : interface_(interface), prompt_("> ") {
#define REGISTER(command, handler)                    \
  do {                                                \
    commands_[command] = std::make_shared<handler>(); \
  } while (0)

#define ALIAS(command, alias)              \
  do {                                     \
    commands_[alias] = commands_[command]; \
  } while (0)

  commands_["help"] = nullptr;
  commands_["?"] = nullptr;
  commands_["!"] = nullptr;
  REGISTER("gdb", ShellCommandGDB);
  REGISTER("trace", ShellCommandTrace);
  REGISTER("reconnect", ShellCommandReconnect);
  REGISTER("quit", ShellCommandQuit);
  ALIAS("quit", "exit");

  REGISTER("/run", DebuggerCommandRun);
  REGISTER("/launch", DebuggerCommandLaunch);
  REGISTER("/launchwait", DebuggerCommandLaunchWait);
  REGISTER("/attach", DebuggerCommandAttach);
  REGISTER("/detach", DebuggerCommandDetach);
  REGISTER("/restart", DebuggerCommandRestart);
  REGISTER("/switch", DebuggerCommandSetActiveThread);
  REGISTER("/threads", DebuggerCommandGetThreads);
  REGISTER("/info", DebuggerCommandGetThreadInfo);
  REGISTER("/infowithcontext", DebuggerCommandGetThreadInfoAndContext);
  ALIAS("/infowithcontext", "/ic");
  //  REGISTER("/stepi", DebuggerCommandStepInstruction);
  REGISTER("/info", DebuggerCommandGetThreadInfo);
  REGISTER("/haltall", DebuggerCommandHaltAll);
  REGISTER("/halt", DebuggerCommandHalt);
  REGISTER("/continueall", DebuggerCommandContinueAll);
  REGISTER("/continueallgo", DebuggerCommandContinueAllAndGo);
  ALIAS("/continueallgo", "/cag");

  REGISTER("/continue", DebuggerCommandContinue);
  REGISTER("/suspend", DebuggerCommandSuspend);
  REGISTER("/resume", DebuggerCommandResume);
  REGISTER("/modules", DebuggerCommandGetModules);
  REGISTER("/sections", DebuggerCommandGetSections);
  REGISTER("/stepfun", DebuggerCommandStepFunction);
  ALIAS("/stepfun", "/stepf");

  REGISTER("@bootstrap", DynDXTCommandLoadBootstrap);
  REGISTER("@hello", DynDXTCommandHello);
  REGISTER("@load", DynDXTCommandLoad);
  auto invoke_simple = std::make_shared<DynDXTCommandInvokeSimple>();
  commands_["@"] = invoke_simple;
  commands_["@simple"] = invoke_simple;
  auto invoke_multiline = std::make_shared<DynDXTCommandInvokeMultiline>();
  commands_["@multiline"] = invoke_multiline;
  commands_["@m"] = invoke_multiline;
  auto invoke_sendbin = std::make_shared<DynDXTCommandInvokeSendBinary>();
  commands_["@sendbin"] = invoke_sendbin;
  commands_["@sb"] = invoke_sendbin;
  auto invoke_recvbin =
      std::make_shared<DynDXTCommandInvokeReceiveSizePrefixedBinary>();
  commands_["@recvbin"] = invoke_recvbin;
  commands_["@rbin"] = invoke_recvbin;
  auto invoke_recvbytes =
      std::make_shared<DynDXTCommandInvokeReceiveKnownSizedBinary>();
  commands_["@recvbytes"] = invoke_recvbytes;
  commands_["@rby"] = invoke_recvbytes;
  commands_["@rbytes"] = invoke_recvbytes;

  REGISTER("$init", TracerCommandInit);
  REGISTER("$detach", TracerCommandDetach);
  REGISTER("$stepflip", TracerCommandBreakOnNextFlip);
  REGISTER("$trace", TracerCommandTraceFrames);

  REGISTER("altaddr", CommandAltAddr);
  REGISTER("break", CommandBreak);
  REGISTER("bye", CommandBye);
  REGISTER("continue", CommandContinue);
  REGISTER("debugoptions", CommandDebugOptions);
  REGISTER("debugger", CommandDebugger);
  REGISTER("dedicate", CommandDedicate);
  REGISTER("dmversion", CommandDebugMonitorVersion);
  REGISTER("rm", CommandDelete);
  REGISTER("ls", CommandDirList);
  REGISTER("df", CommandDriveFreeSpace);
  REGISTER("drivelist", CommandDriveList);
  REGISTER("getchecksum", CommandGetChecksum);
  REGISTER("getcontext", CommandGetContext);
  REGISTER("getextcontext", CommandGetExtContext);
  REGISTER("getfile", CommandGetFile);
  REGISTER("getfileattr", CommandGetFileAttributes);
  REGISTER("getmem", CommandGetMem);
  REGISTER("getpid", CommandGetProcessID);
  REGISTER("getutilitydriveinfo", CommandGetUtilityDriveInfo);
  REGISTER("go", CommandGo);
  REGISTER("halt", CommandHalt);
  REGISTER("isbreak", CommandIsBreak);
  REGISTER("isdebugger", CommandIsDebugger);
  REGISTER("isstopped", CommandIsStopped);
  REGISTER("run", CommandMagicBoot);
  auto mem_map = std::make_shared<CommandMemoryMap>();
  commands_["memorymap"] = mem_map;
  commands_["mem_map"] = mem_map;
  REGISTER("mkdir", CommandMakeDirectory);
  REGISTER("modsections", CommandModuleSections);
  REGISTER("modules", CommandModules);
  REGISTER("nostopon", CommandNoStopOn);
  REGISTER("notifyat", CommandNotifyAt);
  REGISTER("mv", CommandRename);
  REGISTER("putfile", CommandPutFile);
  REGISTER("reboot", CommandReboot);
  REGISTER("resume", CommandResume);
  REGISTER("screenshot", CommandScreenshot);
  //  REGISTER("setcontext", CommandSetContext);
  REGISTER("setmem", CommandSetMem);
  REGISTER("stop", CommandStop);
  REGISTER("stopon", CommandStopOn);
  REGISTER("suspend", CommandSuspend);
  REGISTER("threadinfo", CommandThreadInfo);
  REGISTER("threads", CommandThreads);
  auto walk_mem = std::make_shared<CommandWalkMem>();
  commands_["memwalk"] = walk_mem;
  commands_["walkmem"] = walk_mem;
  REGISTER("xbeinfo", CommandXBEInfo);
  REGISTER("xtlinfo", CommandXTLInfo);

  // Macro commands perform some interesting logic and generally invoke several
  // raw commands. They start with the percent sign (%) character.
  REGISTER("%syncfile", MacroCommandSyncFile);
  REGISTER("%syncdir", MacroCommandSyncDirectory);

#undef ALIAS
#undef REGISTER

  rx_.set_word_break_characters(" \t");
  using std::placeholders::_1;
  using std::placeholders::_2;
  using std::placeholders::_3;
  using std::placeholders::_4;

  for (auto const& [command, _] : commands_) {
    rx_.add_completion(command);
  }
}

void Shell::Run() {
  std::string line;

  while (true) {
    char const* cinput{nullptr};

    do {
      cinput = rx_.input(prompt_);
    } while ((cinput == nullptr) && (errno == EAGAIN));

    if (cinput == nullptr) {
      break;
    }

    line = cinput;

    boost::algorithm::trim(line);
    if (line.empty()) {
      continue;
    }

    std::vector<std::string> args;
    boost::escaped_list_separator<char> separator('~', ' ', '\"');
    typedef boost::tokenizer<boost::escaped_list_separator<char>>
        SpaceTokenizer;
    SpaceTokenizer keyvals(line, separator);
    for (auto it = keyvals.begin(); it != keyvals.end(); ++it) {
      const std::string& token = *it;
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
    rx_.history_add(line);
  }
}

Command::Result Shell::ProcessCommand(
    const std::vector<std::string>& command_args) {
  auto args = command_args;
  std::string command = args.front();

  if (command == "!") {
    if (last_command_.empty()) {
      std::cout << "No command to replay." << std::endl;
      return Command::HANDLED;
    }
    for (auto& element : last_command_) {
      std::cout << element << " ";
    }
    std::cout << std::endl;

    args = last_command_;
    command = args.front();
  }

  last_command_ = args;

  boost::algorithm::to_lower(command);
  args.erase(args.begin());

  if (command == "help" || command == "?") {
    PrintHelp(args);
    return Command::HANDLED;
  }

  auto it = commands_.find(command);
  if (it != commands_.end()) {
#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
    LOG(trace) << "Processing shell command '" << command << "'";
    auto timer = Timer();
    timer.Start();
#endif

    Command& handler = *it->second;
    auto ret = handler(*interface_, args);

#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
    LOG(trace) << "... processed shell command '" << command << "'  in "
               << timer.FractionalMillisecondsElapsed() << " ms";
#endif
    return ret;
  }

  return Command::UNHANDLED;
}

static char kRerunCommandHelp[] = "Re-runs the last shell command.";

void Shell::PrintHelp(std::vector<std::string>& args) const {
  if (args.empty()) {
    std::cout << "Commands:" << std::endl;

    for (auto& it : commands_) {
      std::cout << it.first << " - ";
      if (it.second) {
        std::cout << it.second->short_help;
      } else {
        if (it.first == "!") {
          std::cout << kRerunCommandHelp;
        } else {
          std::cout
              << "Print this help list (pass an argument for detailed help).";
        }
      }
      std::cout << std::endl;
    }
    return;
  }

  std::string target = args.front();
  boost::algorithm::to_lower(target);

  if (target == "help" || target == "?") {
    std::cout << "[command]" << std::endl;
    std::cout << "With no argument: print all commands." << std::endl;
    std::cout << "With argument: print detailed help about `command`."
              << std::endl;
    return;
  }

  if (target == "!") {
    std::cout << kRerunCommandHelp << std::endl;
    return;
  }

  auto it = commands_.find(target);
  if (it != commands_.end()) {
    Command& handler = *it->second;
    handler.PrintUsage();
    return;
  }

  std::cout << "Unknown command '" << target << "'" << std::endl;
}
