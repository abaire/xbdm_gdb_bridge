#include <boost/test/unit_test.hpp>
#include <chrono>

#include "configure_test.h"
#include "shell/commands.h"
#include "test_util/mock_xbdm_server/mock_server_debugger_interface_fixture.h"

using namespace xbdm_gdb_bridge;
using namespace xbdm_gdb_bridge::testing;

#define DEBUGGER_TEST_CASE(__name) \
  BOOST_AUTO_TEST_CASE(__name, *boost::unit_test::timeout(TEST_TIMEOUT_SECONDS))

BOOST_FIXTURE_TEST_SUITE(GetMemRenderTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(GetMemDefaultRender) {
  std::vector<uint8_t> data{0x01, 0x02, 0x03, 0x04};
  server->AddRegion(0x1000, data);
  std::stringstream capture;
  CommandGetMem cmd;
  ArgParser args("getmem 0x1000 4");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK_EQUAL(Trimmed(capture), "01 02 03 04 ");
}

DEBUGGER_TEST_CASE(GetMemExplicitByteRender) {
  std::vector<uint8_t> data{0x01, 0x02, 0x03, 0x04};
  server->AddRegion(0x1000, data);

  {
    std::stringstream capture;
    CommandGetMem cmd;
    ArgParser args("getmem 0x1000 4 b");
    BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
    BOOST_CHECK_EQUAL(Trimmed(capture), "01 02 03 04 ");
  }

  {
    std::stringstream capture;
    CommandGetMem cmd;
    ArgParser args("getmem 0x1000 4 byte");
    BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
    BOOST_CHECK_EQUAL(Trimmed(capture), "01 02 03 04 ");
  }
}

DEBUGGER_TEST_CASE(GetMemWordRender) {
  std::vector<uint8_t> data{0x01, 0x02, 0x03, 0x04};
  server->AddRegion(0x1000, data);

  {
    std::stringstream capture;
    CommandGetMem cmd;
    ArgParser args("getmem 0x1000 4 w");
    BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
    BOOST_CHECK_EQUAL(Trimmed(capture), "0201 0403 ");
  }

  {
    std::stringstream capture;
    CommandGetMem cmd;
    ArgParser args("getmem 0x1000 4 word");
    BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
    BOOST_CHECK_EQUAL(Trimmed(capture), "0201 0403 ");
  }
}

DEBUGGER_TEST_CASE(GetMemWordRenderUnaligned) {
  std::vector<uint8_t> data{0x01, 0x02, 0x03};
  server->AddRegion(0x1000, data);

  std::stringstream capture;
  CommandGetMem cmd;
  ArgParser args("getmem 0x1000 3 w");
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK_EQUAL(Trimmed(capture), "0201 03 ");
}

DEBUGGER_TEST_CASE(GetMemDwordRender) {
  std::vector<uint8_t> data{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  server->AddRegion(0x1000, data);

  {
    std::stringstream capture;
    CommandGetMem cmd;
    ArgParser args("getmem 0x1000 8 d");
    BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
    BOOST_CHECK_EQUAL(Trimmed(capture), "04030201 08070605 ");
  }

  {
    std::stringstream capture;
    CommandGetMem cmd;
    ArgParser args("getmem 0x1000 8 dword");
    BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
    BOOST_CHECK_EQUAL(Trimmed(capture), "04030201 08070605 ");
  }
}

DEBUGGER_TEST_CASE(GetMemDwordRenderUnaligned) {
  std::vector<uint8_t> data{0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
  server->AddRegion(0x1000, data);

  std::stringstream capture;
  CommandGetMem cmd;
  ArgParser args("getmem 0x1000 6 d");
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK_EQUAL(Trimmed(capture), "04030201 0605 ");
}

DEBUGGER_TEST_CASE(GetMemInvalidRender) {
  std::stringstream capture;
  CommandGetMem cmd;
  ArgParser args("getmem 0x1000 4 invalid");
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK_EQUAL(Trimmed(capture), "Invalid render mode invalid");
}

BOOST_AUTO_TEST_SUITE_END()
