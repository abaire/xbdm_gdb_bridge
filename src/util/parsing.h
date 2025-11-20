#ifndef XBDM_GDB_BRIDGE_PARSING_H
#define XBDM_GDB_BRIDGE_PARSING_H

#include <boost/algorithm/string.hpp>
#include <cinttypes>
#include <expected>
#include <string>
#include <vector>

class DebuggerExpressionParser;
int32_t ParseInt32(const std::vector<uint8_t>& value);
int32_t ParseInt32(const std::vector<char>& data);
int32_t ParseInt32(const std::string& value);
uint32_t ParseUint32(const std::vector<uint8_t>& value);
uint32_t ParseUint32(const std::vector<char>& data);
uint32_t ParseUint32(const std::string& value);

namespace command_line_command_tokenizer {

//! Splits the given vector of strings into sub-vectors delimited by "&&"
// kCommandDelimiter
std::vector<std::vector<std::string>> SplitCommands(
    const std::vector<std::string>& additional_commands);

}  // namespace command_line_command_tokenizer

template <typename T>
bool MaybeParseHexInt(T& ret, const std::vector<uint8_t>& data,
                      size_t offset = 0) {
  auto data_start = data.data() + offset;
  auto data_end = data_start + (data.size() - offset);
  std::string to_parse(data_start, data_end);

  char* end = nullptr;

  ret = static_cast<T>(strtoull(to_parse.c_str(), &end, 16));
  return end != to_parse.c_str();
}

template <typename T>
bool MaybeParseHexInt(T& ret, const std::string& data) {
  char* end = nullptr;
  ret = static_cast<T>(strtoull(data.c_str(), &end, 16));
  return end != data.c_str();
}

/**
 * Interface for expression parsing.
 * Concrete implementations (like DebuggerExpressionParser) must implement
 * Parse.
 */
struct ExpressionParser {
  virtual ~ExpressionParser() = default;

  /**
   * Attempts to evaluate the given string as an expression.
   * @param expr - The expression to evaluate
   * @return The final value or a string indicating an error.
   */
  virtual std::expected<uint32_t, std::string> Parse(
      const std::string& expr) = 0;
};

/**
 * Parses shell command strings.
 */
struct ArgParser {
  struct ArgType {
    enum Enum { NOT_FOUND = 0, BASIC, PARENTHESIZED, QUOTED, SYNTAX_ERROR };

    Enum val;
    std::string message;  // Stores error details if val == SYNTAX_ERROR

    ArgType(Enum v = NOT_FOUND) : val(v) {}
    ArgType(Enum v, std::string msg) : val(v), message(std::move(msg)) {}

    explicit operator bool() const {
      return val != NOT_FOUND && val != SYNTAX_ERROR;
    }

    bool operator==(Enum other) const { return val == other; }
    bool operator!=(Enum other) const { return val != other; }
    bool operator==(const ArgType& other) const { return val == other.val; }

    explicit operator Enum() const { return val; }
  };

  struct Argument {
    std::string value;
    ArgType type;
  };

 public:
  ArgParser() = default;

  explicit ArgParser(std::string_view raw_line) {
    std::vector<Argument> full_tokens = Tokenize(raw_line);

    if (!full_tokens.empty()) {
      command = full_tokens[0].value;
      boost::algorithm::to_lower(command);

      if (full_tokens.size() > 1) {
        arguments.assign(full_tokens.begin() + 1, full_tokens.end());
      }
    }
  }

  ArgParser(std::string_view cmd, const std::vector<std::string>& args) {
    command = cmd;
    boost::algorithm::to_lower(command);

    arguments.reserve(args.size());
    for (const auto& s : args) {
      arguments.emplace_back(s, ArgType::BASIC);
    }
  }

  ArgParser(std::string_view cmd, std::vector<Argument>&& args)
      : command(cmd), arguments(std::move(args)) {
    boost::algorithm::to_lower(command);
  }

  [[nodiscard]] std::optional<ArgParser> ExtractSubcommand() const {
    if (arguments.empty()) {
      return std::nullopt;
    }

    std::vector<Argument> sub_args;
    if (arguments.size() > 1) {
      sub_args.assign(arguments.begin() + 1, arguments.end());
    }

    return ArgParser(arguments[0].value, std::move(sub_args));
  }

  /**
   * Splits this ArgParser around the first instance of the given string.
   * @param pre ArgParser to be populated with arguments before [delimiter]
   * @param post ArgParser to be populated with arguments after [delimiter]
   * @param delimiter The argument string to split around
   * @param case_sensitive Whether the delimiter comparison should be case
   *                       sensitive or not
   * @return true if the delimiter was found
   */
  bool SplitAt(ArgParser& pre, ArgParser& post, const std::string& delimiter,
               bool case_sensitive = false) const;

  [[nodiscard]] bool HasCommand() const { return !command.empty(); }

  [[nodiscard]] bool ShiftPrefixModifier(char modifier) {
    if (command.empty() || command[0] != modifier) {
      return false;
    }
    command.erase(0, 1);
    return true;
  }

  /**
   * Generates a minimal command line string that will parse to this instance.
   * @return Recomposed command line
   */
  std::string Flatten() const;

  template <typename T, typename... Ts>
  using are_same_type = std::conjunction<std::is_same<T, Ts>...>;

  template <typename T>
  [[nodiscard]] bool IsCommand(const T& cmd) const {
    return (command == cmd);
  }

  template <typename T, typename... Ts, typename = are_same_type<T, Ts...>>
  [[nodiscard]] bool IsCommand(const T& cmd, const Ts&... rest) const {
    if (command == cmd) {
      return true;
    }

    return IsCommand(rest...);
  }

  template <typename T>
  [[nodiscard]] bool ArgExists(const T& arg) const {
    std::string test(arg);
    return std::find_if(arguments.begin(), arguments.end(),
                        [&test](const Argument& arg_struct) {
                          return test == boost::algorithm::to_lower_copy(
                                             arg_struct.value);
                        }) != arguments.end();
  }

  template <typename T, typename... Ts, typename = are_same_type<T, Ts...>>
  [[nodiscard]] bool ArgExists(const T& arg, const Ts&... rest) const {
    if (ArgExists(arg)) return true;
    return ArgExists(rest...);
  }

  template <
      typename T,
      std::enable_if_t<std::is_integral<T>::value && sizeof(T) == 4, int> = 0>
  ArgType Parse(int arg_index, T& ret) const {
    if (arg_index < 0 || arg_index >= static_cast<int>(arguments.size())) {
      return ArgType::NOT_FOUND;
    }

    ret = ParseInt32(arguments[arg_index].value);
    return arguments[arg_index].type;
  }

  ArgType Parse(int arg_index, bool& ret) const {
    if (arg_index < 0 || arg_index >= static_cast<int>(arguments.size())) {
      return ArgType::NOT_FOUND;
    }

    std::string param = arguments[arg_index].value;
    boost::algorithm::to_lower(param);

    ret = param == "t" || param == "true" || param == "y" || param == "yes" ||
          param == "on" || param == "1";

    return arguments[arg_index].type;
  }

  ArgType Parse(int arg_index, std::string& ret) const {
    if (arg_index < 0 || arg_index >= static_cast<int>(arguments.size())) {
      return ArgType::NOT_FOUND;
    }

    ret = arguments[arg_index].value;
    return arguments[arg_index].type;
  }

  /**
   * Parses an argument into a uint32_t.
   * If the argument is PARENTHESIZED, it delegates to the provided
   * ExpressionParser.
   */
  template <
      typename T,
      std::enable_if_t<std::is_integral<T>::value && sizeof(T) == 4, int> = 0>
  ArgType Parse(int arg_index, T& ret, ExpressionParser& expr_parser) const {
    if (arg_index < 0 || arg_index >= static_cast<int>(arguments.size())) {
      return ArgType::NOT_FOUND;
    }

    const auto& arg = arguments[arg_index];

    if (arg.type == ArgType::PARENTHESIZED) {
      auto result = expr_parser.Parse(arg.value);

      if (result) {
        ret = *result;
        return arg.type;
      }
      return {ArgType::SYNTAX_ERROR, result.error()};
    }

    ret = ParseInt32(arg.value);
    return arg.type;
  }

  template <
      typename T,
      std::enable_if_t<std::is_integral<T>::value && sizeof(T) == 4, int> = 0>
  ArgType Parse(int arg_index, T& ret,
                const std::shared_ptr<ExpressionParser>& expr_parser) const {
    if (expr_parser) {
      return Parse(arg_index, ret, *expr_parser);
    }
    return Parse(arg_index, ret);
  }

  /**
   * Iterator over parsed arguments.
   */
  class const_iterator {
   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = std::string;
    using difference_type = std::ptrdiff_t;
    using pointer = const std::string*;
    using reference = const std::string&;

    explicit const_iterator(std::vector<Argument>::const_iterator it)
        : current_(it) {}

    reference operator*() const { return current_->value; }
    pointer operator->() const { return &current_->value; }

    const_iterator& operator++() {
      ++current_;
      return *this;
    }
    const_iterator operator++(int) {
      const_iterator tmp = *this;
      ++current_;
      return tmp;
    }
    const_iterator& operator--() {
      --current_;
      return *this;
    }
    const_iterator operator--(int) {
      const_iterator tmp = *this;
      --current_;
      return tmp;
    }
    bool operator==(const const_iterator& other) const {
      return current_ == other.current_;
    }
    bool operator!=(const const_iterator& other) const {
      return current_ != other.current_;
    }
    bool operator<(const const_iterator& other) const {
      return current_ < other.current_;
    }
    const_iterator operator+(difference_type n) const {
      return const_iterator(current_ + n);
    }
    const_iterator operator-(difference_type n) const {
      return const_iterator(current_ - n);
    }
    difference_type operator-(const const_iterator& other) const {
      return current_ - other.current_;
    }
    const_iterator& operator+=(difference_type n) {
      current_ += n;
      return *this;
    }
    const_iterator& operator-=(difference_type n) {
      current_ -= n;
      return *this;
    }
    reference operator[](difference_type n) const { return current_[n].value; }

   private:
    std::vector<Argument>::const_iterator current_;
  };

  [[nodiscard]] const std::string& front() const {
    return arguments.front().value;
  }

  [[nodiscard]] const std::string& back() const {
    return arguments.back().value;
  }

  [[nodiscard]] const_iterator begin() const {
    return const_iterator(arguments.begin());
  }
  [[nodiscard]] const_iterator end() const {
    return const_iterator(arguments.end());
  }

  [[nodiscard]] size_t size() const { return arguments.size(); }
  [[nodiscard]] bool empty() const { return arguments.empty(); }

  std::string command;
  std::vector<Argument> arguments;

 private:
  static std::vector<Argument> Tokenize(std::string_view input);
};

#endif  // XBDM_GDB_BRIDGE_PARSING_H
