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
            "[ip][:port]\n"
            "\n"
            "Starts a GDB server, allowing GDB to communicate with the XBDM "
            "target.\n"
            "\n"
            "[ip][:port] - The IP and port at which GDB can connect.\n"
            "              Both components are optional. Default IP is to bind "
            "to all local interfaces at an arbitrary port.\n") {}
  Result operator()(XBOXInterface &interface,
                    const std::vector<std::string> &args) override {
    std::vector<std::string> components;
    IPAddress address;

    if (!args.empty()) {
      address = IPAddress(args.front());
    }

    interface.StartGDBServer(address);
    if (!interface.GetGDBListenAddress(address)) {
      std::cout << "Failed to start GDB server." << std::endl;
    } else {
      std::cout << "GDB server listening at address " << address << std::endl;
    }

    return Result::HANDLED;
  }
};

#endif  // XBDM_GDB_BRIDGE_SHELL_COMMANDS_H
