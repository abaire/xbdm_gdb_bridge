#include "commands.h"

#include <boost/algorithm/string.hpp>
#include <string>
#include <vector>

#include "util/parsing.h"

static void SendAndPrintMessage(
    XBOXInterface &interface,
    const std::shared_ptr<RDCPProcessedRequest> &request) {
  interface.SendCommandSync(request);
  std::cout << *request << std::endl;
}

struct ArgParser {
  explicit ArgParser(const std::vector<std::string> &args,
                     bool extract_command = false)
      : arguments(args) {
    if (args.empty()) {
      return;
    }

    if (extract_command) {
      command = args.front();
      boost::algorithm::to_lower(command);
    }
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

  template <typename T>
  [[nodiscard]] bool IsCommand(const T &cmd) const {
    return (command == cmd);
  }

  template <typename T, typename... Ts, typename = are_same_type<T, Ts...>>
  [[nodiscard]] bool IsCommand(const T &cmd, const Ts &...rest) const {
    if (command == cmd) {
      return true;
    }

    return IsCommand(rest...);
  }

  template <typename T>
  [[nodiscard]] bool ArgExists(const T &arg) const {
    std::string test(arg);
    return std::find_if(arguments.begin(), arguments.end(),
                        [&test](const auto &v) {
                          return test == boost::algorithm::to_lower_copy(v);
                        }) != arguments.end();
  }

  template <typename T, typename... Ts, typename = are_same_type<T, Ts...>>
  [[nodiscard]] bool ArgExists(const T &arg, const Ts &...rest) const {
    if (ArgExists(arg)) {
      return true;
    }

    return ArgExists(rest...);
  }

  template <
      typename T,
      std::enable_if_t<std::is_integral<T>::value && sizeof(T) == 4, int> = 0>
  bool Parse(int arg_index, T &ret) const {
    if (HasCommand()) {
      ++arg_index;
    }
    if (arg_index >= arguments.size()) {
      return false;
    }
    ret = ParseInt32(arguments[arg_index]);
    return true;
  }

  bool Parse(int arg_index, bool &ret) const {
    if (HasCommand()) {
      ++arg_index;
    }
    if (arg_index >= arguments.size()) {
      return false;
    }

    std::string param = arguments[arg_index];
    boost::algorithm::to_lower(param);

    ret = param == "t" || param == "true" || param == "y" || param == "yes" ||
          param == "on" || param == "1";
    return true;
  }

  std::string command;
  const std::vector<std::string> &arguments;
};

Command::Result CommandBreak::operator()(XBOXInterface &interface,
                                         const std::vector<std::string> &args) {
  ArgParser parser(args, true);
  if (!parser.HasCommand()) {
    PrintUsage();
    return HANDLED;
  }

  if (parser.IsCommand("start")) {
    SendAndPrintMessage(interface, std::make_shared<BreakAtStart>());
    return HANDLED;
  }

  if (parser.IsCommand("clearall")) {
    SendAndPrintMessage(interface, std::make_shared<BreakClearAll>());
    return HANDLED;
  }

  bool clear = parser.ShiftPrefixModifier('-');

  if (parser.IsCommand("a", "addr", "address")) {
    uint32_t address;
    if (!parser.Parse(0, address)) {
      std::cout << "Missing required address argument." << std::endl;
      PrintUsage();
      return HANDLED;
    }
    SendAndPrintMessage(interface,
                        std::make_shared<BreakAddress>(address, clear));
    return HANDLED;
  }

  if (parser.IsCommand("r", "read")) {
    uint32_t address;
    if (!parser.Parse(0, address)) {
      std::cout << "Missing required address argument." << std::endl;
      PrintUsage();
      return HANDLED;
    }
    int32_t size = 1;
    parser.Parse(1, size);
    SendAndPrintMessage(interface,
                        std::make_shared<BreakOnRead>(address, size, clear));
    return HANDLED;
  }

  if (parser.IsCommand("w", "write")) {
    uint32_t address;
    if (!parser.Parse(0, address)) {
      std::cout << "Missing required address argument." << std::endl;
      PrintUsage();
      return HANDLED;
    }
    int32_t size = 1;
    parser.Parse(1, size);
    SendAndPrintMessage(interface,
                        std::make_shared<BreakOnWrite>(address, size, clear));
    return HANDLED;
  }

  if (parser.IsCommand("e", "exec", "execute")) {
    uint32_t address;
    if (!parser.Parse(0, address)) {
      std::cout << "Missing required address argument." << std::endl;
      PrintUsage();
      return HANDLED;
    }
    int32_t size = 1;
    parser.Parse(1, size);
    SendAndPrintMessage(interface,
                        std::make_shared<BreakOnExecute>(address, size, clear));
    return HANDLED;
  }

  std::cout << "Invalid mode " << args.front() << std::endl;
  PrintUsage();
  return HANDLED;
}

Command::Result CommandBye::operator()(XBOXInterface &interface,
                                       const std::vector<std::string> &) {
  SendAndPrintMessage(interface, std::make_shared<Bye>());
  return HANDLED;
}

Command::Result CommandContinue::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    std::cout << "Missing required thread_id argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  bool exception = false;
  parser.Parse(1, exception);

  SendAndPrintMessage(interface,
                      std::make_shared<Continue>(thread_id, exception));
  return HANDLED;
}

Command::Result CommandDebugOptions::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  if (args.empty()) {
    SendAndPrintMessage(interface, std::make_shared<GetDebugOptions>());
    return HANDLED;
  }

  ArgParser parser(args);
  bool enable_crashdump = parser.ArgExists("c", "crashdump");
  bool enable_dcptrace = parser.ArgExists("d", "dpctrace");
  SendAndPrintMessage(interface, std::make_shared<SetDebugOptions>(
                                     enable_crashdump, enable_dcptrace));
  return HANDLED;
}
