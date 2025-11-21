#include "debugger_expression_parser.h"

std::expected<uint32_t, std::string> DebuggerExpressionParser::Parse(
    const std::string& expr) {
  ParseState state(context_, thread_id_, memory_reader_);
  return state.Parse(expr);
}

std::expected<uint32_t, std::string>
DebuggerExpressionParser::ParseState::Parse(const std::string& expr) {
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
DebuggerExpressionParser::ParseState::ResolveRegisterValue(
    const std::string& reg) const {
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
  }
  if (lower == "ebx") {
    return resolve(context_.ebx, "ebx");
  }
  if (lower == "ecx") {
    return resolve(context_.ecx, "ecx");
  }
  if (lower == "edx") {
    return resolve(context_.edx, "edx");
  }
  if (lower == "esi") {
    return resolve(context_.esi, "esi");
  }
  if (lower == "edi") {
    return resolve(context_.edi, "edi");
  }
  if (lower == "ebp") {
    return resolve(context_.ebp, "ebp");
  }
  if (lower == "esp") {
    return resolve(context_.esp, "esp");
  }
  if (lower == "eip") {
    return resolve(context_.eip, "eip");
  }
  if (lower == "eflags") {
    return resolve(context_.eflags, "eflags");
  }
  if (lower == "ax") {
    return resolve16(context_.eax, "ax");
  }
  if (lower == "bx") {
    return resolve16(context_.ebx, "bx");
  }
  if (lower == "cx") {
    return resolve16(context_.ecx, "cx");
  }
  if (lower == "dx") {
    return resolve16(context_.edx, "dx");
  }
  if (lower == "si") {
    return resolve16(context_.esi, "si");
  }
  if (lower == "di") {
    return resolve16(context_.edi, "di");
  }
  if (lower == "ah") {
    return resolve8(context_.eax, "eax", 8);
  }
  if (lower == "bh") {
    return resolve8(context_.ebx, "ebx", 8);
  }
  if (lower == "ch") {
    return resolve8(context_.ecx, "ecx", 8);
  }
  if (lower == "dh") {
    return resolve8(context_.edx, "edx", 8);
  }
  if (lower == "al") {
    return resolve8(context_.eax, "ebx");
  }
  if (lower == "bl") {
    return resolve8(context_.ebx, "ebx");
  }
  if (lower == "cl") {
    return resolve8(context_.ecx, "ecx");
  }
  if (lower == "dl") {
    return resolve8(context_.edx, "edx");
  }

  return std::unexpected("Unknown register: " + reg);
}

std::expected<std::string, std::string>
DebuggerExpressionParser::ParseState::ParseRegister() {
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

std::expected<uint32_t, std::string>
DebuggerExpressionParser::ParseState::ParseNumber() {
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

std::expected<uint32_t, std::string>
DebuggerExpressionParser::ParseState::ParseMemory() {
  if (Peek() != '@') {
    return std::unexpected("Expected '@' for memory access");
  }
  Consume();

  uint32_t size = 4;
  std::expected<uint32_t, std::string> address_result;

  if (Peek() == '(') {
    Consume();
    address_result = ParseExpression();
    if (!address_result.has_value()) {
      return address_result;
    }

    SkipWhitespace();
    if (Peek() == ',') {
      Consume();
      auto size_result = ParseExpression();
      if (!size_result.has_value()) {
        return size_result;
      }
      size = size_result.value();
    }

    if (Peek() != ')') {
      return std::unexpected("Expected ')'");
    }
    Consume();
  } else {
    address_result = ParseFactor();
  }

  if (!address_result.has_value()) {
    return address_result;
  }

  if (!memory_reader_) {
    return std::unexpected("Memory reader not available");
  }

  if (size > 4) {
    return std::unexpected("Memory read size too large (max 4 bytes)");
  }

  auto data_result = memory_reader_(address_result.value(), size);
  if (!data_result.has_value()) {
    return std::unexpected(data_result.error());
  }

  const auto& data = data_result.value();
  if (data.size() != size) {
    return std::unexpected("Failed to read requested memory size");
  }

  uint64_t value = 0;
  for (size_t i = 0; i < data.size(); ++i) {
    value |= static_cast<uint64_t>(data[i]) << (i * 8);
  }

  return static_cast<uint32_t>(value);
}

std::expected<uint32_t, std::string>
DebuggerExpressionParser::ParseState::ParseFactor() {
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

  if (Peek() == '@') {
    return ParseMemory();
  }

  if (input_.substr(pos, 3) == "tid") {
    pos += 3;
    if (thread_id_ == -1) {
      return std::unexpected("Thread ID not available in this context");
    }
    return thread_id_;
  }

  return ParseNumber();
}

std::expected<uint32_t, std::string>
DebuggerExpressionParser::ParseState::ParseTerm() {
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
DebuggerExpressionParser::ParseState::ParseAdditive() {
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

std::expected<uint32_t, std::string>
DebuggerExpressionParser::ParseState::ParseRelational() {
  auto result = ParseAdditive();
  if (!result.has_value()) {
    return result;
  }

  uint32_t value = result.value();
  SkipWhitespace();

  while (true) {
    char c = Peek();
    if (c != '<' && c != '>') {
      break;
    }

    std::string op;
    op += c;
    if (pos + 1 < input_.size() && input_[pos + 1] == '=') {
      op += '=';
    }

    if (op.length() == 2) {
      Consume();
      Consume();
    } else {
      Consume();
    }

    auto right = ParseAdditive();
    if (!right.has_value()) {
      return right;
    }

    if (op == "<") {
      value = (value < right.value());
    } else if (op == ">") {
      value = (value > right.value());
    } else if (op == "<=") {
      value = (value <= right.value());
    } else if (op == ">=") {
      value = (value >= right.value());
    }
    SkipWhitespace();
  }

  return value;
}

std::expected<uint32_t, std::string>
DebuggerExpressionParser::ParseState::ParseEquality() {
  auto result = ParseRelational();
  if (!result.has_value()) {
    return result;
  }

  uint32_t value = result.value();
  SkipWhitespace();

  while (true) {
    char c = Peek();
    if (c != '=' && c != '!') {
      break;
    }

    std::string op;
    op += c;
    if (pos + 1 < input_.size() && input_[pos + 1] == '=') {
      op += '=';
    }

    if (op != "==" && op != "!=") {
      break;
    }

    Consume();
    Consume();

    auto right = ParseRelational();
    if (!right.has_value()) {
      return right;
    }

    if (op == "==") {
      value = (value == right.value());
    } else if (op == "!=") {
      value = (value != right.value());
    }
    SkipWhitespace();
  }
  return value;
}

std::expected<uint32_t, std::string>
DebuggerExpressionParser::ParseState::ParseLogicalAnd() {
  auto result = ParseEquality();
  if (!result.has_value()) {
    return result;
  }

  uint32_t value = result.value();
  SkipWhitespace();

  while (true) {
    bool match = false;
    if (Peek() == '&' && pos + 1 < input_.size() && input_[pos + 1] == '&') {
      match = true;
      pos += 2;
    } else if (input_.size() - pos >= 3) {
      std::string sub = input_.substr(pos, 3);
      if (boost::algorithm::iequals(sub, "AND")) {
        match = true;
        pos += 3;
      }
    }

    if (!match) {
      break;
    }

    auto right = ParseEquality();
    if (!right.has_value()) {
      return right;
    }

    value = (value != 0) && (right.value() != 0);
    SkipWhitespace();
  }
  return value;
}

std::expected<uint32_t, std::string>
DebuggerExpressionParser::ParseState::ParseLogicalOr() {
  auto result = ParseLogicalAnd();
  if (!result.has_value()) {
    return result;
  }

  uint32_t value = result.value();
  SkipWhitespace();

  while (true) {
    bool match = false;
    if (Peek() == '|' && pos + 1 < input_.size() && input_[pos + 1] == '|') {
      match = true;
      pos += 2;
    } else if (input_.size() - pos >= 2) {
      std::string sub = input_.substr(pos, 2);
      if (boost::algorithm::iequals(sub, "OR")) {
        match = true;
        pos += 2;
      }
    }

    if (!match) {
      break;
    }

    auto right = ParseLogicalAnd();
    if (!right.has_value()) {
      return right;
    }

    value = (value != 0) || (right.value() != 0);
    SkipWhitespace();
  }
  return value;
}

std::expected<uint32_t, std::string>
DebuggerExpressionParser::ParseState::ParseExpression() {
  return ParseLogicalOr();
}
