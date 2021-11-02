#include <boost/test/unit_test.hpp>

#include "rdcp/rdcp_processed_request.h"
#include "test_util/vector.h"

static void AppendLines(std::vector<char> &data, const char **lines,
                        const char **end) {
  while (lines != end) {
    data += *lines;
    if (++lines != end) {
      data.insert(data.end(), RDCPResponse::kTerminator,
                  std::end(RDCPResponse::kTerminator));
    }
  }
}

BOOST_AUTO_TEST_SUITE(multiline_response_suite)

BOOST_AUTO_TEST_CASE(empty_data_returns_empty_list) {
  std::vector<char> data;
  RDCPMultilineResponse response(data);
  BOOST_TEST(response.lines.empty());
}

BOOST_AUTO_TEST_CASE(single_line_data_returns_single_line) {
  const char test_data[] = "test";
  std::vector<char> data(test_data, std::end(test_data));

  RDCPMultilineResponse response(data);
  BOOST_TEST(response.lines.size() == 1);

  auto const &first_line = response.lines.front();
  BOOST_TEST(std::equal(first_line.begin(), first_line.end(), test_data));
}

BOOST_AUTO_TEST_CASE(empty_terminated_line_returns_empty_lines) {
  std::vector<char> data(RDCPResponse::kTerminator,
                         std::end(RDCPResponse::kTerminator));

  RDCPMultilineResponse response(data);
  BOOST_TEST(response.lines.size() == 2);

  int line = 1;
  for (auto it = response.lines.begin(); it != response.lines.end();
       ++it, ++line) {
    char message[16] = {0};
    snprintf(message, 15, "Line: %d", line);
    BOOST_TEST(std::equal(it->begin(), it->end(), ""), message);
  }
}

BOOST_AUTO_TEST_CASE(multiple_lines_returns_multiple_lines) {
  const char *lines[] = {
      "First line",
      "Second line",
  };

  std::vector<char> data;
  AppendLines(data, lines, std::end(lines));

  RDCPMultilineResponse response(data);
  BOOST_TEST(response.lines.size() == std::size(lines));

  int line = 0;
  for (auto it = response.lines.begin(); it != response.lines.end();
       ++it, ++line) {
    char message[16] = {0};
    snprintf(message, 15, "Line: %d", line + 1);
    BOOST_TEST(std::equal(it->begin(), it->end(), lines[line]), message);
  }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(map_response_suite)

BOOST_AUTO_TEST_CASE(empty_data_returns_empty_map) {
  std::vector<char> data;
  RDCPMapResponse response(data);
  BOOST_TEST(response.map.empty());
}

BOOST_AUTO_TEST_CASE(single_valueless_key) {
  const char test_data[] = "test";
  std::vector<char> data(test_data, std::end(test_data) - 1);

  RDCPMapResponse response(data);
  BOOST_TEST(response.map.size() == 1);
  BOOST_TEST(response.HasKey("test"));
}

BOOST_AUTO_TEST_CASE(single_string_key) {
  const char test_data[] = "test=value";
  std::vector<char> data(test_data, std::end(test_data) - 1);

  RDCPMapResponse response(data);
  BOOST_TEST(response.map.size() == 1);
  std::string value = response.GetString("test");
  BOOST_TEST(value == "value");
}

BOOST_AUTO_TEST_CASE(single_decimal_key) {
  const char test_data[] = "test=123";
  std::vector<char> data(test_data, std::end(test_data) - 1);

  RDCPMapResponse response(data);
  BOOST_TEST(response.map.size() == 1);
  auto value = response.GetDWORD("test");
  BOOST_TEST(value == 123);
}

BOOST_AUTO_TEST_CASE(single_hex_key) {
  const char test_data[] = "test=0x3DA2";
  std::vector<char> data(test_data, std::end(test_data) - 1);

  RDCPMapResponse response(data);
  BOOST_TEST(response.map.size() == 1);
  auto value = response.GetDWORD("test");
  BOOST_TEST(value == 0x3DA2);
}

BOOST_AUTO_TEST_CASE(multiple_keys) {
  const char test_data[] =
      "string=test flag quoted=\"quoted string\" decimal=123456 hex=0x3DA2 "
      "last_flag";
  std::vector<char> data(test_data, std::end(test_data) - 1);

  RDCPMapResponse response(data);
  BOOST_TEST(response.map.size() == 6);
  BOOST_TEST(response.GetDWORD("hex") == 0x3DA2);
  BOOST_TEST(response.GetDWORD("decimal") == 123456);
  BOOST_TEST(response.HasKey("flag"));
  BOOST_TEST(response.HasKey("last_flag"));
  BOOST_TEST(response.GetString("string") == "test");
  BOOST_TEST(response.GetString("quoted") == "quoted string");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(multimap_response_suite)

BOOST_AUTO_TEST_CASE(empty_data_returns_empty_map) {
  std::vector<char> data;
  RDCPMultiMapResponse response(data);
  BOOST_TEST(response.maps.empty());
}

BOOST_AUTO_TEST_CASE(single_valueless_key) {
  const char test_data[] = "test";
  std::vector<char> data(test_data, std::end(test_data) - 1);

  RDCPMultiMapResponse response(data);
  BOOST_TEST(response.maps.size() == 1);
  BOOST_TEST(response.maps.front().HasKey("test"));
}

BOOST_AUTO_TEST_CASE(multi_maps) {
  const char *lines[] = {
      "test",
      "hex=0xABCD flag quoted=\"quoted string\"",
  };
  std::vector<char> data;
  AppendLines(data, lines, std::end(lines));

  RDCPMultiMapResponse response(data);
  BOOST_TEST(response.maps.size() == 2);
  BOOST_TEST(response.maps.front().HasKey("test"));
  BOOST_TEST(response.maps.back().HasKey("flag"));
  BOOST_TEST(response.maps.back().GetDWORD("hex") == 0xABCD);
  BOOST_TEST(response.maps.back().GetString("quoted") == "quoted string");
}

BOOST_AUTO_TEST_SUITE_END()
