#include <boost/test/unit_test.hpp>

#include "util/parsing.h"

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

/*
 *
 template <typename T>
bool MaybeParseHexInt(T &ret, const std::vector<uint8_t> &data,
                      size_t offset = 0) {

 template <typename T>
bool MaybeParseHexInt(T &ret, const std::string &data) {
 */

BOOST_AUTO_TEST_SUITE_END()
