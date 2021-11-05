#ifndef XBDM_GDB_BRIDGE_PARSING_H
#define XBDM_GDB_BRIDGE_PARSING_H

#include <boost/algorithm/string.hpp>
#include <cinttypes>
#include <string>
#include <vector>

int32_t ParseInt32(const std::vector<uint8_t> &value);
int32_t ParseInt32(const std::vector<char> &data);
int32_t ParseInt32(const std::string &value);

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

  [[nodiscard]] bool HasCommand() const { return !command.empty(); }
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

  bool Parse(int arg_index, std::string &ret) const {
    if (HasCommand()) {
      ++arg_index;
    }
    if (arg_index >= arguments.size()) {
      return false;
    }
    ret = arguments[arg_index];
    return true;
  }

  std::string command;
  const std::vector<std::string> &arguments;
};

#endif  // XBDM_GDB_BRIDGE_PARSING_H
