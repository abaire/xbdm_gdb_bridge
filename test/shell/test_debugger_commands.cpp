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
  AwaitQuiescence();

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
  AwaitQuiescence();

  BOOST_CHECK_EQUAL(Trimmed(capture), "Failed to launch XBE");
}

DEBUGGER_TEST_CASE(RunWithValidPathSucceeds) {
  std::stringstream capture;
  DebuggerCommandRun cmd;
  ArgParser args("run", std::vector<std::string>{"e:\\test.xbe"});
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();

  BOOST_CHECK_EQUAL(Trimmed(capture), "");
}

BOOST_AUTO_TEST_SUITE_END()

// LaunchWaitTests

BOOST_FIXTURE_TEST_SUITE(LaunchWaitTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(LaunchWaitWithNoPathFails) {
  std::stringstream capture;
  DebuggerCommandLaunchWait cmd;
  BOOST_REQUIRE(cmd(*interface, empty_args, capture) == Command::HANDLED);
  AwaitQuiescence();

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
  AwaitQuiescence();

  BOOST_CHECK_EQUAL(Trimmed(capture), "Failed to launch XBE");
}

DEBUGGER_TEST_CASE(LaunchWaitWithValidPathSucceeds) {
  server->AddModule("test.exe", 0x80000000, 0x10000);
  server->AddXbeSection(".test", 0x1000, 100, 0);
  server->AddRegion(0x00010000, 0x00001000, 0x00000004);
  server->AddRegion(0x80000000, 0x10000, 2);

  server->AddExecutionStateCallback(S_STARTED, [this]() {
    server->SimulateExecutionBreakpoint(0x80000000, 1);
  });

  server->SetAfterCommandHandler("stopon", [this](const std::string&) {
    server->SetExecutionState(S_STOPPED);
  });

  std::stringstream capture;
  DebuggerCommandLaunchWait cmd;
  ArgParser args("/launchwait", std::vector<std::string>{"e:\\test.xbe"});
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();

  BOOST_CHECK_EQUAL(Trimmed(capture), "");
}

BOOST_AUTO_TEST_SUITE_END()

// GetThreadsTests

BOOST_FIXTURE_TEST_SUITE(GetThreadsTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(GetThreadsWithThreads) {
  auto debugger_interface =
      std::static_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(debugger_interface->AttachDebugger());
  server->AddThread("1", 0x1234, 0x4567, 0x89AB);
  server->AddThread("2", 0x2222, 0x2000, 0x2200);
  server->AddThread("3", 0x3333, 0x3000, 0x3300);

  std::stringstream capture;
  DebuggerCommandGetThreads cmd;
  BOOST_REQUIRE(cmd(*interface, empty_args, capture) == Command::HANDLED);
  AwaitQuiescence();

  std::string expected =
      "Thread 1\n"
      "Priority 9\n"
      "Suspend count 0\n"
      "Base:  0xd0000000\n"
      "Start:  0x00060000\n"
      "Thread local base:  0xd0001000\n"
      "Limit:  0xd0200000\n"
      "\n"
      "Thread 2\n"
      "Priority 9\n"
      "Suspend count 0\n"
      "Base:  0x00004567\n"
      "Start:  0x000089ab\n"
      "Thread local base:  0xd0001000\n"
      "Limit:  0xd0200000\n"
      "\n"
      "Thread 3\n"
      "Priority 9\n"
      "Suspend count 0\n"
      "Base:  0x00002000\n"
      "Start:  0x00002200\n"
      "Thread local base:  0xd0001000\n"
      "Limit:  0xd0200000\n"
      "\n"
      "Thread 4\n"
      "Priority 9\n"
      "Suspend count 0\n"
      "Base:  0x00003000\n"
      "Start:  0x00003300\n"
      "Thread local base:  0xd0001000\n"
      "Limit:  0xd0200000\n";
  BOOST_CHECK_EQUAL(Trimmed(capture), expected);
}

DEBUGGER_TEST_CASE(GetThreadsWithActiveThread) {
  auto debugger_interface =
      std::static_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(debugger_interface->AttachDebugger());
  server->AddThread("Something", 0x1234, 0x4567, 0x89AB);
  uint32_t active_tid = server->AddThread("Active", 0x2222, 0x2000, 0x2200);
  server->AddThread("AnotherThread", 0x3333, 0x3000, 0x3300);

  server->SimulateExecutionBreakpoint(0x1000, active_tid);
  AwaitQuiescence();
  auto debugger = debugger_interface->Debugger();
  debugger->FetchThreads();
  debugger->SetActiveThread(active_tid);

  std::stringstream capture;
  DebuggerCommandGetThreads cmd;
  BOOST_REQUIRE(cmd(*interface, empty_args, capture) == Command::HANDLED);
  AwaitQuiescence();

  std::string expected =
      "Thread 1\n"
      "Priority 9\n"
      "Suspend count 0\n"
      "Base:  0xd0000000\n"
      "Start:  0x00060000\n"
      "Thread local base:  0xd0001000\n"
      "Limit:  0xd0200000\n"
      "\n"
      "Thread 2\n"
      "Priority 9\n"
      "Suspend count 0\n"
      "Base:  0x00004567\n"
      "Start:  0x000089ab\n"
      "Thread local base:  0xd0001000\n"
      "Limit:  0xd0200000\n"
      "\n"
      "[Active thread]\n"
      "Thread 3\n"
      "Priority 9\n"
      "Suspend count 0\n"
      "Base:  0x00002000\n"
      "Start:  0x00002200\n"
      "Thread local base:  0xd0001000\n"
      "Limit:  0xd0200000\n"
      "\n"
      "Thread 4\n"
      "Priority 9\n"
      "Suspend count 0\n"
      "Base:  0x00003000\n"
      "Start:  0x00003300\n"
      "Thread local base:  0xd0001000\n"
      "Limit:  0xd0200000\n";
  BOOST_CHECK_EQUAL(Trimmed(capture), expected);
}

BOOST_AUTO_TEST_SUITE_END()
