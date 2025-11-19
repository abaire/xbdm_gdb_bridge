#include "shell/shell.h"

#include <boost/test/unit_test.hpp>

#include "xbox/xbox_interface.h"

BOOST_AUTO_TEST_SUITE(ShellTests)

BOOST_AUTO_TEST_CASE(TestTokenizeSimple) {
  std::shared_ptr<XBOXInterface> interface;
  Shell shell(interface);
  std::vector<std::string> tokens = shell.Tokenize("a b c");
  BOOST_REQUIRE_EQUAL(3, tokens.size());
  BOOST_CHECK_EQUAL("a", tokens[0]);
  BOOST_CHECK_EQUAL("b", tokens[1]);
  BOOST_CHECK_EQUAL("c", tokens[2]);
}

BOOST_AUTO_TEST_CASE(TestTokenizeEmpty) {
  std::shared_ptr<XBOXInterface> interface;
  Shell shell(interface);
  std::vector<std::string> tokens = shell.Tokenize("");
  BOOST_REQUIRE_EQUAL(0, tokens.size());
}

BOOST_AUTO_TEST_CASE(TestTokenizeQuoted) {
  std::shared_ptr<XBOXInterface> interface;
  Shell shell(interface);
  std::vector<std::string> tokens = shell.Tokenize("a \"b c\" d");
  BOOST_REQUIRE_EQUAL(3, tokens.size());
  BOOST_CHECK_EQUAL("a", tokens[0]);
  BOOST_CHECK_EQUAL("b c", tokens[1]);
  BOOST_CHECK_EQUAL("d", tokens[2]);
}

BOOST_AUTO_TEST_CASE(TestTokenizeEscapedQuote) {
  std::shared_ptr<XBOXInterface> interface;
  Shell shell(interface);
  std::vector<std::string> tokens = shell.Tokenize("a \"b ~\" c\" d");
  BOOST_REQUIRE_EQUAL(3, tokens.size());
  BOOST_CHECK_EQUAL("a", tokens[0]);
  BOOST_CHECK_EQUAL("b \" c", tokens[1]);
  BOOST_CHECK_EQUAL("d", tokens[2]);
}

BOOST_AUTO_TEST_CASE(TestTokenizeMultipleQuoted) {
  std::shared_ptr<XBOXInterface> interface;
  Shell shell(interface);
  std::vector<std::string> tokens = shell.Tokenize("a \"b c\" d \"e f g\" h");
  BOOST_REQUIRE_EQUAL(5, tokens.size());
  BOOST_CHECK_EQUAL("a", tokens[0]);
  BOOST_CHECK_EQUAL("b c", tokens[1]);
  BOOST_CHECK_EQUAL("d", tokens[2]);
  BOOST_CHECK_EQUAL("e f g", tokens[3]);
  BOOST_CHECK_EQUAL("h", tokens[4]);
}

BOOST_AUTO_TEST_SUITE_END()
