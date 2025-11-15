#ifndef XBDM_GDB_BRIDGE_SHELL_H
#define XBDM_GDB_BRIDGE_SHELL_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "replxx.hpp"
#include "shell/command.h"
#include "xbox/xbox_interface.h"

class Shell {
 public:
  explicit Shell(std::shared_ptr<XBOXInterface>& interface);

  void Run();
  Command::Result ProcessCommand(const std::vector<std::string>& args);

 private:
  void PrintHelp(std::vector<std::string>& args) const;

 private:
  std::shared_ptr<XBOXInterface> interface_;
  std::string prompt_;
  std::map<std::string, std::shared_ptr<Command>> commands_;
  replxx::Replxx rx_;

  std::vector<std::string> last_command_;
};

#endif  // XBDM_GDB_BRIDGE_SHELL_H
