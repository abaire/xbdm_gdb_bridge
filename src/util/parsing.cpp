#include "parsing.h"

static constexpr char kCommandDelimiter[] = "&&";

int32_t ParseInt32(const std::vector<uint8_t>& data) {
  std::string value(reinterpret_cast<const char*>(data.data()), data.size());
  return ParseInt32(value);
}

int32_t ParseInt32(const std::vector<char>& data) {
  std::string value(data.data(), data.size());
  return ParseInt32(value);
}

int32_t ParseInt32(const std::string& value) {
  int base = 10;
  if (value.size() > 2 && (value[1] == 'x' || value[1] == 'X')) {
    base = 16;
  }

  return static_cast<int32_t>(strtol(value.c_str(), nullptr, base));
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
  int base = 10;
  if (value.size() > 2 && (value[1] == 'x' || value[1] == 'X')) {
    base = 16;
  }

  return static_cast<uint32_t>(strtoul(value.c_str(), nullptr, base));
}

std::vector<ArgParser::Argument> ArgParser::Tokenize(std::string_view input) {
  std::vector<Argument> args;
  std::string current_token;

  bool in_quote = false;
  int paren_depth = 0;
  bool token_dirty = false;
  ArgType current_type = ArgType::BASIC;

  auto flush_token = [&]() {
    if (token_dirty) {
      args.push_back({current_token, current_type});
      current_token.clear();
      token_dirty = false;
      current_type = ArgType::BASIC;
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
        if (paren_depth > 0) current_token += c;
      } else {
        current_token += c;
      }
    } else {
      if (c == ' ' || c == '\t') {
        flush_token();
      } else if (c == '"') {
        in_quote = true;
        token_dirty = true;
        current_type = ArgType::QUOTED;
      } else if (c == '(') {
        paren_depth = 1;
        token_dirty = true;
        current_type = ArgType::PARENTHESIZED;
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
  auto it = std::find_if(arguments.begin(), arguments.end(),
                         [&](const Argument& arg) {
                           if (case_sensitive) {
                             return arg.value == delimiter;
                           }
                           return boost::algorithm::to_lower_copy(arg.value) ==
                                  boost::algorithm::to_lower_copy(delimiter);
                         });

  if (it == arguments.end()) {
    return false;
  }

  std::vector pre_args(arguments.begin(), it);
  pre = ArgParser(this->command, std::move(pre_args));

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
