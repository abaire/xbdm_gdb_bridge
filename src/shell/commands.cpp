#include "commands.h"

#include <boost/algorithm/string.hpp>
#include <string>
#include <vector>

#include "util/parsing.h"

static void SendAndPrintMessage(XBOXInterface& interface, const std::shared_ptr<RDCPProcessedRequest>& request) {
  interface.SendCommandSync(request);
  std::cout << *request << std::endl;
}

struct ArgParser {
  explicit ArgParser(const std::vector<std::string> &args) : arguments(args) {
    if (args.empty()) {
      return;
    }

    command = args.front();
    boost::algorithm::to_lower(command);
  }

  [[nodiscard]] bool HasCommand() const { return !arguments.empty(); }
  [[nodiscard]] bool ShiftPrefixModifier(char modifier) {
    if (command.empty() || command[0] != modifier) {
      return false;
    }

    command.erase(0);
    return true;
  }

  template <typename T, typename... Ts>
  using are_same_type = std::conjunction<std::is_same<T, Ts>...>;

  template<typename T>
  [[nodiscard]] bool IsCommand(const T &cmd) const {
    return (command == cmd);
  }

  template<typename T,
            typename... Ts,
            typename = are_same_type<T, Ts...>>
  [[nodiscard]] bool IsCommand(const T &cmd, const Ts &...rest) const {
    if (command == cmd) {
      return true;
    }

    return IsCommand(rest...);
  }

  template<typename T,
      std::enable_if_t<std::is_integral<T>::value && sizeof(T) == 4, int> = 0>
  bool Parse(int arg_index, T &ret) const {
    ++arg_index;
    if (arg_index >= arguments.size()) {
      return false;
    }
    ret = ParseInt32(arguments[arg_index]);
    return true;
  }

  std::string command;
  const std::vector<std::string> &arguments;
};

Command::Result CommandBreak::operator()(XBOXInterface& interface,
                                         const std::vector<std::string>& args) {
    ArgParser parsed(args);
    if (!parsed.HasCommand()) {
      PrintUsage();
      return HANDLED;
    }

    if (parsed.IsCommand("start")) {
      SendAndPrintMessage(interface, std::make_shared<BreakAtStart>());
      return HANDLED;
    }

    if (parsed.IsCommand( "clearall")) {
      SendAndPrintMessage(interface, std::make_shared<BreakClearAll>());
      return HANDLED;
    }

    bool clear = parsed.ShiftPrefixModifier('-');

    if (parsed.IsCommand("a", "addr", "address")) {
      uint32_t address;
      if (!parsed.Parse(0, address)) {
        std::cout << "Missing required address argument." << std::endl;
        PrintUsage();
        return HANDLED;
      }
      SendAndPrintMessage(interface, std::make_shared<BreakAddress>(address, clear));
      return HANDLED;
    }

  if (parsed.IsCommand("r", "read")) {
    uint32_t address;
    if (!parsed.Parse(0, address)) {
      std::cout << "Missing required address argument." << std::endl;
      PrintUsage();
      return HANDLED;
    }
    int32_t size = 1;
    parsed.Parse(1, size);
    SendAndPrintMessage(interface, std::make_shared<BreakOnRead>(address, size, clear));
    return HANDLED;
  }

  if (parsed.IsCommand("w", "write")) {
    uint32_t address;
    if (!parsed.Parse(0, address)) {
      std::cout << "Missing required address argument." << std::endl;
      PrintUsage();
      return HANDLED;
    }
    int32_t size = 1;
    parsed.Parse(1, size);
    SendAndPrintMessage(interface, std::make_shared<BreakOnWrite>(address, size, clear));
    return HANDLED;
  }

  if (parsed.IsCommand("e", "exec", "execute")) {
    uint32_t address;
    if (!parsed.Parse(0, address)) {
      std::cout << "Missing required address argument." << std::endl;
      PrintUsage();
      return HANDLED;
    }
    int32_t size = 1;
    parsed.Parse(1, size);
    SendAndPrintMessage(interface, std::make_shared<BreakOnExecute>(address, size, clear));
    return HANDLED;
  }

  std::cout << "Invalid mode " << args.front() << std::endl;
  PrintUsage();
    return HANDLED;
}
