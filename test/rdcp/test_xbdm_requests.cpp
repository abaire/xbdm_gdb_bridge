#include <boost/test/unit_test.hpp>
#include <string>

#include "rdcp/xbdm_requests.h"
#include "test_util/vector.h"

static void CompleteRequest(RDCPProcessedRequest &request, StatusCode status) {
  request.Complete(std::make_shared<RDCPResponse>(status, "<NO MESSAGE>"));
}

static void CompleteRequest(RDCPProcessedRequest &request, StatusCode status,
                            const std::string &message) {
  request.Complete(std::make_shared<RDCPResponse>(status, message));
}

static void CompleteRequest(RDCPProcessedRequest &request, StatusCode status,
                            std::string message, std::vector<char> data) {
  request.Complete(std::make_shared<RDCPResponse>(status, std::move(message),
                                                  std::move(data)));
}

static void CompleteRequest(RDCPProcessedRequest &request, StatusCode status,
                            std::string message,
                            const std::map<std::string, std::string> &data) {
  std::vector<char> buffer = Serialize(data);
  request.Complete(std::make_shared<RDCPResponse>(status, std::move(message),
                                                  std::move(buffer)));
}

BOOST_AUTO_TEST_SUITE(altaddr)

BOOST_AUTO_TEST_CASE(altaddr_ok) {
  AltAddr request;
  std::map<std::string, std::string> data;

  const char address_string[] = "127.0.0.1";
  uint32_t address = inet_network(address_string);

  char value[32] = {0};
  snprintf(value, 31, "0x%x", address);
  data["addr"] = value;
  CompleteRequest(request, StatusCode::OK, "", data);

  BOOST_TEST(request.IsOK());
  BOOST_TEST(request.address_string == address_string);
  BOOST_TEST(request.address == 0x0100007F);
}

BOOST_AUTO_TEST_CASE(altaddr_fail) {
  AltAddr request;
  CompleteRequest(request, StatusCode::ERR_UNEXPECTED);
  BOOST_TEST(!request.IsOK());
}

BOOST_AUTO_TEST_SUITE_END()
