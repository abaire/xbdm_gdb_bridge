#ifndef XBDM_GDB_BRIDGE_SHELL_COMMANDS_H
#define XBDM_GDB_BRIDGE_SHELL_COMMANDS_H

#include <iostream>

#include "shell/command.h"

struct ShellCommandQuit : Command {
  ShellCommandQuit() : Command("Terminate the connection and exit.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    return Result::EXIT_REQUESTED;
  }
};

struct ShellCommandReconnect : Command {
  ShellCommandReconnect()
      : Command("Attempt to disconnect and reconnect from XBDM.") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &) override {
    if (interface.ReconnectXBDM()) {
      std::cout << "Connected." << std::endl;
    } else {
      std::cout << "Failed to connect." << std::endl;
    }
    return Result::HANDLED;
  }
};

struct ShellCommandGDB : Command {
  ShellCommandGDB()
      : Command(
            "Start GDB <-> XBDM service.",
            "[IP]:port [xbe_launch_path]\n"
            "\n"
            "Start a GDB server, allowing GDB to communicate with the XBDM "
            "target.\n"
            "\n"
            "[IP]:port - The IP and port at which GDB can connect.\n"
            "              The IP is optional, where the default behavior is "
            "bind to all local interfaces.\n"
            "[xbe_launch_path] - An XBOX path to an XBE (or directory "
            "containing a default.xbe) that should be launched "
            "when a GDB debugger first connects.\n") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override {
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
};

#endif  // XBDM_GDB_BRIDGE_SHELL_COMMANDS_H
