#include "tracer_commands.h"

#include <filesystem>
#include <vector>

#include "tracer/tracer.h"

Command::Result TracerCommandInit::operator()(
    XBOXInterface &interface, const std::vector<std::string> &) {
  if (!NTRCTracer::Tracer::Initialize(interface)) {
    std::cout << "Failed to initialize tracer." << std::endl;
    return HANDLED;
  }

  if (!NTRCTracer::Tracer::Attach(interface)) {
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
  if (args.size() > 1) {
    try {
      auto explicit_path = std::filesystem::path(args[0]);
      if (explicit_path.is_relative()) {
        local_artifact_path /= explicit_path;
      } else {
        local_artifact_path = explicit_path;
      }
    } catch (...) {
      std::cout << "Invalid 'local_artifact_path' argument." << std::endl;
      return HANDLED;
    }
  }

  uint32_t num_frames = 1;
  if (args.size() > 2) {
    try {
      num_frames = std::stoi(args[1]);
    } catch (std::invalid_argument &e) {
      std::cout << "Invalid 'num_frames' argument." << std::endl;
      return HANDLED;
    }
  }

  if (!NTRCTracer::Tracer::BreakOnFrameStart(interface, false)) {
    std::cout << "Failed to request break on frame start." << std::endl;
    return HANDLED;
  }

  if (!NTRCTracer::Tracer::TraceFrames(interface, local_artifact_path,
                                       num_frames)) {
    std::cout << "Failed to trace frames." << std::endl;
    return HANDLED;
  }

  return HANDLED;
}
