#include "shell_commands.h"

#include <boost/algorithm/string/case_conv.hpp>

#include "tracer_commands.h"
#include "util/parsing.h"

Command::Result ShellCommandReconnect::operator()(XBOXInterface& interface,
                                                  const ArgParser&,
                                                  std::ostream& out) {
  if (interface.ReconnectXBDM()) {
    out << "Connected." << std::endl;
  } else {
    out << "Failed to connect." << std::endl;
  }
  return Result::HANDLED;
}

Command::Result ShellCommandTrace::operator()(XBOXInterface& interface,
                                              const ArgParser& args,
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
    } else if (key == "nodiscard") {
      trace_args.emplace_back(key);
    } else {
      attach_args.emplace_back(key);
      attach_args.emplace_back(*it++);
    }
  }

  {
    ArgParser subcommand_args("init", attach_args);
    auto init = TracerCommandInit();
    init(interface, subcommand_args, out);
  }

  {
    ArgParser subcommand_args("trace", trace_args);
    auto trace = TracerCommandTraceFrames();
    trace(interface, subcommand_args, out);
  }

  {
    ArgParser empty({"detach"});
    auto detach = TracerCommandDetach();
    detach(interface, empty, out);
  }

  return HANDLED;
}
