#include "tracer_commands.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <filesystem>
#include <vector>

#include "tracer/tracer.h"
#include "util/parsing.h"

Command::Result TracerCommandInit::operator()(XBOXInterface& interface,
                                              const ArgParser& args,
                                              std::ostream& out) {
  if (!NTRCTracer::Tracer::Initialize(interface)) {
    out << "Failed to initialize tracer." << std::endl;
    return HANDLED;
  }

  std::map<std::string, bool> opts = {{"tex", true},     {"depth", false},
                                      {"color", true},   {"rdi", false},
                                      {"pgraph", false}, {"pfb", false}};
  auto it = args.begin();
  while (it != args.end()) {
    auto key = boost::algorithm::to_lower_copy(*it++);
    if (opts.find(key) == opts.end()) {
      out << "Unknown parameter '" << key << "'." << std::endl;
      return HANDLED;
    }

    if (it == args.end()) {
      out << "Invalid argument list, missing value for argument '" << key << "'"
          << std::endl;
      return HANDLED;
    }

    auto param = boost::algorithm::to_lower_copy(*it++);
    opts[key] = param == "t" || param == "true" || param == "y" ||
                param == "yes" || param == "on" || param == "1";
  }

  if (!NTRCTracer::Tracer::Attach(interface, opts["tex"], opts["depth"],
                                  opts["color"], opts["rdi"], opts["pgraph"],
                                  opts["pfb"])) {
    out << "Failed to attach to tracer." << std::endl;
    return HANDLED;
  }

  return HANDLED;
}

Command::Result TracerCommandDetach::operator()(XBOXInterface& interface,
                                                const ArgParser&,
                                                std::ostream& out) {
  if (!NTRCTracer::Tracer::Detach(interface)) {
    out << "Failed to detach from the tracer." << std::endl;
    return HANDLED;
  }

  return HANDLED;
}

Command::Result TracerCommandBreakOnNextFlip::operator()(
    XBOXInterface& interface, const ArgParser& args, std::ostream& out) {
  if (!NTRCTracer::Tracer::BreakOnFrameStart(interface, !args.empty())) {
    out << "Failed to request break." << std::endl;
    return HANDLED;
  }

  return HANDLED;
}

Command::Result TracerCommandTraceFrames::operator()(XBOXInterface& interface,
                                                     const ArgParser& args,
                                                     std::ostream& out) {
  auto local_artifact_path = std::filesystem::current_path();
  auto num_frames = 1;
  auto verbose = false;
  auto nodiscard = false;

  auto it = args.begin();
  while (it != args.end()) {
    auto key = boost::algorithm::to_lower_copy(*it++);

    if (key == "verbose") {
      verbose = true;
      continue;
    }

    if (key == "nodiscard") {
      nodiscard = true;
      continue;
    }

    if (it == args.end()) {
      out << "Invalid argument list, missing value for argument '" << key << "'"
          << std::endl;
      return HANDLED;
    }

    if (key == "path") {
      try {
        auto explicit_path = std::filesystem::path(*it++);
        if (explicit_path.is_relative()) {
          local_artifact_path /= explicit_path;
        } else {
          local_artifact_path = explicit_path;
        }
      } catch (...) {
        out << "Invalid '" << key << "' argument." << std::endl;
        return HANDLED;
      }
    } else if (key == "frames") {
      try {
        num_frames = std::stoi((*it++));
      } catch (std::invalid_argument& e) {
        out << "Invalid '" << key << "' argument." << std::endl;
        return HANDLED;
      }
    } else if (key == "nodiscard") {
      nodiscard = true;
    } else {
      out << "Unknown config argument '" << key << "'" << std::endl;
    }
  }

  if (!nodiscard && !NTRCTracer::Tracer::BreakOnFrameStart(interface, false)) {
    out << "Failed to request break on frame start." << std::endl;
    return HANDLED;
  }

  if (!NTRCTracer::Tracer::TraceFrames(interface, local_artifact_path,
                                       num_frames, verbose, nodiscard)) {
    out << "Failed to trace frames." << std::endl;
    return HANDLED;
  }

  return HANDLED;
}
