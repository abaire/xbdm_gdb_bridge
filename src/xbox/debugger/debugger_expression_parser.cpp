#include "debugger_expression_parser.h"

#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>
#include <cctype>
#include <cmath>

Token DebuggerExpressionParser::Peek() const {
  if (pos_ >= tokens_.size()) {
    return {TokenType::END_OF_FILE, ""};
  }
  return tokens_[pos_];
}

Token DebuggerExpressionParser::Consume() {
  if (pos_ >= tokens_.size()) {
    return {TokenType::END_OF_FILE, ""};
  }
  return tokens_[pos_++];
}

constexpr Precedence DebuggerExpressionParser::GetPrecedence(TokenType type) {
  switch (type) {
    case TokenType::OR:
      return Precedence::LOGICAL_OR;

    case TokenType::AND:
      return Precedence::LOGICAL_AND;

    case TokenType::EQ:
    case TokenType::NOT_EQ:
      return Precedence::EQUALITY;

    case TokenType::LT:
    case TokenType::GT:
    case TokenType::LTE:
    case TokenType::GTE:
      return Precedence::RELATIONAL;

    case TokenType::PLUS:
    case TokenType::MINUS:
      return Precedence::SUM;

    case TokenType::ASTERISK:
      return Precedence::PRODUCT;

    case TokenType::LPAREN:
      return Precedence::CALL;

    default:
      return Precedence::LOWEST;
  }
}

std::expected<uint32_t, std::string> DebuggerExpressionParser::Parse(
    const std::string& expr) {
  auto token_res = Tokenize(expr);
  if (!token_res) {
    return std::unexpected(token_res.error());
  }

  if (tokens_.empty() || tokens_[0].type == TokenType::END_OF_FILE) {
    return std::unexpected("Empty expression");
  }

  auto result = ParseExpression(Precedence::LOWEST);
  if (!result) {
    return result;
  }

  if (Peek().type != TokenType::END_OF_FILE) {
    return std::unexpected("Unexpected character at position " +
                           std::to_string(Peek().start_pos));
  }

  return result;
}

std::expected<void, std::string> DebuggerExpressionParser::Tokenize(
    const std::string& expr) {
  tokens_.clear();
  pos_ = 0;
  size_t i = 0;

  while (i < expr.length()) {
    char c = expr[i];

    if (std::isspace(c)) {
      ++i;
      continue;
    }

    size_t start = i;

    // Hexadecimal number
    if (c == '0' && i + 1 < expr.length() &&
        (expr[i + 1] == 'x' || expr[i + 1] == 'X')) {
      std::string literal = "0x";
      i += 2;

      if (i >= expr.length() || !std::isxdigit(expr[i])) {
        return std::unexpected("Invalid hexadecimal number");
      }

      while (i < expr.length() && std::isxdigit(expr[i])) {
        literal += expr[i++];
      }

      try {
        uint32_t val = std::stoul(literal, nullptr, 16);
        tokens_.push_back({TokenType::INT, literal, val, start});
      } catch (const std::invalid_argument&) {
        return std::unexpected("Invalid hexadecimal number");
      } catch (const std::out_of_range&) {
        return std::unexpected("Hexadecimal number is out of range");
      }

      continue;
    }

    // Number
    if (std::isdigit(c)) {
      std::string literal;
      while (i < expr.length() && std::isdigit(expr[i])) {
        literal += expr[i];
        i++;
      }
      try {
        uint32_t val = std::stoul(literal);
        tokens_.push_back({TokenType::INT, literal, val, start});
      } catch (const std::out_of_range&) {
        return std::unexpected("Number is out of range");
      }

      continue;
    }

    // Register
    if (c == '$') {
      i++;
      std::string literal;
      while (i < expr.length() && std::isalpha(expr[i])) {
        literal += expr[i];
        i++;
      }
      if (literal.empty()) {
        return std::unexpected("Empty register name");
      }
      tokens_.push_back({TokenType::REGISTER, literal, 0, start});
      continue;
    }

    // Identifier (E.g., tid, AND, OR)
    if (std::isalpha(c)) {
      std::string literal;
      while (i < expr.length() && std::isalpha(expr[i])) {
        literal += expr[i++];
      }
      boost::algorithm::to_lower(literal);

      if (literal == "tid") {
        tokens_.push_back({TokenType::IDENTIFIER, literal, 0, start});
      } else if (literal == "and") {
        tokens_.push_back({TokenType::AND, literal, 0, start});
      } else if (literal == "or") {
        tokens_.push_back({TokenType::OR, literal, 0, start});
      } else {
        return std::unexpected("Unexpected character at position " +
                               std::to_string(start));
      }
      continue;
    }

    // Operators
    switch (c) {
      case '+':
        tokens_.push_back({TokenType::PLUS, "+", 0, start});
        ++i;
        break;

      case '-':
        tokens_.push_back({TokenType::MINUS, "-", 0, start});
        ++i;
        break;

      case '*':
        tokens_.push_back({TokenType::ASTERISK, "*", 0, start});
        ++i;
        break;

      case '(':
        tokens_.push_back({TokenType::LPAREN, "(", 0, start});
        ++i;
        break;

      case ')':
        tokens_.push_back({TokenType::RPAREN, ")", 0, start});
        ++i;
        break;

      case '[':
        tokens_.push_back({TokenType::LBRACKET, "[", 0, start});
        ++i;
        break;

      case ']':
        tokens_.push_back({TokenType::RBRACKET, "]", 0, start});
        ++i;
        break;

      case ',':
        tokens_.push_back({TokenType::COMMA, ",", 0, start});
        ++i;
        break;

      case '@':
        tokens_.push_back({TokenType::AT, "@", 0, start});
        ++i;
        break;

      case '=':
        if (i + 1 < expr.length() && expr[i + 1] == '=') {
          tokens_.push_back({TokenType::EQ, "==", 0, start});
          i += 2;
        } else {
          return std::unexpected("Unexpected character at position " +
                                 std::to_string(start));
        }
        break;

      case '!':
        if (i + 1 < expr.length() && expr[i + 1] == '=') {
          tokens_.push_back({TokenType::NOT_EQ, "!=", 0, start});
          i += 2;
        } else {
          return std::unexpected("Unexpected character at position " +
                                 std::to_string(start));
        }
        break;

      case '<':
        if (i + 1 < expr.length() && expr[i + 1] == '=') {
          tokens_.push_back({TokenType::LTE, "<=", 0, start});
          i += 2;
        } else {
          tokens_.push_back({TokenType::LT, "<", 0, start});
          ++i;
        }
        break;

      case '>':
        if (i + 1 < expr.length() && expr[i + 1] == '=') {
          tokens_.push_back({TokenType::GTE, ">=", 0, start});
          i += 2;
        } else {
          tokens_.push_back({TokenType::GT, ">", 0, start});
          ++i;
        }
        break;

      case '&':
        if (i + 1 < expr.length() && expr[i + 1] == '&') {
          tokens_.push_back({TokenType::AND, "&&", 0, start});
          i += 2;
        } else {
          return std::unexpected("Unexpected character at position " +
                                 std::to_string(start));
        }
        break;

      case '|':
        if (i + 1 < expr.length() && expr[i + 1] == '|') {
          tokens_.push_back({TokenType::OR, "||", 0, start});
          i += 2;
        } else {
          return std::unexpected("Unexpected character at position " +
                                 std::to_string(start));
        }
        break;

      default:
        return std::unexpected("Unexpected character at position " +
                               std::to_string(start));
    }
  }

  tokens_.push_back({TokenType::END_OF_FILE, "", 0, i});
  return {};
}

std::expected<uint32_t, std::string> DebuggerExpressionParser::ParseExpression(
    Precedence precedence) {
  Token token = Consume();
  if (token.type == TokenType::END_OF_FILE) {
    return std::unexpected("Unexpected end of expression");
  }

  auto left = ParsePrefix(token);
  if (!left) {
    return left;
  }

  while (precedence < GetPrecedence(Peek().type)) {
    Token op = Consume();
    auto infix = ParseInfix(op, *left);
    if (!infix) {
      return infix;
    }
    left = infix;
  }

  return left;
}

std::expected<uint32_t, std::string> DebuggerExpressionParser::ParsePrefix(
    const Token& token) {
  switch (token.type) {
    case TokenType::INT:
      return token.int_value;

    case TokenType::IDENTIFIER:
      if (token.literal == "tid") {
        if (!thread_id_) {
          return std::unexpected("Thread ID not available in this context");
        }
        return *thread_id_;
      }
      return std::unexpected("Unknown identifier: " + token.literal);

    case TokenType::REGISTER:
      return ResolveRegisterValue(token.literal);

    case TokenType::LPAREN: {
      auto exp = ParseExpression(Precedence::LOWEST);
      if (!exp) {
        return exp;
      }
      if (Peek().type != TokenType::RPAREN) {
        return std::unexpected("Expected ')'");
      }
      Consume();
      return exp;
    }

    case TokenType::AT: {
      auto perform_read =
          [&](uint32_t addr,
              uint32_t size) -> std::expected<uint32_t, std::string> {
        if (!memory_reader_) {
          return std::unexpected("Memory reader not available");
        }
        if (size > 4) {
          return std::unexpected("Memory read size too large (max 4 bytes)");
        }

        auto data_res = memory_reader_(addr, size);
        if (!data_res) {
          return std::unexpected(data_res.error());
        }

        const auto& data = data_res.value();
        if (data.size() != size) {
          return std::unexpected("Failed to read requested memory size");
        }

        uint64_t val = 0;
        for (size_t i = 0; i < data.size(); ++i) {
          val |= static_cast<uint64_t>(data[i]) << (i * 8);
        }
        assert(val <= 0xFFFFFFFF && "Value overflow");

        return static_cast<uint32_t>(val);
      };

      if (Peek().type == TokenType::LPAREN) {
        Consume();
        auto addr_res = ParseExpression(Precedence::LOWEST);
        if (!addr_res) {
          return addr_res;
        }

        uint32_t size = 4;
        if (Peek().type == TokenType::COMMA) {
          Consume();
          auto size_res = ParseExpression(Precedence::LOWEST);
          if (!size_res) {
            return size_res;
          }
          size = *size_res;
        }

        if (Peek().type != TokenType::RPAREN) {
          return std::unexpected("Expected ')'");
        }
        Consume();
        return perform_read(*addr_res, size);
      }

      // Handle bare "@addr"
      // Use HIGHEST precedence to bind tightly to the next token
      auto addr_res = ParseExpression(Precedence::HIGHEST);
      if (!addr_res) {
        return addr_res;
      }

      uint32_t addr = *addr_res;

      // Check for optional bracket offset: [expression]
      if (Peek().type == TokenType::LBRACKET) {
        Consume();
        auto offset_res = ParseExpression(Precedence::LOWEST);
        if (!offset_res) {
          return offset_res;
        }
        if (Peek().type != TokenType::RBRACKET) {
          return std::unexpected("Expected ']'");
        }
        Consume();
        addr += *offset_res;
      }

      return perform_read(addr, 4);
    }

    default:
      return std::unexpected("Unexpected token: " + token.literal);
  }
}

std::expected<uint32_t, std::string> DebuggerExpressionParser::ParseInfix(
    const Token& token, uint32_t left) {
  Precedence p = GetPrecedence(token.type);
  auto right_res = ParseExpression(p);
  if (!right_res) {
    return right_res;
  }
  uint32_t right = *right_res;

  switch (token.type) {
    case TokenType::PLUS:
      return left + right;

    case TokenType::MINUS:
      return left - right;

    case TokenType::ASTERISK:
      return left * right;

    case TokenType::EQ:
      return (left == right) ? 1 : 0;

    case TokenType::NOT_EQ:
      return (left != right) ? 1 : 0;

    case TokenType::LT:
      return (left < right) ? 1 : 0;

    case TokenType::GT:
      return (left > right) ? 1 : 0;

    case TokenType::LTE:
      return (left <= right) ? 1 : 0;

    case TokenType::GTE:
      return (left >= right) ? 1 : 0;

    case TokenType::AND:
      return (left && right) ? 1 : 0;

    case TokenType::OR:
      return (left || right) ? 1 : 0;

    default:
      assert(!"Unhandled infix operator in ParseInfix");
      return std::unexpected("Internal parser error: unhandled operator " +
                             token.literal);
  }
}

std::expected<uint32_t, std::string>
DebuggerExpressionParser::ResolveRegisterValue(const std::string& reg) const {
  const std::string lower = boost::algorithm::to_lower_copy(reg);

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
    return resolve8(context_.eax, "eax");
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
