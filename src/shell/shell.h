#ifndef XBDM_GDB_BRIDGE_SHELL_H
#define XBDM_GDB_BRIDGE_SHELL_H

#include <map>
#include <memory>
#include <string>

#include "shell/command.h"
#include "xbox/xbox_interface.h"

class Shell {
 public:
  explicit Shell(std::shared_ptr<XBOXInterface> &interface);

  void Run();

 private:
  Command::Result ProcessCommand(std::vector<std::string> &args);
  void PrintHelp(std::vector<std::string> &args) const;

 private:
  std::shared_ptr<XBOXInterface> interface_;
  std::string prompt_;
  std::map<std::string, std::shared_ptr<Command>> commands_;
};

#endif  // XBDM_GDB_BRIDGE_SHELL_H
