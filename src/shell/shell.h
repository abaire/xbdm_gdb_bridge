#ifndef XBDM_GDB_BRIDGE_SHELL_H
#define XBDM_GDB_BRIDGE_SHELL_H

#include <map>
#include <memory>
#include <optional>
#include <string>

namespace replxx {
class Replxx;
}

#include "shell/command.h"
#include "util/parsing.h"
#include "xbox/xbox_interface.h"

class Shell {
 public:
  explicit Shell(std::shared_ptr<XBOXInterface>& interface);
  ~Shell();

  void Run();
  Command::Result ProcessCommand(ArgParser parser);

  void RegisterCommand(const std::string& command,
                       std::shared_ptr<Command> processor) {
    std::vector<std::string> empty;
    RegisterCommand(command, processor, empty);
  }
  void RegisterCommand(const std::string& command,
                       std::shared_ptr<Command> processor,
                       const std::vector<std::string>& aliases);

 private:
  void PrintHelp(const ArgParser& parser) const;

 private:
  std::shared_ptr<XBOXInterface> interface_;
  std::string prompt_;
  std::map<std::string, std::shared_ptr<Command>> commands_;


  std::unique_ptr<replxx::Replxx> rx_;
  std::optional<ArgParser> last_command_;
};

#endif  // XBDM_GDB_BRIDGE_SHELL_H
