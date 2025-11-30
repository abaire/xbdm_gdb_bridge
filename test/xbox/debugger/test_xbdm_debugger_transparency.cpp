#include <boost/test/unit_test.hpp>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "configure_test.h"
#include "net/select_thread.h"
#include "test_util/mock_xbdm_server/mock_xbdm_server.h"
#include "xbdm_debugger_fixture.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"

using namespace xbdm_gdb_bridge;
using namespace xbdm_gdb_bridge::testing;
using namespace std::chrono_literals;

#define DEBUGGER_TEST_CASE(__name) \
  BOOST_AUTO_TEST_CASE(__name, *boost::unit_test::timeout(TEST_TIMEOUT_SECONDS))

BOOST_FIXTURE_TEST_SUITE(TransparentSteppingTests, XBDMDebuggerFixture)

void MockCommands(MockXBDMServer* server) {
  server->SetCommandHandler(
      "stop", [server](ClientTransport& client, const std::string&) {
        server->SendResponse(client, StatusCode::OK);
        // Force state to stopped so subsequent commands like GetContext work if
        // they check state
        server->SetExecutionState(ExecutionState::S_STOPPED);
        return true;
      });

  server->SetCommandHandler(
      "setcontext", [server](ClientTransport& client, const std::string&) {
        server->SendResponse(client, StatusCode::OK);
        return true;
      });
}

DEBUGGER_TEST_CASE(StepOverBreakpointTemporarilyClearsIt) {
  MockCommands(server.get());

  Bootup();
  Connect();

  const uint32_t kAddress = 0x80001000;
  uint32_t thread_id = server->AddThread("Thread1", kAddress);

  BOOST_REQUIRE(debugger->FetchThreads());
  BOOST_REQUIRE(debugger->SetActiveThread(thread_id));

  BOOST_REQUIRE(debugger->AddBreakpoint(kAddress));

  BOOST_CHECK(server->HasBreakpoint(kAddress));

  // Ensure we are in a stopped state.
  BOOST_REQUIRE(debugger->Stop());

  BOOST_REQUIRE(debugger->StepInstruction());

  // Verify breakpoint is cleared on server (transparently suspended).
  // Because StepInstruction suspends it, then calls Go() (which is
  // asynchronous), and we haven't received S_STOPPED yet, the breakpoint should
  // still be cleared on the target.
  BOOST_CHECK(!server->HasBreakpoint(kAddress));

  // Now simulate the single step completion by transitioning to S_STOPPED.
  // This should trigger OnExecutionStateChanged -> RestoreBreakpoints.
  server->SetExecutionState(ExecutionState::S_STOPPED);

  bool restored = false;
  for (int i = 0; i < 20; ++i) {
    if (server->HasBreakpoint(kAddress)) {
      restored = true;
      break;
    }
    std::this_thread::sleep_for(50ms);
  }

  BOOST_CHECK_MESSAGE(
      restored, "Breakpoint should be restored after S_STOPPED notification");
}

DEBUGGER_TEST_CASE(StepOverNonBreakpointDoesNotClear) {
  MockCommands(server.get());

  Bootup();
  Connect();

  const uint32_t kAddress = 0x80001000;
  uint32_t thread_id = server->AddThread("Thread1", kAddress);

  BOOST_REQUIRE(debugger->FetchThreads());
  BOOST_REQUIRE(debugger->SetActiveThread(thread_id));

  const uint32_t kOtherAddress = 0x80002000;
  BOOST_REQUIRE(debugger->AddBreakpoint(kOtherAddress));

  BOOST_CHECK(server->HasBreakpoint(kOtherAddress));

  BOOST_REQUIRE(debugger->Stop());
  BOOST_REQUIRE(debugger->StepInstruction());

  // Breakpoint should NOT be cleared because it didn't overlap EIP.
  BOOST_CHECK(server->HasBreakpoint(kOtherAddress));

  server->SetExecutionState(ExecutionState::S_STOPPED);

  // Give a little time for things to settle before destruction
  std::this_thread::sleep_for(100ms);
}

BOOST_AUTO_TEST_SUITE_END()
