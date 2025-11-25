#include "dyndxt_commands.h"

#include <fstream>

#include "dyndxt_loader/dxt_library.h"
#include "dyndxt_loader/dyndxt_requests.h"
#include "dyndxt_loader/loader.h"
#include "xbox/debugger/debugger_xbox_interface.h"
#include "xbox/debugger/xbdm_debugger.h"

Command::Result DynDXTCommandLoadBootstrap::operator()(
    XBOXInterface& base_interface, const ArgParser&, std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  auto debugger = interface.Debugger();
  if (!debugger) {
    out << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  auto request = std::make_shared<::Stop>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << "Failed to stop target." << std::endl;
    return HANDLED;
  }

  // The status will be ERR_UNEXPECTED if the target was already stopped.
  bool should_go = request->status == OK;

  if (!debugger->HaltAll()) {
    out << "Failed to halt target." << std::endl;
  }

  bool successful = DynDXTLoader::Loader::Bootstrap(interface);
  if (!successful) {
    out << "Failed to inject handler loader. XBDM handlers will not work."
        << std::endl;
  }

  if (!debugger->ContinueAll()) {
    out << "Failed to resume target." << std::endl;
  }

  if (should_go) {
    if (!debugger->Go()) {
      out << "Failed to go." << std::endl;
    }
  }

  return HANDLED;
}

Command::Result DynDXTCommandHello::operator()(XBOXInterface& interface,
                                               const ArgParser&,
                                               std::ostream& out) {
  if (!DynDXTLoader::Loader::Bootstrap(interface)) {
    out << "Failed to install Dynamic DXT loader.";
    return HANDLED;
  }

  auto request = std::make_shared<DynDXTLoader::InvokeMultiline>("ddxt!hello");
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
    return HANDLED;
  }

  out << *request << std::endl;
  return HANDLED;
}

Command::Result DynDXTCommandInvokeSimple::operator()(XBOXInterface& interface,
                                                      const ArgParser& args,
                                                      std::ostream& out) {
  ArgParser parser(args);
  std::string command;
  if (!parser.Parse(0, command)) {
    out << "Missing required `processor!command` argument." << std::endl;
    return HANDLED;
  }
  std::string command_line_args;
  parser.Parse(1, command_line_args);

  auto request =
      std::make_shared<DynDXTLoader::InvokeSimple>(command, command_line_args);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
    return HANDLED;
  }

  out << *request << std::endl;

  return HANDLED;
}

Command::Result DynDXTCommandInvokeMultiline::operator()(
    XBOXInterface& interface, const ArgParser& args, std::ostream& out) {
  ArgParser parser(args);
  std::string command;
  if (!parser.Parse(0, command)) {
    out << "Missing required `processor!command` argument." << std::endl;
    return HANDLED;
  }
  std::string command_line_args;
  parser.Parse(1, command_line_args);

  auto request = std::make_shared<DynDXTLoader::InvokeMultiline>(
      command, command_line_args);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
    return HANDLED;
  }

  out << *request << std::endl;

  return HANDLED;
}

Command::Result DynDXTCommandInvokeSendBinary::operator()(
    XBOXInterface& interface, const ArgParser& args, std::ostream& out) {
  ArgParser parser(args);
  std::string command;
  if (!parser.Parse(0, command)) {
    out << "Missing required `processor!command` argument." << std::endl;
    return HANDLED;
  }
  std::string file_path;
  if (!parser.Parse(1, file_path)) {
    out << "Missing required `binary_path` argument." << std::endl;
    return HANDLED;
  }
  std::string command_line_args;
  parser.Parse(2, command_line_args);

  FILE* fp = fopen(file_path.c_str(), "rb");
  if (!fp) {
    out << "Failed to open '" << file_path << "'" << std::endl;
    return HANDLED;
  }

  std::vector<uint8_t> data;
  uint8_t buf[4096];
  while (!feof(fp)) {
    size_t bytes_read = fread(buf, 1, sizeof(buf), fp);
    if (bytes_read) {
      std::copy(buf, buf + bytes_read, std::back_inserter(data));
    } else if (!feof(fp) && ferror(fp)) {
      out << "Failed to read '" << file_path << "'";
      return HANDLED;
    }
  }

  auto request = std::make_shared<DynDXTLoader::InvokeSendBinary>(
      command, data, command_line_args);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
    return HANDLED;
  }

  out << *request << std::endl;

  return HANDLED;
}

Command::Result DynDXTCommandInvokeReceiveSizePrefixedBinary::operator()(
    XBOXInterface& interface, const ArgParser& args, std::ostream& out) {
  ArgParser parser(args);
  std::string command;
  if (!parser.Parse(0, command)) {
    out << "Missing required `processor!command` argument." << std::endl;
    return HANDLED;
  }
  std::string file_path;
  if (!parser.Parse(1, file_path)) {
    out << "Missing required `save_path` argument." << std::endl;
    return HANDLED;
  }
  std::string command_line_args;
  parser.Parse(2, command_line_args);

  auto request =
      std::make_shared<DynDXTLoader::InvokeReceiveSizePrefixedBinary>(
          command, command_line_args);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
    return HANDLED;
  }

  // TODO: Save the content.

  out << *request << std::endl;

  return HANDLED;
}

Command::Result DynDXTCommandInvokeReceiveKnownSizedBinary::operator()(
    XBOXInterface& interface, const ArgParser& args, std::ostream& out) {
  ArgParser parser(args);
  std::string command;
  if (!parser.Parse(0, command)) {
    out << "Missing required `processor!command` argument." << std::endl;
    return HANDLED;
  }
  std::string file_path;
  if (!parser.Parse(1, file_path)) {
    out << "Missing required `save_path` argument." << std::endl;
    return HANDLED;
  }
  uint32_t size;
  if (!parser.Parse(2, size)) {
    out << "Missing required `size_in_bytes` argument." << std::endl;
    return HANDLED;
  }
  std::string command_line_args;
  parser.Parse(3, command_line_args);

  auto request = std::make_shared<DynDXTLoader::InvokeReceiveKnownSizedBinary>(
      command, size, command_line_args);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
    return HANDLED;
  }

  // TODO: Save the content.

  out << *request << std::endl;

  return HANDLED;
}

Command::Result DynDXTCommandLoad::operator()(XBOXInterface& interface,
                                              const ArgParser& args,
                                              std::ostream& out) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    out << "Missing required <dll_path> argument." << std::endl;
    return HANDLED;
  }

  if (!DynDXTLoader::Loader::Load(interface, path)) {
    out << "Load failed." << std::endl;
    return HANDLED;
  }

  return HANDLED;
}
