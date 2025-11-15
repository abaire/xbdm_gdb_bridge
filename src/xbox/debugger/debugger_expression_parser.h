#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_EXPRESSION_PARSER_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_EXPRESSION_PARSER_H_

#include <expected>

#include "rdcp/types/thread_context.h"

/**
 * Processes basic arithmetic expressions and resolves register references.
 */
class DebuggerExpressionParser {
 public:
  explicit DebuggerExpressionParser(const ThreadContext& context)
      : context_(context), pos(0) {}

  /**
   * Attempts to evaluate the given string as an expression.
   * @param expr - The expression to evaluate
   * @return The final value or a string indicating an error.
   */
  std::expected<uint32_t, std::string> parse(const std::string& expr);

 private:
  [[nodiscard]] inline char peek() const {
    return pos < input_.size() ? input_[pos] : 0;
  }

  inline char consume() { return pos < input_.size() ? input_[pos++] : 0; }

  inline void skip_whitespace() {
    while (pos < input_.size() && std::isspace(input_[pos])) {
      pos++;
    }
  }

  [[nodiscard]] std::expected<uint32_t, std::string> resolve_reg_value(
      const std::string& reg) const;
  std::expected<std::string, std::string> parse_register();
  std::expected<uint32_t, std::string> parse_number();
  std::expected<uint32_t, std::string> parse_factor();
  std::expected<uint32_t, std::string> parse_term();
  std::expected<uint32_t, std::string> parse_expression();

 private:
  const ThreadContext& context_;
  std::string input_;
  size_t pos;
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_EXPRESSION_PARSER_H_
