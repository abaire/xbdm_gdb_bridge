#include "rdcp/rdcp_processed_request.h"

#define BOOST_TEST_MODULE TestRDCPProcessedRequest
#include <boost/test/unit_test.hpp>

#include "test_util/vector.h"

static void AppendLines(std::vector<char> &data, const char **lines, const char **end) {
  while (lines != end) {
    data += *lines;
    if (++lines != end) {
      data.insert(data.end(), RDCPResponse::kTerminator, std::end(RDCPResponse::kTerminator));
    }
  }
}

BOOST_AUTO_TEST_SUITE(multiline_response_suite)

BOOST_AUTO_TEST_CASE(empty_data_returns_empty_list)
{
  std::vector<char> data;
  RDCPMultilineResponse response(data);
  BOOST_TEST(response.lines.empty());
}

BOOST_AUTO_TEST_CASE(single_line_data_returns_single_line)
{
  const char test_data[] = "test";
  std::vector<char> data(test_data, std::end(test_data));

  RDCPMultilineResponse response(data);
  BOOST_TEST(response.lines.size() == 1);

  auto const &first_line = response.lines.front();
  BOOST_TEST(std::equal(first_line.begin(), first_line.end(), test_data));
}

BOOST_AUTO_TEST_CASE(empty_terminated_line_returns_empty_lines)
{
  std::vector<char> data(RDCPResponse::kTerminator, std::end(RDCPResponse::kTerminator));

  RDCPMultilineResponse response(data);
  BOOST_TEST(response.lines.size() == 2);

  int line = 1;
  for (auto it = response.lines.begin(); it != response.lines.end(); ++it, ++line) {
    char message[16] = {0};
    snprintf(message, 15, "Line: %d", line);
    BOOST_TEST(std::equal(it->begin(), it->end(), ""), message);
  }
}

BOOST_AUTO_TEST_CASE(multiple_lines_returns_multiple_lines)
{
  const char *lines[] = {
      "First line",
      "Second line",
  };

  std::vector<char> data;
  AppendLines(data, lines, std::end(lines));

  RDCPMultilineResponse response(data);
  BOOST_TEST(response.lines.size() == std::size(lines));

  int line = 0;
  for (auto it = response.lines.begin(); it != response.lines.end(); ++it, ++line) {
    char message[16] = {0};
    snprintf(message, 15, "Line: %d", line + 1);
    BOOST_TEST(std::equal(it->begin(), it->end(), lines[line]), message);
  }
}


BOOST_AUTO_TEST_SUITE_END()
