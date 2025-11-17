#include <boost/test/unit_test.hpp>
#include <chrono>
#include <memory>

#include "mock_xbdm_server/mock_xbdm_server.h"
#include "net/select_thread.h"
#include "shell/dyndxt_commands.h"
#include "xbox/debugger/debugger_xbox_interface.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"

using namespace xbdm_gdb_bridge;
using namespace xbdm_gdb_bridge::testing;
using namespace std::chrono_literals;

struct XBDMDebuggerFixture {
  XBDMDebuggerFixture() {
    server = std::make_unique<MockXBDMServer>(0);
    BOOST_REQUIRE(server->Start());

    interface =
        std::make_shared<DebuggerXBOXInterface>("Client", server->GetAddress());
    interface->Start();
  }

  ~XBDMDebuggerFixture() {
    if (interface) {
      interface->Stop();
      interface.reset();
    }

    server->Stop();
    server.reset();
  }

  std::shared_ptr<XBOXInterface> interface;
  std::unique_ptr<MockXBDMServer> server;
  uint16_t port = 0;

  const std::vector<std::string> empty_args;
};

namespace {
std::string trimmed(const std::stringstream& captured) {
  auto s = captured.str();
  if (s.empty()) {
    return s;
  }

  // Check for \n
  if (s.back() == '\n') {
    s.pop_back();
    // If it was \r\n, remove the \r as well
    if (!s.empty() && s.back() == '\r') {
      s.pop_back();
    }
  }
  // Handle the \r-only case
  else if (s.back() == '\r') {
    s.pop_back();
  }
  return s;
}

}  // namespace

#define DEBUGGER_TEST_CASE(__name) \
  BOOST_AUTO_TEST_CASE(__name, *boost::unit_test::timeout(2))

// ============================================================================
// Connection Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(ConnectionTests, XBDMDebuggerFixture)

DEBUGGER_TEST_CASE(InvokeSimpleWithNoCommandFails) {
  std::stringstream capture;
  DynDXTCommandInvokeSimple cmd;
  BOOST_REQUIRE(cmd(*interface, empty_args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(trimmed(capture),
                    "Missing required `processor!command` argument.");
}

DEBUGGER_TEST_CASE(InvokeSimpleBuiltInWithNoArgumentsSucceeds) {
  std::stringstream capture;
  DynDXTCommandInvokeSimple cmd;
  cmd(*interface, {"threads"}, capture);

  BOOST_CHECK_EQUAL(trimmed(capture), "threads: 202 thread list follows");
}

DEBUGGER_TEST_CASE(InvokeSimpleBuiltInWithArgumentSucceeds) {
  std::stringstream capture;
  DynDXTCommandInvokeSimple cmd;
  cmd(*interface, {"debugger", "disconnect"}, capture);

  BOOST_CHECK_EQUAL(trimmed(capture), "debugger: 200 OK");
}

BOOST_AUTO_TEST_SUITE_END()
