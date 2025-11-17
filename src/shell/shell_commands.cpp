#include "shell_commands.h"

#include <boost/algorithm/string/case_conv.hpp>

#include "tracer_commands.h"

Command::Result ShellCommandReconnect::operator()(
    XBOXInterface& interface, const std::vector<std::string>&,
    std::ostream& out) {
  if (interface.ReconnectXBDM()) {
    out << "Connected." << std::endl;
  } else {
    out << "Failed to connect." << std::endl;
  }
  return Result::HANDLED;
}

Command::Result ShellCommandTrace::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  std::vector<std::string> attach_args;
  std::vector<std::string> trace_args;

  auto it = args.begin();
  while (it != args.end()) {
    auto key = boost::algorithm::to_lower_copy(*it++);
    if (it == args.end()) {
      out << "Invalid argument list, missing value for argument '" << key << "'"
          << std::endl;
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
    init(interface, attach_args, out);
  }

  {
    auto trace = TracerCommandTraceFrames();
    trace(interface, trace_args, out);
  }

  {
    auto detach = TracerCommandDetach();
    detach(interface, {}, out);
  }

  return HANDLED;
}
