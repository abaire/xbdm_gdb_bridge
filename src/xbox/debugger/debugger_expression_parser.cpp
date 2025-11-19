#include "debugger_expression_parser.h"

std::expected<uint32_t, std::string> DebuggerExpressionParser::Parse(
    const std::string& expr) {
  input_ = expr;
  pos = 0;

  auto result = ParseExpression();
  if (!result.has_value()) {
    return result;
  }

  SkipWhitespace();
  if (pos < input_.size()) {
    return std::unexpected("Unexpected character at position " +
                           std::to_string(pos));
  }

  return result;
}

std::expected<uint32_t, std::string>
DebuggerExpressionParser::ResolveRegisterValue(const std::string& reg) const {
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
DebuggerExpressionParser::ParseRegister() {
  SkipWhitespace();

  if (Peek() != '$') {
    return std::unexpected("Expected '$' for register");
  }
  Consume();

  std::string reg_name;
  while (pos < input_.size() && std::isalpha(Peek())) {
    reg_name += Consume();
  }

  if (reg_name.empty()) {
    return std::unexpected("Empty register name");
  }

  return reg_name;
}

std::expected<uint32_t, std::string> DebuggerExpressionParser::ParseNumber() {
  SkipWhitespace();

  if (Peek() == '0' && pos + 1 < input_.size() &&
      (input_[pos + 1] == 'x' || input_[pos + 1] == 'X')) {
    pos += 2;

    uint32_t value = 0;
    bool has_digits = false;
    while (pos < input_.size() && std::isxdigit(Peek())) {
      char c = Consume();
      has_digits = true;
      value =
          value * 16 + (std::isdigit(c) ? c - '0' : std::tolower(c) - 'a' + 10);
    }

    if (!has_digits) {
      return std::unexpected("Invalid hexadecimal number");
    }
    return value;
  }

  if (std::isdigit(Peek())) {
    uint32_t value = 0;
    while (pos < input_.size() && std::isdigit(Peek())) {
      value = value * 10 + (Consume() - '0');
    }
    return value;
  }

  return std::unexpected("Expected number at position " + std::to_string(pos));
}

std::expected<uint32_t, std::string> DebuggerExpressionParser::ParseFactor() {
  SkipWhitespace();

  if (Peek() == '(') {
    Consume();
    auto result = ParseExpression();
    if (!result.has_value()) {
      return result;
    }
    SkipWhitespace();
    if (Peek() != ')') {
      return std::unexpected("Expected ')'");
    }
    Consume();
    return result;
  }

  if (Peek() == '$') {
    auto reg = ParseRegister();
    if (!reg.has_value()) {
      return std::unexpected(reg.error());
    }
    return ResolveRegisterValue(reg.value());
  }

  return ParseNumber();
}

std::expected<uint32_t, std::string> DebuggerExpressionParser::ParseTerm() {
  auto result = ParseFactor();
  if (!result.has_value()) {
    return result;
  }

  uint32_t value = result.value();
  SkipWhitespace();

  while (Peek() == '*') {
    Consume();
    auto factor_result = ParseFactor();
    if (!factor_result.has_value()) {
      return factor_result;
    }
    value *= factor_result.value();
    SkipWhitespace();
  }

  return value;
}

std::expected<uint32_t, std::string>
DebuggerExpressionParser::ParseExpression() {
  auto result = ParseTerm();
  if (!result.has_value()) {
    return result;
  }

  uint32_t value = result.value();
  SkipWhitespace();

  while (Peek() == '+' || Peek() == '-') {
    char op = Consume();
    auto term_result = ParseTerm();
    if (!term_result.has_value()) {
      return term_result;
    }

    if (op == '+') {
      value += term_result.value();
    } else {
      value -= term_result.value();
    }
    SkipWhitespace();
  }

  return value;
}
