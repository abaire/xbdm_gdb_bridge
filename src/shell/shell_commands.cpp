#include "shell_commands.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <filesystem>

#include "tracer_commands.h"

Command::Result ShellCommandReconnect::operator()(
    XBOXInterface& interface, const std::vector<std::string>&) {
  if (interface.ReconnectXBDM()) {
    std::cout << "Connected." << std::endl;
  } else {
    std::cout << "Failed to connect." << std::endl;
  }
  return Result::HANDLED;
}

Command::Result ShellCommandGDB::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args) {
  std::vector<std::string> components;
  IPAddress address;

  if (args.empty()) {
    std::cout << "Missing required port argument." << std::endl;
    PrintUsage();
    return Result::HANDLED;
  }
  address = IPAddress(args.front());

  if (!interface.StartGDBServer(address)) {
    std::cout << "Failed to start GDB server." << std::endl;
    return Result::HANDLED;
  }

  if (args.size() > 1) {
    interface.SetGDBLaunchTarget(args.back());
  }

  if (!interface.GetGDBListenAddress(address)) {
    std::cout << "GDB server failed to bind." << std::endl;
    interface.ClearGDBLaunchTarget();
  } else {
    std::cout << "GDB server listening at Address " << address << std::endl;
  }

  return Result::HANDLED;
}

Command::Result ShellCommandTrace::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args) {
  std::vector<std::string> attach_args;
  std::vector<std::string> trace_args;

  auto it = args.begin();
  while (it != args.end()) {
    auto key = boost::algorithm::to_lower_copy(*it++);
    if (it == args.end()) {
      std::cout << "Invalid argument list, missing value for argument '" << key
                << "'" << std::endl;
      return HANDLED;
    }

    if (key == "path" || key == "frames") {
      trace_args.emplace_back(key);
      trace_args.emplace_back(*it++);
    } else {
      attach_args.emplace_back(key);
      attach_args.emplace_back(*it++);
    }
  }

  {
    auto init = TracerCommandInit();
    init(interface, attach_args);
  }

  {
    auto trace = TracerCommandTraceFrames();
    trace(interface, trace_args);
  }

  {
    auto detach = TracerCommandDetach();
    detach(interface, {});
  }

  return HANDLED;
}
