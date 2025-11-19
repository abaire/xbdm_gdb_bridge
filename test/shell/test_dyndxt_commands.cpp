#include <boost/test/unit_test.hpp>
#include <chrono>
#include <memory>

#include "configure_test.h"
#include "net/select_thread.h"
#include "shell/dyndxt_commands.h"
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

BOOST_FIXTURE_TEST_SUITE(InvokeSimpleTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(InvokeSimpleWithNoCommandFails) {
  std::stringstream capture;
  DynDXTCommandInvokeSimple cmd;
  BOOST_REQUIRE(cmd(*interface, empty_args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture),
                    "Missing required `processor!command` argument.");
}

DEBUGGER_TEST_CASE(InvokeSimpleBuiltInWithNoArgumentsSucceeds) {
  std::stringstream capture;
  DynDXTCommandInvokeSimple cmd;
  ArgParser args("invokesimple", std::vector<std::string>{"threads"});
  cmd(*interface, args, capture);

  BOOST_CHECK_EQUAL(Trimmed(capture), "threads: 202 thread list follows");
}

DEBUGGER_TEST_CASE(InvokeSimpleBuiltInWithArgumentSucceeds) {
  std::stringstream capture;
  DynDXTCommandInvokeSimple cmd;
  ArgParser args("invokesimple",
                 std::vector<std::string>{"debugger", "disconnect"});

  cmd(*interface, args, capture);

  BOOST_CHECK_EQUAL(Trimmed(capture), "debugger: 200 OK");
}

BOOST_AUTO_TEST_SUITE_END()
