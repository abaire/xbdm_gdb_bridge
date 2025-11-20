#ifndef XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_EXPRESSION_PARSER_H_
#define XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_EXPRESSION_PARSER_H_

#include <expected>

#include "rdcp/types/thread_context.h"
#include "util/parsing.h"

/**
 * Processes basic arithmetic expressions and resolves register references.
 */
class DebuggerExpressionParser : public ExpressionParser {
 public:
  DebuggerExpressionParser() = default;
  explicit DebuggerExpressionParser(const ThreadContext& context)
      : context_(context) {}

  std::expected<uint32_t, std::string> Parse(const std::string& expr) override;

 private:
  struct ParseState {
    ParseState(const ThreadContext& context) : context_(context) {}

    std::expected<uint32_t, std::string> Parse(const std::string& expr);

    [[nodiscard]] char Peek() const {
      return pos < input_.size() ? input_[pos] : 0;
    }

    char Consume() { return pos < input_.size() ? input_[pos++] : 0; }

    void SkipWhitespace() {
      while (pos < input_.size() && std::isspace(input_[pos])) {
        pos++;
      }
    }

    [[nodiscard]] std::expected<uint32_t, std::string> ResolveRegisterValue(
        const std::string& reg) const;
    std::expected<std::string, std::string> ParseRegister();
    std::expected<uint32_t, std::string> ParseNumber();
    std::expected<uint32_t, std::string> ParseFactor();
    std::expected<uint32_t, std::string> ParseTerm();
    std::expected<uint32_t, std::string> ParseAdditive();
    std::expected<uint32_t, std::string> ParseExpression();

   private:
    const ThreadContext& context_;
    std::string input_;
    size_t pos{0};
  };

 protected:
  ThreadContext context_;
};

#endif  // XBDM_GDB_BRIDGE_SRC_XBOX_DEBUGGER_EXPRESSION_PARSER_H_
