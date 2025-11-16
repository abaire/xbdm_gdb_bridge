#include "shell/gdb/gdb_commands.h"

#include "shell/shell.h"
#include "xbox/bridge/gdb_xbox_interface.h"

void RegisterGDBCommands(Shell& shell) {
  shell.RegisterCommand("gdb", std::make_shared<ShellCommandGDB>());
}

Command::Result ShellCommandGDB::operator()(
    XBOXInterface& base_interface, const std::vector<std::string>& args,
    std::ostream& out) {
  GET_GDBXBOXINTERFACE(base_interface, interface);

  std::vector<std::string> components;
  IPAddress address;

  if (args.empty()) {
    out << "Missing required port argument." << std::endl;
    PrintUsage();
    return Result::HANDLED;
  }
  address = IPAddress(args.front());

  if (!interface.StartGDBServer(address)) {
    out << "Failed to start GDB server." << std::endl;
    return Result::HANDLED;
  }

  if (args.size() > 1) {
    interface.SetGDBLaunchTarget(args.back());
  }

  if (!interface.GetGDBListenAddress(address)) {
    out << "GDB server failed to bind." << std::endl;
    interface.ClearGDBLaunchTarget();
  } else {
    out << "GDB server listening at Address " << address << std::endl;
  }

  return Result::HANDLED;
}
