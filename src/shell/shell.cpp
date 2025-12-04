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
#include "replxx.hxx"
#include "shell_commands.h"
#include "tracer_commands.h"
#include "util/config_path.h"
#include "util/logging.h"

#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
#include "util/timer.h"
#endif

static constexpr char kRerunCommandHelp[] = "Re-runs the last shell command.";
static constexpr char kAppName[] = "xbdm_gdb_bridge";
static constexpr char kHistoryFilename[] = "shell_history";

Shell::Shell(std::shared_ptr<XBOXInterface>& interface)
    : interface_(interface),
      prompt_("> "),
      rx_(std::make_unique<replxx::Replxx>()) {
  rx_->history_load(config_path::GetConfigFilePath(kAppName, kHistoryFilename));

  rx_->set_completion_callback(
      [this](std::string const& context,
             int& context_length) -> replxx::Replxx::completions_t {
        std::vector<replxx::Replxx::Completion> completions;

        size_t lastSpace = context.find_last_of(" \t");
        std::string prefix;

        if (lastSpace == std::string::npos) {
          prefix = context;
          context_length = prefix.length();
        } else {
          return {};
        }

        for (const auto& [cmd, handler] : commands_) {
          if (cmd.find(prefix) == 0) {
            completions.emplace_back(cmd);
          }
        }

        return completions;
      });

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
  REGISTER("/whichthread", DebuggerCommandWhichThread);
  ALIAS("/whichthread", "/wt");
  REGISTER("/info", DebuggerCommandGetThreadInfo);
  REGISTER("/infowithcontext", DebuggerCommandGetThreadInfoAndContext);
  ALIAS("/infowithcontext", "/ic");
  REGISTER("/autoinfo", DebuggerCommandSetAutoInfo);
  REGISTER("/haltall", DebuggerCommandHaltAll);
  REGISTER("/halt", DebuggerCommandHalt);
  REGISTER("/continueall", DebuggerCommandContinueAll);
  REGISTER("/continueallgo", DebuggerCommandContinueAllAndGo);
  ALIAS("/continueallgo", "/cag");

  REGISTER("/disassemble", DebuggerCommandDisassemble);
  ALIAS("/disassemble", "/disasm");
  ALIAS("/disassemble", "/u");

  REGISTER("/continue", DebuggerCommandContinue);
  REGISTER("/suspend", DebuggerCommandSuspend);
  REGISTER("/resume", DebuggerCommandResume);
  REGISTER("/modules", DebuggerCommandGetModules);
  REGISTER("/sections", DebuggerCommandGetSections);
  REGISTER("/stepi", DebuggerCommandStepInstruction);
  ALIAS("/stepi", "/si");
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
  REGISTER("modlong", CommandModuleLongName);
  ALIAS("modlong", "modpath");
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
}

Shell::~Shell() = default;

void Shell::RegisterCommand(const std::string& command,
                            std::shared_ptr<Command> processor,
                            const std::vector<std::string>& aliases) {
  commands_[command] = processor;
  for (auto& alias : aliases) {
    commands_[alias] = processor;
  }
}

void Shell::Run() {
  while (true) {
    char const* c_line{nullptr};

    do {
      c_line = rx_->input(prompt_);
    } while (c_line && strlen(c_line) == 0);

    if (!c_line) {
      break;
    }

    std::string line = c_line;
    boost::algorithm::trim(line);
    if (line.empty()) {
      continue;
    }

    rx_->history_add(line);

    auto args = ArgParser(line);

    Command::Result result = ProcessCommand(std::move(args));
    if (result == Command::EXIT_REQUESTED) {
      break;
    }

    if (result == Command::UNHANDLED) {
      std::cout << "Unknown command." << std::endl;
    }
  }
  rx_->history_save(config_path::GetConfigFilePath(kAppName, kHistoryFilename));
}

Command::Result Shell::ProcessCommand(ArgParser parser) {
  if (parser.command == "!") {
    if (!last_command_.has_value()) {
      std::cout << "No command to replay." << std::endl;
      return Command::HANDLED;
    }

    parser = *last_command_;

    std::cout << parser.command;
    for (const auto& arg : parser.arguments) {
      std::cout << " " << arg.value;
    }
    std::cout << std::endl;
  }

  last_command_ = parser;

  if (parser.command == "help" || parser.command == "?") {
    PrintHelp(parser);
    return Command::HANDLED;
  }

  // 4. Find and Execute
  // parser.command is already lower-cased by the ArgParser constructor
  auto it = commands_.find(parser.command);
  if (it != commands_.end()) {
#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
    LOG(trace) << "Processing shell command '" << parser.command << "'";
    auto timer = Timer();
    timer.Start();
#endif

    Command& handler = *it->second;
    auto ret = handler(*interface_, parser);

#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
    LOG(trace) << "... processed shell command '" << parser.command << "'  in "
               << timer.FractionalMillisecondsElapsed() << " ms";
#endif
    return ret;
  }

  return Command::UNHANDLED;
}

void Shell::PrintHelp(const ArgParser& parser) const {
  if (parser.arguments.empty()) {
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

  std::string target;
  parser.Parse(0, target);
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
