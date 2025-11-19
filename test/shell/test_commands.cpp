#include <boost/test/unit_test.hpp>
#include <chrono>

#include "configure_test.h"
#include "shell/commands.h"
#include "shell/debugger_commands.h"
#include "test_util/mock_xbdm_server/mock_server_debugger_interface_fixture.h"
#include "test_util/mock_xbdm_server/mock_xbdm_server.h"
#include "xbox/debugger/debugger_xbox_interface.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"

using namespace xbdm_gdb_bridge;
using namespace xbdm_gdb_bridge::testing;
using namespace std::chrono_literals;

#define DEBUGGER_TEST_CASE(__name) \
  BOOST_AUTO_TEST_CASE(__name, *boost::unit_test::timeout(TEST_TIMEOUT_SECONDS))

// GetMem

BOOST_FIXTURE_TEST_SUITE(GetMemTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(GetMemWithNoAddressFails) {
  std::stringstream capture;
  CommandGetMem cmd;
  BOOST_REQUIRE(cmd(*interface, empty_args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture), "Missing required address argument.");
}

DEBUGGER_TEST_CASE(GetMemWithNoSizeFails) {
  std::stringstream capture;
  CommandGetMem cmd;
  std::vector<std::string> args{"0x12345"};
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture), "Missing required size argument.");
}

DEBUGGER_TEST_CASE(GetMemSucceeds) {
  std::vector<uint8_t> data{
      0xFF, 0xEE, 0x44, 0x11, 0x22, 0x33, 0x88, 0x99,
      0x01, 0x02, 0x03, 0x04, 0xA0, 0xA1, 0xA2, 0xA3,
  };
  server->AddRegion(0x12345, data);

  std::stringstream capture;
  CommandGetMem cmd;
  std::vector<std::string> args{"0x12345", "16"};
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture),
                    "ff ee 44 11 22 33 88 99 01 02 03 04 a0 a1 a2 a3 ");
}

BOOST_AUTO_TEST_SUITE_END()
