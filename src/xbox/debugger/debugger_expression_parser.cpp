#include "debugger_expression_parser.h"

std::expected<uint32_t, std::string> DebuggerExpressionParser::parse(
    const std::string& expr) {
  input_ = expr;
  pos = 0;

  auto result = parse_expression();
  if (!result.has_value()) {
    return result;
  }

  skip_whitespace();
  if (pos < input_.size()) {
    return std::unexpected("Unexpected character at position " +
                           std::to_string(pos));
  }

  return result;
}

std::expected<uint32_t, std::string>
DebuggerExpressionParser::resolve_reg_value(const std::string& reg) const {
  std::string lower = reg;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  auto resolve =
      [](const std::optional<int32_t>& opt,
         const std::string& name) -> std::expected<uint32_t, std::string> {
    if (!opt.has_value()) {
      return std::unexpected("Register " + name + " not available in context");
    }
    return static_cast<uint32_t>(opt.value());
  };

  auto resolve16 =
      [&resolve](
          const std::optional<int32_t>& opt,
          const std::string& name) -> std::expected<uint32_t, std::string> {
    auto r = resolve(opt, name);
    return r.has_value() ? r.value() & 0xFFFF : r;
  };

  auto resolve8 =
      [&resolve](const std::optional<int32_t>& opt, const std::string& name,
                 uint32_t shift = 0) -> std::expected<uint32_t, std::string> {
    auto r = resolve(opt, name);
    return r.has_value() ? (r.value() >> shift) & 0xFF : r;
  };

  // 32-bit registers
  if (lower == "eax") {
    return resolve(context_.eax, "eax");
  } else if (lower == "ebx") {
    return resolve(context_.ebx, "ebx");
  } else if (lower == "ecx") {
    return resolve(context_.ecx, "ecx");
  } else if (lower == "edx") {
    return resolve(context_.edx, "edx");
  } else if (lower == "esi") {
    return resolve(context_.esi, "esi");
  } else if (lower == "edi") {
    return resolve(context_.edi, "edi");
  } else if (lower == "ebp") {
    return resolve(context_.ebp, "ebp");
  } else if (lower == "esp") {
    return resolve(context_.esp, "esp");
  } else if (lower == "eip") {
    return resolve(context_.eip, "eip");
  } else if (lower == "eflags") {
    return resolve(context_.eflags, "eflags");
  } else if (lower == "ax") {
    return resolve16(context_.eax, "ax");
  } else if (lower == "bx") {
    return resolve16(context_.ebx, "bx");
  } else if (lower == "cx") {
    return resolve16(context_.ecx, "cx");
  } else if (lower == "dx") {
    return resolve16(context_.edx, "dx");
  } else if (lower == "si") {
    return resolve16(context_.esi, "si");
  } else if (lower == "di") {
    return resolve16(context_.edi, "di");
  } else if (lower == "ah") {
    return resolve8(context_.eax, "eax", 8);
  } else if (lower == "bh") {
    return resolve8(context_.ebx, "ebx", 8);
  } else if (lower == "ch") {
    return resolve8(context_.ecx, "ecx", 8);
  } else if (lower == "dh") {
    return resolve8(context_.edx, "edx", 8);
  } else if (lower == "al") {
    return resolve8(context_.eax, "ebx");
  } else if (lower == "bl") {
    return resolve8(context_.ebx, "ebx");
  } else if (lower == "cl") {
    return resolve8(context_.ecx, "ecx");
  } else if (lower == "dl") {
    return resolve8(context_.edx, "edx");
  }

  return std::unexpected("Unknown register: " + reg);
}

std::expected<std::string, std::string>
DebuggerExpressionParser::parse_register() {
  skip_whitespace();

  if (peek() != '$') {
    return std::unexpected("Expected '$' for register");
  }
  consume();

  std::string reg_name;
  while (pos < input_.size() && std::isalpha(peek())) {
    reg_name += consume();
  }

  if (reg_name.empty()) {
    return std::unexpected("Empty register name");
  }

  return reg_name;
}

std::expected<uint32_t, std::string> DebuggerExpressionParser::parse_number() {
  skip_whitespace();

  if (peek() == '0' && pos + 1 < input_.size() &&
      (input_[pos + 1] == 'x' || input_[pos + 1] == 'X')) {
    pos += 2;

    uint32_t value = 0;
    bool has_digits = false;
    while (pos < input_.size() && std::isxdigit(peek())) {
      char c = consume();
      has_digits = true;
      value =
          value * 16 + (std::isdigit(c) ? c - '0' : std::tolower(c) - 'a' + 10);
    }

    if (!has_digits) {
      return std::unexpected("Invalid hexadecimal number");
    }
    return value;
  }

  if (std::isdigit(peek())) {
    uint32_t value = 0;
    while (pos < input_.size() && std::isdigit(peek())) {
      value = value * 10 + (consume() - '0');
    }
    return value;
  }

  return std::unexpected("Expected number at position " + std::to_string(pos));
}

std::expected<uint32_t, std::string> DebuggerExpressionParser::parse_factor() {
  skip_whitespace();

  if (peek() == '(') {
    consume();
    auto result = parse_expression();
    if (!result.has_value()) {
      return result;
    }
    skip_whitespace();
    if (peek() != ')') {
      return std::unexpected("Expected ')'");
    }
    consume();
    return result;
  }

  if (peek() == '$') {
    auto reg = parse_register();
    if (!reg.has_value()) {
      return std::unexpected(reg.error());
    }
    return resolve_reg_value(reg.value());
  }

  return parse_number();
}

std::expected<uint32_t, std::string> DebuggerExpressionParser::parse_term() {
  auto result = parse_factor();
  if (!result.has_value()) {
    return result;
  }

  uint32_t value = result.value();
  skip_whitespace();

  while (peek() == '*') {
    consume();
    auto factor_result = parse_factor();
    if (!factor_result.has_value()) {
      return factor_result;
    }
    value *= factor_result.value();
    skip_whitespace();
  }

  return value;
}

std::expected<uint32_t, std::string>
DebuggerExpressionParser::parse_expression() {
  auto result = parse_term();
  if (!result.has_value()) {
    return result;
  }

  uint32_t value = result.value();
  skip_whitespace();

  while (peek() == '+' || peek() == '-') {
    char op = consume();
    auto term_result = parse_term();
    if (!term_result.has_value()) {
      return term_result;
    }

    if (op == '+') {
      value += term_result.value();
    } else {
      value -= term_result.value();
    }
    skip_whitespace();
  }

  return value;
}
