#include <boost/test/unit_test.hpp>

#include "util/parsing.h"

BOOST_AUTO_TEST_SUITE(cli_tokenizer_suite)

BOOST_AUTO_TEST_CASE(EmptyInputReturnsEmptyVector) {
  std::vector<std::string> input = {};
  auto result = command_line_command_tokenizer::SplitCommands(input);

  BOOST_TEST(result.empty());
}

BOOST_AUTO_TEST_CASE(NoDelimiterReturnsSingleVector) {
  std::vector<std::string> input = {"ls", "-la", "/home"};
  auto result = command_line_command_tokenizer::SplitCommands(input);

  BOOST_TEST(result.size() == 1);
  BOOST_TEST(result[0] == input);
}

BOOST_AUTO_TEST_CASE(SimpleSplitOnDelimiter) {
  std::vector<std::string> input = {"echo", "hello", "&&", "echo", "world"};
  auto result = command_line_command_tokenizer::SplitCommands(input);

  BOOST_TEST(result.size() == 2);

  std::vector<std::string> expected_first = {"echo", "hello"};
  std::vector<std::string> expected_second = {"echo", "world"};

  BOOST_TEST(result[0] == expected_first);
  BOOST_TEST(result[1] == expected_second);
}

BOOST_AUTO_TEST_CASE(MultipleDelimiters) {
  std::vector<std::string> input = {"A", "&&", "B", "&&", "C"};
  auto result = command_line_command_tokenizer::SplitCommands(input);

  BOOST_TEST(result.size() == 3);
  BOOST_TEST(result[0] == std::vector<std::string>{"A"});
  BOOST_TEST(result[1] == std::vector<std::string>{"B"});
  BOOST_TEST(result[2] == std::vector<std::string>{"C"});
}

BOOST_AUTO_TEST_CASE(LeadingDelimiterCreatesEmptyFirstCommand) {
  // If "&&" is first, 'cmd' is empty.
  std::vector<std::string> input = {"&&", "ls"};
  auto result = command_line_command_tokenizer::SplitCommands(input);

  BOOST_TEST(result.size() == 2);
  BOOST_TEST(result[0].empty(),
             "First element should be empty because && was leading");
  BOOST_TEST(result[1] == std::vector<std::string>{"ls"});
}

BOOST_AUTO_TEST_CASE(TrailingDelimiterCreatesEmptyLastCommand) {
  // Your code pushes the remaining 'cmd' after the loop.
  // If "&&" is last, the loop clears 'cmd', and the final push adds an empty
  // vector.
  std::vector<std::string> input = {"ls", "&&"};
  auto result = command_line_command_tokenizer::SplitCommands(input);

  BOOST_TEST(result.size() == 2);
  BOOST_TEST(result[0] == std::vector<std::string>{"ls"});
  BOOST_TEST(result[1].empty(),
             "Last element should be empty because && was trailing");
}

BOOST_AUTO_TEST_CASE(ConsecutiveDelimitersCreateEmptyMiddleCommand) {
  std::vector<std::string> input = {"A", "&&", "&&", "B"};
  auto result = command_line_command_tokenizer::SplitCommands(input);

  BOOST_TEST(result.size() == 3);
  BOOST_TEST(result[0] == std::vector<std::string>{"A"});
  BOOST_TEST(result[1].empty(),
             "Middle element should be empty due to double &&");
  BOOST_TEST(result[2] == std::vector<std::string>{"B"});
}

BOOST_AUTO_TEST_SUITE_END()
