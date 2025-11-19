#include <boost/test/unit_test.hpp>
#include <chrono>
#include <memory>

#include "configure_test.h"
#include "net/select_thread.h"
#include "test_util/mock_xbdm_server/mock_xbdm_server.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"

using namespace xbdm_gdb_bridge;
using namespace xbdm_gdb_bridge::testing;
using namespace std::chrono_literals;

struct XBDMDebuggerFixture {
  XBDMDebuggerFixture() {
    server = std::make_unique<MockXBDMServer>(TEST_MOCK_XBDM_PORT);
    BOOST_REQUIRE(server->Start());

    select_thread_ = std::make_shared<SelectThread>("ST_ClntFixture");
    context_ = std::make_shared<XBDMContext>("Client", server->GetAddress(),
                                             select_thread_);
    select_thread_->Start();

    debugger = std::make_unique<XBDMDebugger>(context_);
  }

  ~XBDMDebuggerFixture() {
    if (debugger) {
      debugger->Shutdown();
      debugger.reset();
    }

    server->Stop();
    server.reset();

    select_thread_->Stop();
    select_thread_.reset();
    context_.reset();
  }

  void Connect() const {
    BOOST_REQUIRE(debugger->Attach());
    BOOST_REQUIRE(debugger->IsAttached());
  }

  std::unique_ptr<MockXBDMServer> server;
  std::unique_ptr<XBDMDebugger> debugger;
  uint16_t port = 0;

  // Generally these should not be modified, but are visible for specific
  // test scenarios.
  std::shared_ptr<XBDMContext> context_;
  std::shared_ptr<SelectThread> select_thread_;
};

#define DEBUGGER_TEST_CASE(__name) \
  BOOST_AUTO_TEST_CASE(__name, *boost::unit_test::timeout(TEST_TIMEOUT_SECONDS))

// ============================================================================
// Connection Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(ConnectionTests, XBDMDebuggerFixture)

DEBUGGER_TEST_CASE(ConnectToValidServerSucceeds) {
  BOOST_REQUIRE(debugger->Attach());
  BOOST_CHECK(debugger->IsAttached());

  debugger->Shutdown();
  BOOST_CHECK(!debugger->IsAttached());
}

DEBUGGER_TEST_CASE(ConnectToInvalidPortFails) {
  IPAddress addr("127.0.0.1:1");
  auto bad_ctx = std::make_shared<XBDMContext>("Client", addr, select_thread_);

  debugger = std::make_unique<XBDMDebugger>(bad_ctx);
  BOOST_CHECK(!debugger->Attach());
  BOOST_CHECK(!debugger->IsAttached());
}

DEBUGGER_TEST_CASE(ReconnectAfterDisconnectSucceeds) {
  BOOST_REQUIRE(debugger->Attach());
  debugger->Shutdown();
  BOOST_CHECK(!debugger->IsAttached());

  BOOST_REQUIRE(debugger->Attach());
  BOOST_CHECK(debugger->IsAttached());
}

BOOST_AUTO_TEST_SUITE_END()
