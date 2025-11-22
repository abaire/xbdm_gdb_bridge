#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_EXPRESSION_PARSER_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_EXPRESSION_PARSER_H_

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "rdcp/types/thread_context.h"
#include "util/parsing.h"

enum class Precedence {
  LOWEST = 1,
  LOGICAL_OR,   // ||, OR
  LOGICAL_AND,  // &&, AND
  EQUALITY,     // ==, !=
  RELATIONAL,   // <, >, <=, >=
  SUM,          // +, -
  PRODUCT,      // *
  PREFIX,       // @, - (if implemented), etc.
  CALL,         // ( )
  HIGHEST       // Special binding for @
};

enum class TokenType {
  ILLEGAL,
  END_OF_FILE,
  IDENTIFIER,  // tid
  INT,         // 123, 0x123
  REGISTER,    // $eax

  // Operators
  PLUS,      // +
  MINUS,     // -
  ASTERISK,  // *
  AT,        // @

  EQ,      // ==
  NOT_EQ,  // !=
  LT,      // <
  GT,      // >
  LTE,     // <=
  GTE,     // >=

  AND,  // &&, AND
  OR,   // ||, OR

  LPAREN,    // (
  RPAREN,    // )
  COMMA,     // ,
  LBRACKET,  // [
  RBRACKET   // ]
};

struct Token {
  TokenType type;
  std::string literal;
  uint32_t int_value = 0;  // For INT tokens
  size_t start_pos = 0;
};

/**
 * Processes basic arithmetic expressions and resolves register references.
 */
class DebuggerExpressionParser : public ExpressionParser {
 public:
  using MemoryReader =
      std::function<std::expected<std::vector<uint8_t>, std::string>(
          uint32_t address, uint32_t size)>;

  DebuggerExpressionParser() = default;
  explicit DebuggerExpressionParser(
      const ThreadContext& context,
      std::optional<uint32_t> thread_id = std::nullopt,
      MemoryReader memory_reader = nullptr)
      : context_(context),
        thread_id_(thread_id),
        memory_reader_(std::move(memory_reader)) {}

  std::expected<uint32_t, std::string> Parse(const std::string& expr) override;

 protected:
  ThreadContext context_;
  std::optional<uint32_t> thread_id_;
  MemoryReader memory_reader_;

 private:
  std::vector<Token> tokens_;
  size_t pos_{0};

  [[nodiscard]] Token Peek() const;
  Token Consume();

  std::expected<void, std::string> Tokenize(const std::string& expr);
  std::expected<uint32_t, std::string> ParseExpression(Precedence precedence);

  // NUD (Null Denotation) - Prefix handlers
  std::expected<uint32_t, std::string> ParsePrefix(const Token& token);

  // LED (Left Denotation) - Infix handlers
  std::expected<uint32_t, std::string> ParseInfix(const Token& token,
                                                  uint32_t left);

  [[nodiscard]] std::expected<uint32_t, std::string> ResolveRegisterValue(
      const std::string& reg) const;

  static constexpr Precedence GetPrecedence(TokenType type);
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_EXPRESSION_PARSER_H_
