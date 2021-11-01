#include "shell.h"

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <string>

#include "debugger_commands.h"
#include "shell_commands.h"

Shell::Shell(std::shared_ptr<XBOXInterface>& interface) : interface_(interface), prompt_("> ") {
  auto quit = std::make_shared<ShellCommandQuit>();
  commands_["exit"] = quit;
  commands_["gdb"] = std::make_shared<ShellCommandGDB>();
  commands_["help"] = nullptr;
  commands_["reconnect"] = std::make_shared<ShellCommandReconnect>();
  commands_["quit"] = quit;

  commands_["/launch"] = std::make_shared<DebuggerCommandLaunch>();
  commands_["/launchwait"] = std::make_shared<DebuggerCommandLaunchWait>();
  commands_["/attach"] = std::make_shared<DebuggerCommandAttach>();
  commands_["/detach"] = std::make_shared<DebuggerCommandDetach>();
  commands_["/restart"] = std::make_shared<DebuggerCommandRestart>();
  commands_[      "/switch"] = std::make_shared<DebuggerCommandSetActiveThread>();
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
}

void Shell::Run() {
  std::string line;
  std::vector<std::string> args;

  while(true) {
    std::cout << prompt_;
    std::getline(std::cin, line);

    boost::algorithm::trim(line);
    if (line.empty()) {
      continue;
    }

    boost::algorithm::split(args, line, boost::algorithm::is_space(), boost::algorithm::token_compress_on);

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

  if (command == "help") {
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
    std::cout <<"Commands:" << std::endl;

    for (auto &it : commands_) {
      std::cout << it.first << std::endl;
    }
    return;
  }

  std::string target = args.front();
  boost::algorithm::to_lower(target);

  if (target == "help") {
    std::cout << "[command]" << std::endl;
    std::cout <<  "With no argument: print all commands." << std::endl;
    std::cout <<  "With argument: print detailed help about `command`." << std::endl;
    return;
  }

  auto it = commands_.find(target);
  if (it != commands_.end()) {
    Command &handler = *it->second;
    std::cout << handler.help << std::endl;
    return;
  }

  std::cout << "Unknown command '" << target << "'" << std::endl;
}