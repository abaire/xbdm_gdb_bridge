#include <boost/test/unit_test.hpp>
#include <chrono>
#include <memory>

#include "configure_test.h"
#include "net/select_thread.h"
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

// RunTests

BOOST_FIXTURE_TEST_SUITE(RunTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(RunWithNoPathFails) {
  std::stringstream capture;
  DebuggerCommandRun cmd;
  BOOST_REQUIRE(cmd(*interface, empty_args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture), "Missing required path argument.");
}

DEBUGGER_TEST_CASE(RunWithInvalidPathFails) {
  server->SetCommandHandler(
      "title", [this](ClientTransport& client, const std::string& params) {
        server->SendResponse(client, ERR_ACCESS_DENIED);
        return true;
      });

  std::stringstream capture;
  DebuggerCommandRun cmd;
  ArgParser args("run", std::vector<std::string>{"e:\\test.xbe"});
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture), "Failed to launch XBE");
}

DEBUGGER_TEST_CASE(RunWithValidPathSucceeds) {
  std::stringstream capture;
  DebuggerCommandRun cmd;
  ArgParser args("run", std::vector<std::string>{"e:\\test.xbe"});
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture), "");
}

BOOST_AUTO_TEST_SUITE_END()

// LaunchWaitTests

BOOST_FIXTURE_TEST_SUITE(LaunchWaitTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(LaunchWaitWithNoPathFails) {
  std::stringstream capture;
  DebuggerCommandLaunchWait cmd;
  BOOST_REQUIRE(cmd(*interface, empty_args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture), "Missing required path argument.");
}

DEBUGGER_TEST_CASE(LaunchWaitWithInvalidPathFails) {
  server->SetCommandHandler(
      "title", [this](ClientTransport& client, const std::string& params) {
        server->SendResponse(client, ERR_ACCESS_DENIED);
        return true;
      });

  std::stringstream capture;
  DebuggerCommandLaunchWait cmd;
  ArgParser args("/launchwait", std::vector<std::string>{"e:\\test.xbe"});
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture), "Failed to launch XBE");
}

DEBUGGER_TEST_CASE(LaunchWaitWithValidPathSucceeds) {
  server->AddModule("test.exe", 0x80000000, 0x10000);
  server->AddXbeSection(".test", 0x1000, 100, 0);
  server->AddRegion(0x00010000, 0x00001000, 0x00000004);
  server->AddRegion(0x80000000, 0x10000, 2);

  server->SetExecutionStateCallback(S_STARTED, [this]() {
    server->SimulateExecutionBreakpoint(0x80000000, 1);
  });

  server->SetAfterCommandHandler("stopon", [this](const std::string&) {
    server->SetExecutionState(S_STOPPED);
  });

  std::stringstream capture;
  DebuggerCommandLaunchWait cmd;
  ArgParser args("/launchwait", std::vector<std::string>{"e:\\test.xbe"});
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture), "");
}

BOOST_AUTO_TEST_SUITE_END()
