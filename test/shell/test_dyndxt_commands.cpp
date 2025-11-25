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

#include "test_util/mock_xbdm_server/mock_xbdm_client_transport.h"

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

BOOST_FIXTURE_TEST_SUITE(BootstrapTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(BootstrapResumesIfTargetWasRunning) {
  std::stringstream capture;
  DynDXTCommandLoadBootstrap cmd;
  auto debugger_interface =
      std::dynamic_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(debugger_interface);
  BOOST_REQUIRE(debugger_interface->AttachDebugger());

  // Mock 'stop' response to indicate success (target was running)
  server->SetCommandHandler("stop",
                            [](ClientTransport& client, const std::string&) {
                              client.Send("200- stopped\r\n");
                              return true;
                            });
  server->AddThread("main", 1);
  server->SetCommandHandler("halt",
                            [](ClientTransport& client, const std::string&) {
                              client.Send("200- halted\r\n");
                              return true;
                            });
  bool go_called = false;
  server->SetCommandHandler("go",
                            [&](ClientTransport& client, const std::string&) {
                              go_called = true;
                              client.Send("200- running\r\n");
                              return true;
                            });
  server->SetExecutionState(ExecutionState::S_STOPPED);

  cmd(*interface, empty_args, capture);

  BOOST_CHECK(go_called);
}

DEBUGGER_TEST_CASE(BootstrapDoesNotResumeIfTargetWasStopped) {
  std::stringstream capture;
  DynDXTCommandLoadBootstrap cmd;
  auto debugger_interface =
      std::dynamic_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(debugger_interface);
  BOOST_REQUIRE(debugger_interface->AttachDebugger());

  // Mock 'stop' response to indicate soft failure (target was already stopped)
  server->SetCommandHandler("stop",
                            [](ClientTransport& client, const std::string&) {
                              client.Send("400- Already stopped\r\n");
                              return true;
                            });

  server->AddThread("main", 1);
  server->SetCommandHandler("halt",
                            [](ClientTransport& client, const std::string&) {
                              client.Send("200- halted\r\n");
                              return true;
                            });
  bool go_called = false;
  server->SetCommandHandler("go",
                            [&](ClientTransport& client, const std::string&) {
                              go_called = true;
                              client.Send("200- running\r\n");
                              return true;
                            });
  server->SetExecutionState(ExecutionState::S_STOPPED);

  cmd(*interface, empty_args, capture);

  if (go_called) {
    BOOST_TEST_MESSAGE("Capture output: " << capture.str());
  }
  BOOST_CHECK(!go_called);
}

BOOST_AUTO_TEST_SUITE_END()
