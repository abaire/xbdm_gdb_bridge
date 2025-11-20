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
