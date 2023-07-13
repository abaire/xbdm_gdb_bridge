#include "tracer_commands.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <filesystem>
#include <vector>

#include "tracer/tracer.h"
#include "util/parsing.h"

Command::Result TracerCommandInit::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  if (!NTRCTracer::Tracer::Initialize(interface)) {
    std::cout << "Failed to initialize tracer." << std::endl;
    return HANDLED;
  }

  std::map<std::string, bool> opts = {{"tex", true},     {"depth", false},
                                      {"color", true},   {"rdi", false},
                                      {"pgraph", false}, {"pfb", false}};
  auto it = args.begin();
  while (it != args.end()) {
    auto key = boost::algorithm::to_lower_copy(*it++);
    if (opts.find(key) == opts.end()) {
      std::cout << "Unknown parameter '" << key << "'." << std::endl;
      return HANDLED;
    }

    if (it == args.end()) {
      std::cout << "Invalid argument list, missing value for argument '" << key
                << "'" << std::endl;
      return HANDLED;
    }

    auto param = boost::algorithm::to_lower_copy(*it++);
    opts[key] = param == "t" || param == "true" || param == "y" ||
                param == "yes" || param == "on" || param == "1";
  }

  if (!NTRCTracer::Tracer::Attach(interface, opts["tex"], opts["depth"],
                                  opts["color"], opts["rdi"], opts["pgraph"],
                                  opts["pfb"])) {
    std::cout << "Failed to attach to tracer." << std::endl;
    return HANDLED;
  }

  return HANDLED;
}

Command::Result TracerCommandDetach::operator()(
    XBOXInterface &interface, const std::vector<std::string> &) {
  if (!NTRCTracer::Tracer::Detach(interface)) {
    std::cout << "Failed to detach from the tracer." << std::endl;
    return HANDLED;
  }

  return HANDLED;
}

Command::Result TracerCommandBreakOnNextFlip::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  if (!NTRCTracer::Tracer::BreakOnFrameStart(interface, !args.empty())) {
    std::cout << "Failed to request break." << std::endl;
    return HANDLED;
  }

  return HANDLED;
}

Command::Result TracerCommandTraceFrames::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  auto local_artifact_path = std::filesystem::current_path();
  auto num_frames = 1;
  auto verbose = false;

  auto it = args.begin();
  while (it != args.end()) {
    auto key = boost::algorithm::to_lower_copy(*it++);

    if (key == "verbose") {
      verbose = true;
      continue;
    }

    if (it == args.end()) {
      std::cout << "Invalid argument list, missing value for argument '" << key
                << "'" << std::endl;
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
        std::cout << "Invalid '" << key << "' argument." << std::endl;
        return HANDLED;
      }
    } else if (key == "frames") {
      try {
        num_frames = std::stoi((*it++));
      } catch (std::invalid_argument &e) {
        std::cout << "Invalid '" << key << "' argument." << std::endl;
        return HANDLED;
      }
    } else {
      std::cout << "Unknown config argument '" << key << "'" << std::endl;
    }
  }

  if (!NTRCTracer::Tracer::BreakOnFrameStart(interface, false)) {
    std::cout << "Failed to request break on frame start." << std::endl;
    return HANDLED;
  }

  if (!NTRCTracer::Tracer::TraceFrames(interface, local_artifact_path,
                                       num_frames, verbose)) {
    std::cout << "Failed to trace frames." << std::endl;
    return HANDLED;
  }

  return HANDLED;
}
