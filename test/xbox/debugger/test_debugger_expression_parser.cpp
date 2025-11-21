#include <boost/test/unit_test.hpp>
#include <cstdint>
#include <expected>
#include <string>

#include "xbox/debugger/debugger_expression_parser.h"

static std::expected<uint32_t, std::string> evaluate(const std::string& expr,
                                                     const ThreadContext& ctx) {
  DebuggerExpressionParser parser(ctx);
  return parser.Parse(expr);
}

BOOST_AUTO_TEST_SUITE(BasicArithmeticTests)

BOOST_AUTO_TEST_CASE(test_simple_addition) {
  ThreadContext ctx;
  auto result = evaluate("10 + 5", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 15);
}

BOOST_AUTO_TEST_CASE(test_simple_subtraction) {
  ThreadContext ctx;
  auto result = evaluate("20 - 7", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 13);
}

BOOST_AUTO_TEST_CASE(test_simple_multiplication) {
  ThreadContext ctx;
  auto result = evaluate("6 * 7", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 42);
}

BOOST_AUTO_TEST_CASE(test_operator_precedence) {
  ThreadContext ctx;
  auto result = evaluate("2 + 3 * 4", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 14);  // Not 20
}

BOOST_AUTO_TEST_CASE(test_left_associativity_addition) {
  ThreadContext ctx;
  auto result = evaluate("10 - 3 - 2", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 5);  // (10 - 3) - 2 = 5
}

BOOST_AUTO_TEST_CASE(test_left_associativity_multiplication) {
  ThreadContext ctx;
  auto result = evaluate("20 * 2 * 3", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 120);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(BooleanComparisonTests)

BOOST_AUTO_TEST_CASE(test_equality_true) {
  ThreadContext ctx;
  auto result = evaluate("10 == 10", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 1);
}

BOOST_AUTO_TEST_CASE(test_equality_false) {
  ThreadContext ctx;
  auto result = evaluate("10 == 11", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0);
}

BOOST_AUTO_TEST_CASE(test_inequality_true) {
  ThreadContext ctx;
  auto result = evaluate("10 != 11", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 1);
}

BOOST_AUTO_TEST_CASE(test_inequality_false) {
  ThreadContext ctx;
  auto result = evaluate("10 != 10", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0);
}

BOOST_AUTO_TEST_CASE(test_less_than_true) {
  ThreadContext ctx;
  auto result = evaluate("10 < 11", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 1);
}

BOOST_AUTO_TEST_CASE(test_less_than_false) {
  ThreadContext ctx;
  auto result = evaluate("10 < 10", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0);
}

BOOST_AUTO_TEST_CASE(test_greater_than_true) {
  ThreadContext ctx;
  auto result = evaluate("12 > 11", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 1);
}

BOOST_AUTO_TEST_CASE(test_greater_than_false) {
  ThreadContext ctx;
  auto result = evaluate("10 > 10", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0);
}

BOOST_AUTO_TEST_CASE(test_less_equal_true) {
  ThreadContext ctx;
  auto result = evaluate("10 <= 10", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 1);
}

BOOST_AUTO_TEST_CASE(test_greater_equal_true) {
  ThreadContext ctx;
  auto result = evaluate("10 >= 10", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 1);
}

BOOST_AUTO_TEST_CASE(test_register_comparison) {
  ThreadContext ctx;
  ctx.eax = 100;
  ctx.ebx = 200;
  auto result = evaluate("$eax < $ebx", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 1);
}

BOOST_AUTO_TEST_CASE(test_comparison_with_arithmetic) {
  ThreadContext ctx;
  ctx.eax = 100;
  auto result = evaluate("$eax + 10 == 110", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 1);
}

BOOST_AUTO_TEST_CASE(test_precedence_comparison) {
  ThreadContext ctx;
  auto result = evaluate("5 < 10 + 5", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 1);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(HexadecimalTests)

BOOST_AUTO_TEST_CASE(test_hex_lowercase) {
  ThreadContext ctx;
  auto result = evaluate("0x10", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 16);
}

BOOST_AUTO_TEST_CASE(test_hex_uppercase) {
  ThreadContext ctx;
  auto result = evaluate("0X20", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 32);
}

BOOST_AUTO_TEST_CASE(test_hex_with_letters) {
  ThreadContext ctx;
  auto result = evaluate("0xDEADBEEF", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0xDEADBEEF);
}

BOOST_AUTO_TEST_CASE(test_hex_addition) {
  ThreadContext ctx;
  auto result = evaluate("0x10 + 4", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 20);
}

BOOST_AUTO_TEST_CASE(test_hex_multiplication) {
  ThreadContext ctx;
  auto result = evaluate("0xdeadbeef * 3", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0xDEADBEEF * 3);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(RegisterTests)

BOOST_AUTO_TEST_CASE(test_32bit_register_eax) {
  ThreadContext ctx;
  ctx.eax = 0x12345678;
  auto result = evaluate("$eax", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0x12345678);
}

BOOST_AUTO_TEST_CASE(test_32bit_register_uppercase) {
  ThreadContext ctx;
  ctx.ebx = 0xAABBCCDD;
  auto result = evaluate("$EBX", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0xAABBCCDD);
}

BOOST_AUTO_TEST_CASE(test_16bit_register_ax) {
  ThreadContext ctx;
  ctx.eax = 0x12345678;
  auto result = evaluate("$ax", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0x5678);
}

BOOST_AUTO_TEST_CASE(test_8bit_register_ah) {
  ThreadContext ctx;
  ctx.eax = 0x12345678;
  auto result = evaluate("$ah", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0x56);
}

BOOST_AUTO_TEST_CASE(test_8bit_register_al) {
  ThreadContext ctx;
  ctx.eax = 0x12345678;
  auto result = evaluate("$al", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0x78);
}

BOOST_AUTO_TEST_CASE(test_register_addition) {
  ThreadContext ctx;
  ctx.eax = 100;
  ctx.ebx = 200;
  auto result = evaluate("$eax + $ebx", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 300);
}

BOOST_AUTO_TEST_CASE(test_register_with_number) {
  ThreadContext ctx;
  ctx.eax = 0x100;
  auto result = evaluate("$eax + 0x50", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0x150);
}

BOOST_AUTO_TEST_CASE(test_unset_register) {
  ThreadContext ctx;
  // eax is not set
  auto result = evaluate("$eax", ctx);
  BOOST_REQUIRE(!result.has_value());
  BOOST_CHECK(result.error().find("not available") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_unknown_register) {
  ThreadContext ctx;
  auto result = evaluate("$xyz", ctx);
  BOOST_REQUIRE(!result.has_value());
  BOOST_CHECK(result.error().find("Unknown register") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(ParenthesesTests)

BOOST_AUTO_TEST_CASE(test_simple_parentheses) {
  ThreadContext ctx;
  auto result = evaluate("(5 + 3)", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 8);
}

BOOST_AUTO_TEST_CASE(test_parentheses_override_precedence) {
  ThreadContext ctx;
  auto result = evaluate("(2 + 3) * 4", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 20);
}

BOOST_AUTO_TEST_CASE(test_nested_parentheses) {
  ThreadContext ctx;
  auto result = evaluate("((2 + 3) * 4)", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 20);
}

BOOST_AUTO_TEST_CASE(test_deeply_nested_parentheses) {
  ThreadContext ctx;
  auto result = evaluate("(((1 + 2) * 3) + 4) * 5", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 65);  // ((3 * 3) + 4) * 5 = 13 * 5 = 65
}

BOOST_AUTO_TEST_CASE(test_multiple_parentheses_groups) {
  ThreadContext ctx;
  auto result = evaluate("(10 + 5) * (3 + 2)", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 75);  // 15 * 5
}

BOOST_AUTO_TEST_CASE(test_complex_expression_with_parentheses) {
  ThreadContext ctx;
  ctx.eax = 100;
  auto result = evaluate("(($eax + 50) * 2) - 100", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 200);  // ((100 + 50) * 2) - 100 = 300 - 100
}

BOOST_AUTO_TEST_CASE(test_parentheses_with_registers) {
  ThreadContext ctx;
  ctx.eax = 10;
  ctx.ebx = 20;
  auto result = evaluate("($eax + $ebx) * 3", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 90);
}

BOOST_AUTO_TEST_CASE(test_nested_with_hex_and_registers) {
  ThreadContext ctx;
  ctx.eax = 0x10;
  auto result = evaluate("((0x20 + $eax) * 2) + 0x8", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(),
                    0x68);  // ((32 + 16) * 2) + 8 = 96 + 8 = 104
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(ErrorTests)

BOOST_AUTO_TEST_CASE(test_unbalanced_left_paren) {
  ThreadContext ctx;
  auto result = evaluate("(5 + 3", ctx);
  BOOST_REQUIRE(!result.has_value());
  BOOST_CHECK(result.error().find("Expected ')'") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_unbalanced_right_paren) {
  ThreadContext ctx;
  auto result = evaluate("5 + 3)", ctx);
  BOOST_REQUIRE(!result.has_value());
  BOOST_CHECK(result.error().find("Unexpected character") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_nested_unbalanced_parens) {
  ThreadContext ctx;
  auto result = evaluate("((5 + 3) * 2", ctx);
  BOOST_REQUIRE(!result.has_value());
  BOOST_CHECK(result.error().find("Expected ')'") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_empty_expression) {
  ThreadContext ctx;
  auto result = evaluate("", ctx);
  BOOST_REQUIRE(!result.has_value());
}

BOOST_AUTO_TEST_CASE(test_empty_parentheses) {
  ThreadContext ctx;
  auto result = evaluate("()", ctx);
  BOOST_REQUIRE(!result.has_value());
}

BOOST_AUTO_TEST_CASE(test_invalid_hex_no_digits) {
  ThreadContext ctx;
  auto result = evaluate("0x", ctx);
  BOOST_REQUIRE(!result.has_value());
  BOOST_CHECK(result.error().find("Invalid hexadecimal") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_missing_operand_after_operator) {
  ThreadContext ctx;
  auto result = evaluate("5 +", ctx);
  BOOST_REQUIRE(!result.has_value());
}

BOOST_AUTO_TEST_CASE(test_missing_operand_before_operator) {
  ThreadContext ctx;
  auto result = evaluate("+ 5", ctx);
  BOOST_REQUIRE(!result.has_value());
}

BOOST_AUTO_TEST_CASE(test_consecutive_operators) {
  ThreadContext ctx;
  auto result = evaluate("5 + * 3", ctx);
  BOOST_REQUIRE(!result.has_value());
}

BOOST_AUTO_TEST_CASE(test_invalid_character) {
  ThreadContext ctx;
  auto result = evaluate("5 & 3", ctx);
  BOOST_REQUIRE(!result.has_value());
  BOOST_CHECK(result.error().find("Unexpected character") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_register_without_dollar_sign) {
  ThreadContext ctx;
  ctx.eax = 100;
  auto result = evaluate("eax + 5", ctx);
  BOOST_REQUIRE(!result.has_value());
}

BOOST_AUTO_TEST_CASE(test_dollar_sign_without_register) {
  ThreadContext ctx;
  auto result = evaluate("$", ctx);
  BOOST_REQUIRE(!result.has_value());
  BOOST_CHECK(result.error().find("Empty register name") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_dollar_sign_with_number) {
  ThreadContext ctx;
  auto result = evaluate("$123", ctx);
  BOOST_REQUIRE(!result.has_value());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(ComplexExpressionTests)

BOOST_AUTO_TEST_CASE(test_complex_expression_1) {
  ThreadContext ctx;
  ctx.eax = 0x1000;
  ctx.ebx = 0x500;
  auto result = evaluate("($eax + $ebx) * 2 + 0x100", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(
      result.value(),
      0x2B00);  // (0x1000 + 0x500) * 2 + 0x100 = 0x1500 * 2 + 0x100
}

BOOST_AUTO_TEST_CASE(test_complex_expression_2) {
  ThreadContext ctx;
  ctx.eax = 100;
  auto result = evaluate("(($eax * 2) + 50) - (10 * 3)", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 220);  // ((100 * 2) + 50) - 30 = 250 - 30
}

BOOST_AUTO_TEST_CASE(test_complex_expression_with_subregisters) {
  ThreadContext ctx;
  ctx.eax = 0x12345678;
  auto result = evaluate("$ah + $al", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0x56 + 0x78);  // 86 + 120 = 206
}

BOOST_AUTO_TEST_CASE(test_mixed_decimal_hex) {
  ThreadContext ctx;
  auto result = evaluate("(100 + 0xFF) * 2", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 710);  // (100 + 255) * 2
}

BOOST_AUTO_TEST_CASE(test_whitespace_handling) {
  ThreadContext ctx;
  auto result = evaluate("  ( 5   +  3 )  *  2  ", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 16);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(EdgeCaseTests)

BOOST_AUTO_TEST_CASE(test_zero) {
  ThreadContext ctx;
  auto result = evaluate("0", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0);
}

BOOST_AUTO_TEST_CASE(test_hex_zero) {
  ThreadContext ctx;
  auto result = evaluate("0x0", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0);
}

BOOST_AUTO_TEST_CASE(test_subtraction_to_zero) {
  ThreadContext ctx;
  auto result = evaluate("5 - 5", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0);
}

BOOST_AUTO_TEST_CASE(test_multiplication_by_zero) {
  ThreadContext ctx;
  auto result = evaluate("12345 * 0", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0);
}

BOOST_AUTO_TEST_CASE(test_large_hex_value) {
  ThreadContext ctx;
  auto result = evaluate("0xFFFFFFFF", ctx);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0xFFFFFFFF);
}

BOOST_AUTO_TEST_CASE(test_overflow_behavior) {
  ThreadContext ctx;
  auto result = evaluate("0xFFFFFFFF + 1", ctx);
  BOOST_REQUIRE(result.has_value());
  // This will overflow, checking it wraps around
  BOOST_CHECK_EQUAL(result.value(), 0);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(ThreadIDTests)

BOOST_AUTO_TEST_CASE(test_tid_parsing) {
  ThreadContext ctx;
  DebuggerExpressionParser parser(ctx, 28);
  auto result = parser.Parse("tid");
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 28);
}

BOOST_AUTO_TEST_CASE(test_tid_comparison) {
  ThreadContext ctx;
  DebuggerExpressionParser parser(ctx, 28);
  auto result = parser.Parse("tid == 28");
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 1);
}

BOOST_AUTO_TEST_CASE(test_tid_not_available) {
  ThreadContext ctx;
  DebuggerExpressionParser parser(ctx);
  auto result = parser.Parse("tid");
  BOOST_REQUIRE(!result.has_value());
  BOOST_CHECK(result.error().find("Thread ID not available") !=
              std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(MemoryAccessTests)

static std::expected<std::vector<uint8_t>, std::string> MockMemoryReader(
    uint32_t address, uint32_t size) {
  if (address == 0x123) {
    std::vector<uint8_t> data = {0x00, 0x20, 0x00, 0x00};
    if (size > data.size()) {
      return std::unexpected("Read size too large");
    }
    data.resize(size);
    return data;
  }

  if (address == 0x1000) {
    std::vector<uint8_t> data(size);
    for (uint32_t i = 0; i < size; ++i) {
      data[i] = static_cast<uint8_t>(i + 1);
    }
    return data;
  }

  if (address == 0x2000) {
    std::vector<uint8_t> data = {0x10, 0x20, 0x30, 0x40,
                                 0x50, 0x60, 0x70, 0x80};
    if (size > data.size()) {
      return std::unexpected("Read size too large");
    }
    data.resize(size);
    return data;
  }
  return std::unexpected("Memory read failed");
}

BOOST_AUTO_TEST_CASE(test_memory_read_simple) {
  ThreadContext ctx;
  DebuggerExpressionParser parser(ctx, -1, MockMemoryReader);
  auto result = parser.Parse("@0x1000");
  BOOST_REQUIRE(result.has_value());
  // Default size is 4 bytes: 0x04030201 (little endian)
  BOOST_CHECK_EQUAL(result.value(), 0x04030201);
}

BOOST_AUTO_TEST_CASE(test_memory_read_with_parens) {
  ThreadContext ctx;
  DebuggerExpressionParser parser(ctx, -1, MockMemoryReader);
  auto result = parser.Parse("@(0x1000)");
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0x04030201);
}

BOOST_AUTO_TEST_CASE(test_memory_read_1_byte) {
  ThreadContext ctx;
  DebuggerExpressionParser parser(ctx, -1, MockMemoryReader);
  auto result = parser.Parse("@(0x1000, 1)");
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0x01);
}

BOOST_AUTO_TEST_CASE(test_memory_read_2_bytes) {
  ThreadContext ctx;
  DebuggerExpressionParser parser(ctx, -1, MockMemoryReader);
  auto result = parser.Parse("@(0x1000, 2)");
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0x0201);
}

BOOST_AUTO_TEST_CASE(test_memory_read_register_address) {
  ThreadContext ctx;
  ctx.eax = 0x1000;
  DebuggerExpressionParser parser(ctx, -1, MockMemoryReader);
  auto result = parser.Parse("@($eax)");
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0x04030201);
}

BOOST_AUTO_TEST_CASE(test_memory_read_complex_address) {
  ThreadContext ctx;
  ctx.eax = 0x1000;
  DebuggerExpressionParser parser(ctx, -1, MockMemoryReader);
  auto result = parser.Parse("@($eax + 0x1000)");  // 0x2000
  BOOST_REQUIRE(result.has_value());
  auto val = result.value();
  BOOST_CHECK_EQUAL(val, 0x40302010);
}

BOOST_AUTO_TEST_CASE(test_memory_read_invalid_address) {
  ThreadContext ctx;
  DebuggerExpressionParser parser(ctx, -1, MockMemoryReader);
  auto result = parser.Parse("@0x3000");
  BOOST_REQUIRE(!result.has_value());
  BOOST_CHECK(result.error().find("Memory read failed") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_memory_read_no_reader) {
  ThreadContext ctx;
  DebuggerExpressionParser parser(ctx);
  auto result = parser.Parse("@0x1000");
  BOOST_REQUIRE(!result.has_value());
  BOOST_CHECK(result.error().find("Memory reader not available") !=
              std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_memory_read_invalid_size) {
  ThreadContext ctx;
  DebuggerExpressionParser parser(ctx, -1, MockMemoryReader);
  auto result = parser.Parse("@(0x1000, 5)");
  BOOST_REQUIRE(!result.has_value());
  BOOST_CHECK(result.error().find("size too large") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_memory_precedence_vs_equality) {
  ThreadContext ctx;
  DebuggerExpressionParser parser(ctx, -1, MockMemoryReader);
  // @0x2000 reads 0x40302010.
  // Previously, this parsed as @(0x2000 == 0x40302010) -> @0 -> Error
  auto result = parser.Parse("@0x2000 == 0x40302010");
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 1);
}

BOOST_AUTO_TEST_CASE(test_memory_precedence_vs_arithmetic) {
  ThreadContext ctx;
  DebuggerExpressionParser parser(ctx, -1, MockMemoryReader);
  // @0x1000 reads 0x04030201.
  // This should parse as (@0x1000) + 1, not @(0x1000 + 1).
  auto result = parser.Parse("@0x1000 + 1");
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0x04030201 + 1);
}

BOOST_AUTO_TEST_CASE(test_explicit_parens_override_precedence) {
  ThreadContext ctx;
  DebuggerExpressionParser parser(ctx, -1, MockMemoryReader);
  // Here we explicitly want the addition to happen before the address lookup.
  // 0x1000 + 0x1000 = 0x2000. @0x2000 = 0x40302010.
  auto result = parser.Parse("@(0x1000 + 0x1000)");
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0x40302010);
}

BOOST_AUTO_TEST_CASE(test_nested_dereference) {
  ThreadContext ctx;
  DebuggerExpressionParser parser(ctx, -1, MockMemoryReader);

  auto result = parser.Parse("@(@0x123)");
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0x40302010);
}

BOOST_AUTO_TEST_CASE(test_nested_dereference_raw) {
  ThreadContext ctx;
  DebuggerExpressionParser parser(ctx, -1, MockMemoryReader);

  auto result = parser.Parse("@@0x123");
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(result.value(), 0x40302010);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(parsing_suite)

BOOST_AUTO_TEST_CASE(ArgParser_SimpleArithmeticExpression) {
  ArgParser p("cmd (1+2)");

  BOOST_TEST(p.size() == 1);

  uint32_t value{0};
  BOOST_TEST(static_cast<bool>(
      p.Parse(0, value, std::make_shared<DebuggerExpressionParser>())));
  BOOST_TEST(value == 3);
}

BOOST_AUTO_TEST_CASE(ArgParser_RegisterArithmeticExpression) {
  ArgParser p("cmd ($eax + 1 * 2)");

  BOOST_TEST(p.size() == 1);

  ThreadContext ctx;
  ctx.eax = 100;
  uint32_t value{0};
  BOOST_TEST(static_cast<bool>(
      p.Parse(0, value, std::make_shared<DebuggerExpressionParser>(ctx))));
  BOOST_TEST(value == 102);
}

BOOST_AUTO_TEST_SUITE_END()
