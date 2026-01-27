#include <boost/test/unit_test.hpp>

#include "util/parsing.h"

struct MockExpressionParser : ExpressionParser {
  std::expected<uint32_t, std::string> Parse(const std::string& expr) override {
    if (expr == "1 + 2") {
      return 3;
    }
    if (expr == "failure") {
      return std::unexpected("Mock Syntax Error");
    }
    return std::unexpected("Unknown expression");
  }
};

BOOST_AUTO_TEST_SUITE(parsing_suite)

BOOST_AUTO_TEST_CASE(parse_hex_int_valid_byte_array) {
  std::vector<uint8_t> data = {'1', 'a', '2', 'b'};
  uint32_t result;
  bool parsed = MaybeParseHexInt(result, data, 0);
  BOOST_TEST(parsed);
  BOOST_TEST(result == 0x1a2b);
}

BOOST_AUTO_TEST_CASE(parse_hex_int_empty_byte_array) {
  std::vector<uint8_t> data;
  uint32_t result;
  bool parsed = MaybeParseHexInt(result, data);
  BOOST_TEST(!parsed);
}

BOOST_AUTO_TEST_CASE(parse_hex_int_leading_valid_byte_array) {
  std::vector<uint8_t> data = {'1', 'a', '2', 'b', 'Z', 'Z'};
  uint32_t result;
  bool parsed = MaybeParseHexInt(result, data, 0);
  BOOST_TEST(parsed);
  BOOST_TEST(result == 0x1a2b);
}

BOOST_AUTO_TEST_CASE(parse_hex_int_invalid_byte_array) {
  std::vector<uint8_t> data = {'z', '1', 'a', '2', 'b'};
  uint32_t result;
  bool parsed = MaybeParseHexInt(result, data, 0);
  BOOST_TEST(!parsed);
}

BOOST_AUTO_TEST_CASE(parse_hex_int_valid_after_offset_byte_array) {
  std::vector<uint8_t> data = {'z', '1', 'a', '2', 'b'};
  uint32_t result;
  bool parsed = MaybeParseHexInt(result, data, 1);
  BOOST_TEST(parsed);
  BOOST_TEST(result == 0x1a2b);
}

BOOST_AUTO_TEST_CASE(parse_hex_int_valid_string) {
  std::string data = "1a2b";
  uint32_t result;
  bool parsed = MaybeParseHexInt(result, data);
  BOOST_TEST(parsed);
  BOOST_TEST(result == 0x1a2b);
}

BOOST_AUTO_TEST_CASE(parse_hex_int_empty_string) {
  std::string data = "";
  uint32_t result;
  bool parsed = MaybeParseHexInt(result, data);
  BOOST_TEST(!parsed);
}

BOOST_AUTO_TEST_CASE(parse_hex_int_partially_valid_string) {
  std::string data = "1a2bZZ";
  uint32_t result;
  bool parsed = MaybeParseHexInt(result, data);
  BOOST_TEST(parsed);
  BOOST_TEST(result == 0x1a2b);
}

BOOST_AUTO_TEST_CASE(parse_hex_int_invalid_string) {
  std::string data = "zz1a2b";
  uint32_t result;
  bool parsed = MaybeParseHexInt(result, data);
  BOOST_TEST(!parsed);
}

BOOST_AUTO_TEST_CASE(ParseInt32_String_Decimal) {
  BOOST_TEST(ParseInt32("123") == 123);
  BOOST_TEST(ParseInt32("-123") == -123);
  BOOST_TEST(ParseInt32("0") == 0);
}

BOOST_AUTO_TEST_CASE(ParseInt32_String_Hex) {
  BOOST_TEST(ParseInt32("0x10") == 16);
  BOOST_TEST(ParseInt32("0X10") == 16);
  BOOST_TEST(ParseInt32("0xFF") == 255);
  // ParseInt32 uses strtol which handles negative hex if it fits in long, but
  // here we expect 32-bit behavior. Let's check standard positive hex values.
}

BOOST_AUTO_TEST_CASE(ParseInt32_VectorUint8) {
  std::vector<uint8_t> data = {'1', '2', '3'};
  BOOST_TEST(ParseInt32(data) == 123);
}

BOOST_AUTO_TEST_CASE(ParseInt32_VectorChar) {
  std::vector<char> data = {'-', '4', '5'};
  BOOST_TEST(ParseInt32(data) == -45);
}

BOOST_AUTO_TEST_CASE(ParseUint32_String_Decimal) {
  BOOST_TEST(ParseUint32("123") == 123);
  BOOST_TEST(ParseUint32("0") == 0);
}

BOOST_AUTO_TEST_CASE(ParseUint32_String_Hex) {
  BOOST_TEST(ParseUint32("0x10") == 16);
  BOOST_TEST(ParseUint32("0X10") == 16);
  BOOST_TEST(ParseUint32("0xFFFFFFFF") == 0xFFFFFFFF);
}

BOOST_AUTO_TEST_CASE(ParseUint32_VectorUint8) {
  std::vector<uint8_t> data = {'1', '2', '3'};
  BOOST_TEST(ParseUint32(data) == 123);
}

BOOST_AUTO_TEST_CASE(ParseUint32_VectorChar) {
  std::vector<char> data = {'4', '5'};
  BOOST_TEST(ParseUint32(data) == 45);
}

BOOST_AUTO_TEST_CASE(ArgParser_BasicCommandArgs) {
  ArgParser p("process file1 file2");

  BOOST_TEST(p.HasCommand());
  BOOST_TEST(p.command == "process");

  std::string val;
  // arg[0] -> file1
  auto type0 = p.Parse(0, val);
  BOOST_TEST((type0 == ArgParser::ArgType::BASIC));
  BOOST_TEST(val == "file1");

  // arg[1] -> file2
  auto type1 = p.Parse(1, val);
  BOOST_TEST((type1 == ArgParser::ArgType::BASIC));
  BOOST_TEST(val == "file2");
}

BOOST_AUTO_TEST_CASE(ArgParser_ExtractSubcommand) {
  ArgParser p("git commit -m message");

  BOOST_TEST(p.command == "git");

  // Before extraction
  BOOST_TEST(p.ArgExists("commit"));  // "commit" is currently arg[0]

  // Extract
  auto maybe_sub = p.ExtractSubcommand();
  BOOST_REQUIRE(maybe_sub);

  auto& sub = *maybe_sub;
  BOOST_TEST(sub.IsCommand("commit"));

  // Verify offsets shifted
  // Old arg[1] "-m" should now be arg[0]
  std::string val;
  sub.Parse(0, val);
  BOOST_TEST(val == "-m");
}

BOOST_AUTO_TEST_CASE(ArgParser_QuotedStrings) {
  // Tests preservation of spaces and stripping of quotes
  ArgParser p(R"(echo "hello world" "quoted" not="quoted")");

  BOOST_TEST(p.command == "echo");

  std::string val;
  auto type = p.Parse(0, val);

  BOOST_TEST((type == ArgParser::ArgType::QUOTED));
  BOOST_TEST(val == "hello world");

  type = p.Parse(1, val);
  BOOST_TEST((type == ArgParser::ArgType::QUOTED));
  BOOST_TEST(val == "quoted");

  type = p.Parse(2, val);
  BOOST_TEST((type == ArgParser::ArgType::BASIC));
  BOOST_TEST(val == "not=\"quoted\"");
}

BOOST_AUTO_TEST_CASE(ArgParser_QuotedEscapeSequences) {
  // Tests \" becoming " inside a string
  ArgParser p(R"(print "say \"hello\" now")");

  std::string val;
  auto type = p.Parse(0, val);

  BOOST_TEST((type == ArgParser::ArgType::QUOTED));
  BOOST_TEST(val == "say \"hello\" now");
}

BOOST_AUTO_TEST_CASE(ArgParser_ParenthesizedGroups) {
  // Tests ( ... ) grouping and stripping outer parens
  ArgParser p("func (vec3 1 0 0) (scale 5)");

  std::string val;
  auto type = p.Parse(0, val);

  BOOST_TEST((type == ArgParser::ArgType::PARENTHESIZED));
  BOOST_TEST(val == "vec3 1 0 0");

  type = p.Parse(1, val);
  BOOST_TEST((type == ArgParser::ArgType::PARENTHESIZED));
  BOOST_TEST(val == "scale 5");
}

BOOST_AUTO_TEST_CASE(ArgParser_NestedParentheses) {
  // Tests that only the outermost parens are stripped
  ArgParser p("math (calc (1 + 2))");

  std::string val;
  auto type = p.Parse(0, val);

  BOOST_TEST((type == ArgParser::ArgType::PARENTHESIZED));
  BOOST_TEST(val == "calc (1 + 2)");
}

BOOST_AUTO_TEST_CASE(ArgParser_ParsingPrimitives) {
  ArgParser p("config 1024 true 0 yes");

  int intVal;
  bool boolVal;

  // Parse Int
  auto type = p.Parse(0, intVal);
  BOOST_TEST((type == ArgParser::ArgType::BASIC));
  BOOST_TEST(intVal == 1024);

  // Parse Bool (true)
  p.Parse(1, boolVal);
  BOOST_TEST(boolVal);

  // Parse Bool (0 -> false)
  p.Parse(2, boolVal);
  BOOST_TEST(!boolVal);

  // Parse Bool (yes -> true)
  p.Parse(3, boolVal);
  BOOST_TEST(boolVal);
}

BOOST_AUTO_TEST_CASE(ArgParser_BoundsChecking) {
  ArgParser p("cmd one");

  std::string val;
  auto type = p.Parse(1, val);  // Index 1 does not exist (size is 1)

  BOOST_TEST((type == ArgParser::ArgType::NOT_FOUND));
}

BOOST_AUTO_TEST_CASE(ArgParser_ShiftPrefixModifier) {
  ArgParser p("cmd @subcommand");

  auto maybe_sub = p.ExtractSubcommand();
  BOOST_REQUIRE(maybe_sub);

  auto& sub = *maybe_sub;
  BOOST_TEST(sub.IsCommand("@subcommand"));

  bool shifted = sub.ShiftPrefixModifier('@');
  BOOST_TEST(shifted);
  BOOST_TEST(sub.IsCommand("subcommand"));

  // Try shifting non-existent prefix
  shifted = sub.ShiftPrefixModifier('!');
  BOOST_TEST(!shifted);
  BOOST_TEST(sub.IsCommand("subcommand"));
}

BOOST_AUTO_TEST_CASE(ArgParser_MixedSyntax) {
  // cmd "quote" plain (paren)
  ArgParser p("cmd \"quote\" plain (paren)");

  std::string val;

  auto t1 = p.Parse(0, val);
  BOOST_TEST((t1 == ArgParser::ArgType::QUOTED));
  BOOST_TEST(val == "quote");

  auto t2 = p.Parse(1, val);
  BOOST_TEST((t2 == ArgParser::ArgType::BASIC));
  BOOST_TEST(val == "plain");

  auto t3 = p.Parse(2, val);
  BOOST_TEST((t3 == ArgParser::ArgType::PARENTHESIZED));
  BOOST_TEST(val == "paren");
}

BOOST_AUTO_TEST_CASE(ArgParser_ParensInsideQuotes) {
  // Ensure ( ... ) inside quotes are treated as literals
  ArgParser p("cmd \"(literal parens)\"");

  std::string val;
  auto type = p.Parse(0, val);

  BOOST_TEST((type == ArgParser::ArgType::QUOTED));
  BOOST_TEST(val == "(literal parens)");
}

BOOST_AUTO_TEST_CASE(ArgParser_Iterator_BasicIteration) {
  ArgParser p("cmd one two three");
  std::vector<std::string> expected = {"one", "two", "three"};
  std::vector<std::string> actual;

  for (auto it = p.begin(); it != p.end(); ++it) {
    actual.push_back(*it);
  }

  BOOST_TEST(actual == expected);
}

BOOST_AUTO_TEST_CASE(ArgParser_Iterator_RangeBasedFor) {
  ArgParser p("concat A B C");
  std::string result;

  for (const auto& arg : p) {
    result += arg;
  }

  BOOST_TEST(result == "ABC");
}

BOOST_AUTO_TEST_CASE(ArgParser_Iterator_RandomAccess) {
  ArgParser p("jump index0 index1 index2 index3");
  auto it = p.begin();

  // Test dereference at offset
  BOOST_TEST(*(it + 2) == "index2");

  // Test increment
  it += 3;
  BOOST_TEST(*it == "index3");

  // Test decrement
  it -= 2;
  BOOST_TEST(*it == "index1");

  // Test subtraction (distance)
  BOOST_TEST((p.end() - p.begin()) == 4);
}

BOOST_AUTO_TEST_CASE(ArgParser_Iterator_EmptyArgs) {
  ArgParser p("command_only");

  // Command is extracted, arguments should be empty
  BOOST_TEST(p.HasCommand());
  BOOST_TEST(p.empty());
  BOOST_TEST((p.begin() == p.end()));

  int count = 0;
  for (const auto& arg : p) {
    count++;
  }
  BOOST_TEST(count == 0);
}

BOOST_AUTO_TEST_CASE(ArgParser_Iterator_WithSubcommandExtraction) {
  ArgParser p("git commit -m message");

  // Initial state: ["commit", "-m", "message"]
  BOOST_TEST((p.end() - p.begin()) == 3);
  BOOST_TEST(*p.begin() == "commit");

  auto maybe_sub = p.ExtractSubcommand();
  BOOST_REQUIRE(maybe_sub);
  auto& sub = *maybe_sub;

  // Post-extraction state: ["-m", "message"]
  BOOST_TEST((sub.end() - sub.begin()) == 2);
  BOOST_TEST(*sub.begin() == "-m");

  std::vector<std::string> remaining;
  for (const auto& arg : sub) {
    remaining.push_back(arg);
  }
  BOOST_TEST(remaining.size() == 2);
  BOOST_TEST(remaining[1] == "message");
}

BOOST_AUTO_TEST_CASE(ArgParser_ExplicitConstruction) {
  std::vector<std::string> input_args = {"arg1", "ARG2"};

  // Construct with explicit command and vector
  ArgParser p("MyCommand", input_args);

  // Check Command (should be lowercased)
  BOOST_TEST(p.command == "mycommand");
  BOOST_TEST(p.HasCommand());

  // Check Args
  BOOST_TEST(p.size() == 2);

  std::string val;

  // Verify arg1
  auto type1 = p.Parse(0, val);
  BOOST_TEST(val == "arg1");
  BOOST_TEST((type1 == ArgParser::ArgType::BASIC));

  // Verify arg2 (should preserve case of arguments, unlike command)
  auto type2 = p.Parse(1, val);
  BOOST_TEST(val == "ARG2");
  BOOST_TEST((type2 == ArgParser::ArgType::BASIC));
}

BOOST_AUTO_TEST_CASE(ArgParser_Expression_Success) {
  // Input: cmd (1 + 2)
  ArgParser p("cmd (1 + 2)");
  MockExpressionParser mock;
  uint32_t result = 0;

  // Parse index 0 using the expression parser
  auto type = p.Parse(0, result, mock);

  BOOST_TEST((type == ArgParser::ArgType::PARENTHESIZED));
  BOOST_TEST(result == 3);

  // Verify the boolean operator works on the result type
  BOOST_TEST(static_cast<bool>(type));
}

BOOST_AUTO_TEST_CASE(ArgParser_Expression_SyntaxError) {
  // Input: cmd (failure)
  ArgParser p("cmd (failure)");
  MockExpressionParser mock;
  uint32_t result = 999;  // Sentinel value

  auto type = p.Parse(0, result, mock);

  // 1. Should return SYNTAX_ERROR
  BOOST_TEST((type == ArgParser::ArgType::SYNTAX_ERROR));

  // 2. Boolean conversion should be false
  BOOST_TEST(!type);

  // 3. Result should not be modified (or undefined, depending on
  // implementation,
  //    but strictly speaking the code doesn't write to ret on error)
  BOOST_TEST(result == 999);

  // 4. Error message should be propagated
  BOOST_TEST(type.message == "Mock Syntax Error");
}

BOOST_AUTO_TEST_CASE(ArgParser_Expression_FallbackToBasic) {
  // Input: cmd 123
  // This is NOT parenthesized, so ArgParser should skip the ExpressionParser
  // and call ParseInt32 directly.
  ArgParser p("cmd 123");
  MockExpressionParser mock;
  uint32_t result = 0;

  // We assume ParseInt32("123") works in your environment
  auto type = p.Parse(0, result, mock);

  // Should return BASIC, not PARENTHESIZED
  BOOST_TEST((type == ArgParser::ArgType::BASIC));
  BOOST_TEST(result == 123);
}

BOOST_AUTO_TEST_CASE(ArgParser_Expression_FallbackToQuoted) {
  // Input: cmd "456"
  ArgParser p("cmd \"456\"");
  MockExpressionParser mock;
  uint32_t result = 0;

  auto type = p.Parse(0, result, mock);

  // Should return QUOTED
  BOOST_TEST((type == ArgParser::ArgType::QUOTED));
  BOOST_TEST(result == 456);
}

BOOST_AUTO_TEST_CASE(ArgParser_Expression_OutOfBounds) {
  ArgParser p("cmd");
  MockExpressionParser mock;
  uint32_t result = 0;

  auto type = p.Parse(0, result,
                      mock);  // Index 0 exists? No, size is 0 (cmd extracted)

  BOOST_TEST((type == ArgParser::ArgType::NOT_FOUND));
  BOOST_TEST(!type);
}

BOOST_AUTO_TEST_CASE(ArgParser_FrontAndBack) {
  ArgParser p("cmd first middle last");

  BOOST_TEST(!p.empty());
  BOOST_TEST(p.size() == 3);

  // Test front()
  BOOST_TEST(p.front() == "first");

  // Test back()
  BOOST_TEST(p.back() == "last");
}

BOOST_AUTO_TEST_CASE(ArgParser_SplitAt_FoundBasic) {
  ArgParser p("launch program1 argA argB | pipeTo program2 argC");
  ArgParser pre(""), post("");

  // Split at the pipe
  bool found = p.SplitAt(pre, post, "|");

  BOOST_TEST(found);

  // Verify PRE: "launch" [program1, argA, argB]
  BOOST_TEST(pre.command == "launch");
  BOOST_TEST(pre.size() == 3);
  BOOST_TEST(pre.arguments[0].value == "program1");
  BOOST_TEST(pre.arguments[2].value == "argB");

  // Verify POST: "pipeTo" [program2, argC]
  // Note: "pipeTo" is the token *after* the pipe, so it becomes the command
  BOOST_TEST(post.command == "pipeto");  // Commands are auto-lowercased in ctor
  BOOST_REQUIRE(post.size() == 2);
  BOOST_TEST(post.arguments[0].value == "program2");
  BOOST_TEST(post.arguments[1].value == "argC");
}

BOOST_AUTO_TEST_CASE(ArgParser_SplitAt_NotFound) {
  ArgParser p("cmd arg1 arg2");
  ArgParser pre(""), post("");

  bool found = p.SplitAt(pre, post, "|");

  BOOST_TEST(!found);
  BOOST_TEST(pre == p);
  BOOST_TEST(post.command.empty());
}

BOOST_AUTO_TEST_CASE(ArgParser_SplitAt_CaseInsensitive_Default) {
  ArgParser p("select * FROM table");
  ArgParser pre(""), post("");

  // Default is case_sensitive = false
  bool found = p.SplitAt(pre, post, "from");

  BOOST_TEST(found);
  BOOST_TEST(pre.command == "select");
  BOOST_TEST(pre.size() == 1);          // "*"
  BOOST_TEST(post.command == "table");  // Token after FROM
}

BOOST_AUTO_TEST_CASE(ArgParser_SplitAt_CaseSensitive_Failure) {
  ArgParser p("select * FROM table");
  ArgParser pre(""), post("");

  // Explicitly request case sensitive
  bool found = p.SplitAt(pre, post, "from", true);

  BOOST_TEST(!found);
}

BOOST_AUTO_TEST_CASE(ArgParser_SplitAt_CaseSensitive_Success) {
  ArgParser p("select * FROM table");
  ArgParser pre(""), post("");

  bool found = p.SplitAt(pre, post, "FROM", true);

  BOOST_TEST(found);
  BOOST_TEST(post.command == "table");
}

BOOST_AUTO_TEST_CASE(ArgParser_SplitAt_DelimiterAtEnd) {
  // "cmd arg |" -> Delimiter is last. Post should be empty.
  ArgParser p("ls -al |");
  ArgParser pre(""), post("");

  bool found = p.SplitAt(pre, post, "|");

  BOOST_TEST(found);

  // Pre ok
  BOOST_TEST(pre.command == "ls");
  BOOST_TEST(pre.size() == 1);
  BOOST_TEST(pre.arguments[0].value == "-al");

  // Post empty
  BOOST_TEST(post.command == "");
  BOOST_TEST(post.empty());
}

BOOST_AUTO_TEST_CASE(ArgParser_SplitAt_DelimiterAtStart) {
  // "cmd | arg" -> Delimiter is first arg.
  // Note: "cmd" is the command, arguments are ["|", "arg"]
  ArgParser p("cmd | arg");
  ArgParser pre(""), post("");

  bool found = p.SplitAt(pre, post, "|");

  BOOST_TEST(found);

  // Pre has command, but arguments stopped before index 0
  BOOST_TEST(pre.command == "cmd");
  BOOST_TEST(pre.empty());

  // Post starts after index 0.
  // Token at index 1 ("arg") becomes Post command.
  BOOST_TEST(post.command == "arg");
  BOOST_TEST(post.empty());
}

BOOST_AUTO_TEST_CASE(ArgParser_Flatten_Basic) {
  ArgParser p("cmd arg1 arg2");
  BOOST_TEST(p.Flatten() == "cmd arg1 arg2");
}

BOOST_AUTO_TEST_CASE(ArgParser_Flatten_Quoted) {
  ArgParser p("echo \"hello world\"");
  // Should wrap in quotes
  BOOST_TEST(p.Flatten() == "echo \"hello world\"");
}

BOOST_AUTO_TEST_CASE(ArgParser_Flatten_QuotedEscape) {
  // Input: print "say \"hello\""
  // Parsed value: say "hello"
  // Flattened should escape internal quotes: print "say \"hello\""
  ArgParser p("print \"say \\\"hello\\\"\"");
  BOOST_TEST(p.Flatten() == "print \"say \\\"hello\\\"\"");
}

BOOST_AUTO_TEST_CASE(ArgParser_Flatten_Parenthesized) {
  ArgParser p("math (1 + 2)");
  // Should wrap in parens
  BOOST_TEST(p.Flatten() == "math (1 + 2)");
}

BOOST_AUTO_TEST_CASE(ArgParser_Flatten_Mixed) {
  // cmd "quote" basic (paren)
  ArgParser p("cmd \"quote\" basic (paren)");
  BOOST_TEST(p.Flatten() == "cmd \"quote\" basic (paren)");
}

BOOST_AUTO_TEST_CASE(ArgParser_Flatten_EmptyArgs) {
  ArgParser p("justcommand");
  BOOST_TEST(p.Flatten() == "justcommand");
}

BOOST_AUTO_TEST_CASE(ArgParser_Flatten_ExplicitEmptyCommand) {
  ArgParser p("", std::vector<std::string>{"cmd", "arg1", "arg2"});
  BOOST_TEST(p.Flatten() == "cmd arg1 arg2");
}

BOOST_AUTO_TEST_CASE(ArgParser_Parse_Int_OutOfBounds) {
  ArgParser p("cmd 123");
  int result;
  auto type = p.Parse(1, result);  // Index 1 is out of bounds
  BOOST_TEST((type == ArgParser::ArgType::NOT_FOUND));
}

BOOST_AUTO_TEST_CASE(ArgParser_Parse_Bool_OutOfBounds) {
  ArgParser p("cmd true");
  bool result;
  auto type = p.Parse(1, result);  // Index 1 is out of bounds
  BOOST_TEST((type == ArgParser::ArgType::NOT_FOUND));
}

BOOST_AUTO_TEST_CASE(ArgParser_Parse_SharedPtrExpressionParser) {
  ArgParser p("cmd (1 + 2)");
  auto mock = std::make_shared<MockExpressionParser>();
  uint32_t result = 0;

  auto type = p.Parse(0, result, mock);

  BOOST_TEST((type == ArgParser::ArgType::PARENTHESIZED));
  BOOST_TEST(result == 3);
}

BOOST_AUTO_TEST_CASE(ArgParser_Parse_SharedPtrExpressionParser_Null) {
  ArgParser p("cmd 123");
  std::shared_ptr<MockExpressionParser> mock = nullptr;
  uint32_t result = 0;

  auto type = p.Parse(0, result, mock);

  BOOST_TEST((type == ArgParser::ArgType::BASIC));
  BOOST_TEST(result == 123);
}

BOOST_AUTO_TEST_CASE(ArgParser_Iterator_Operators) {
  ArgParser p("cmd one two three");
  auto it = p.begin();

  // operator->
  BOOST_TEST(it->size() == 3);  // "one".size()

  // Post-increment
  auto it_copy = it++;
  BOOST_TEST(*it_copy == "one");
  BOOST_TEST(*it == "two");

  // Pre-decrement
  --it;
  BOOST_TEST(*it == "one");

  // Post-decrement
  it++;  // back to "two"
  auto it_copy2 = it--;
  BOOST_TEST(*it_copy2 == "two");
  BOOST_TEST(*it == "one");

  // operator<
  BOOST_TEST((p.begin() < p.end()));
  BOOST_TEST(!(p.end() < p.begin()));

  // operator-(difference_type)
  auto it_end = p.end();
  auto it_prev = it_end - 1;
  BOOST_TEST(*it_prev == "three");

  // operator[]
  BOOST_TEST(p.begin()[0] == "one");
  BOOST_TEST(p.begin()[1] == "two");
}

BOOST_AUTO_TEST_CASE(ArgParser_ExtractSubcommand_NoArgs) {
  ArgParser p("cmd");
  auto maybe_sub = p.ExtractSubcommand();
  BOOST_TEST(!maybe_sub);
}

BOOST_AUTO_TEST_SUITE_END()
