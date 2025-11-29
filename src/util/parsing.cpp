#include "parsing.h"

static constexpr char kCommandDelimiter[] = "&&";

namespace {

int GetIntegerBase(const std::string& value) {
  int base = 10;
  if (value.size() > 2 && value[0] == '0' &&
      (value[1] == 'x' || value[1] == 'X')) {
    base = 16;
  }
  return base;
}

}  // namespace

int32_t ParseInt32(const std::vector<uint8_t>& data) {
  std::string value(reinterpret_cast<const char*>(data.data()), data.size());
  return ParseInt32(value);
}

int32_t ParseInt32(const std::vector<char>& data) {
  std::string value(data.data(), data.size());
  return ParseInt32(value);
}

int32_t ParseInt32(const std::string& value) {
  int base = GetIntegerBase(value);
  return static_cast<int32_t>(strtol(value.c_str(), nullptr, base));
}

std::optional<int32_t> MaybeParseInt32(const std::string& value) {
  int base = GetIntegerBase(value);

  char* end = nullptr;
  auto ret = strtol(value.c_str(), &end, base);
  if (end == value.c_str() || *end != '\0') {
    return std::nullopt;
  }

  return static_cast<int32_t>(ret);
}

uint32_t ParseUint32(const std::vector<uint8_t>& data) {
  std::string value(reinterpret_cast<const char*>(data.data()), data.size());
  return ParseUint32(value);
}

uint32_t ParseUint32(const std::vector<char>& data) {
  std::string value(data.data(), data.size());
  return ParseUint32(value);
}

uint32_t ParseUint32(const std::string& value) {
  int base = GetIntegerBase(value);

  return static_cast<uint32_t>(strtoul(value.c_str(), nullptr, base));
}

std::optional<uint32_t> MaybeParseUint32(const std::string& value) {
  int base = GetIntegerBase(value);

  char* end = nullptr;
  auto ret = strtoul(value.c_str(), &end, base);
  if (end == value.c_str() || *end != '\0') {
    return std::nullopt;
  }

  return static_cast<uint32_t>(ret);
}

std::vector<ArgParser::Argument> ArgParser::Tokenize(std::string_view input) {
  std::vector<Argument> args;
  std::string current_token;

  bool in_quote = false;
  int paren_depth = 0;
  bool stripping_outer_parens = false;
  bool token_dirty = false;
  ArgType current_type = ArgType{ArgType::BASIC};

  auto flush_token = [&]() {
    if (token_dirty) {
      args.push_back({current_token, current_type});
      current_token.clear();
      token_dirty = false;
      current_type = ArgType{ArgType::BASIC};
    }
  };

  for (size_t i = 0; i < input.size(); ++i) {
    char c = input[i];

    if (in_quote) {
      if (c == '\\' && i + 1 < input.size() && input[i + 1] == '"') {
        current_token += '"';
        ++i;
      } else if (c == '"') {
        in_quote = false;
      } else {
        current_token += c;
      }
    } else if (paren_depth > 0) {
      if (c == '(') {
        paren_depth++;
        current_token += c;
      } else if (c == ')') {
        paren_depth--;
        if (paren_depth > 0 || !stripping_outer_parens) {
          current_token += c;
        }
      } else {
        current_token += c;
      }
    } else {
      if (c == ' ' || c == '\t') {
        flush_token();
      } else if (c == '"') {
        in_quote = true;
        token_dirty = true;
        current_type = ArgType{ArgType::QUOTED};
      } else if (c == '(') {
        paren_depth = 1;
        token_dirty = true;
        current_type = ArgType{ArgType::PARENTHESIZED};
        if (current_token.empty()) {
          stripping_outer_parens = true;
        } else {
          stripping_outer_parens = false;
          current_token += c;
        }
      } else {
        current_token += c;
        token_dirty = true;
      }
    }
  }
  flush_token();
  return args;
}

bool ArgParser::SplitAt(ArgParser& pre, ArgParser& post,
                        const std::string& delimiter,
                        bool case_sensitive) const {
  auto delimiter_insensitive = boost::algorithm::to_lower_copy(delimiter);

  auto it = std::find_if(arguments.begin(), arguments.end(),
                         [&](const Argument& arg) {
                           if (case_sensitive) {
                             return arg.value == delimiter;
                           }
                           return boost::algorithm::to_lower_copy(arg.value) ==
                                  delimiter_insensitive;
                         });

  std::vector pre_args(arguments.begin(), it);
  pre = ArgParser(this->command, std::move(pre_args));
  if (it == arguments.end()) {
    post = ArgParser("", std::vector<ArgParser::Argument>{});
    return false;
  }

  auto post_start = it + 1;
  if (post_start != arguments.end()) {
    std::string post_cmd = post_start->value;
    std::vector<Argument> post_args;

    if (post_start + 1 != arguments.end()) {
      post_args.assign(post_start + 1, arguments.end());
    }

    post = ArgParser(post_cmd, std::move(post_args));
  } else {
    post = ArgParser("", std::vector<ArgParser::Argument>{});
  }

  return true;
}

std::string ArgParser::Flatten() const {
  std::string out = command;
  for (const auto& arg : arguments) {
    if (!out.empty()) {
      out += " ";
    }
    if (arg.type == ArgType::QUOTED) {
      std::string escaped = arg.value;
      size_t pos = 0;
      while ((pos = escaped.find("\"", pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\\"");
        pos += 2;
      }
      out += "\"" + escaped + "\"";
    } else if (arg.type == ArgType::PARENTHESIZED) {
      out += "(" + arg.value + ")";
    } else {
      out += arg.value;
    }
  }
  return out;
}

namespace command_line_command_tokenizer {
std::vector<std::vector<std::string>> SplitCommands(
    const std::vector<std::string>& additional_commands) {
  std::vector<std::vector<std::string>> ret;

  if (additional_commands.empty()) {
    return ret;
  }

  std::vector<std::string> cmd;
  for (auto& elem : additional_commands) {
    if (elem == kCommandDelimiter) {
      ret.push_back(cmd);
      cmd.clear();
      continue;
    }

    cmd.push_back(elem);
  }
  ret.push_back(cmd);

  return std::move(ret);
}
}  // namespace command_line_command_tokenizer
