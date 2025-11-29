#include <boost/test/unit_test.hpp>
#include <chrono>
#include <memory>
#include <thread>

#include "configure_test.h"
#include "net/select_thread.h"
#include "test_util/mock_xbdm_server/mock_xbdm_server.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"

using namespace xbdm_gdb_bridge;
using namespace xbdm_gdb_bridge::testing;
using namespace std::chrono_literals;

struct XBDMDebuggerThreadingFixture {
  XBDMDebuggerThreadingFixture() {
    server = std::make_unique<MockXBDMServer>(TEST_MOCK_XBDM_PORT);
    BOOST_REQUIRE(server->Start());

    select_thread_ = std::make_shared<SelectThread>("ST_ClntFixture");
    context_ = std::make_shared<XBDMContext>("Client", server->GetAddress(),
                                             select_thread_);
    select_thread_->Start();

    debugger = std::make_unique<XBDMDebugger>(context_);
  }

  ~XBDMDebuggerThreadingFixture() {
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

  std::shared_ptr<XBDMContext> context_;
  std::shared_ptr<SelectThread> select_thread_;
};

BOOST_FIXTURE_TEST_SUITE(ThreadingTests, XBDMDebuggerThreadingFixture)

BOOST_AUTO_TEST_CASE(LazyThreadResolution) {
  // Add a second thread.
  // Thread 1 (ID 1) is created by default.
  // Thread 2 (ID 2).
  server->AddThread("Thread2");

  Connect();

  // Ensure both threads are known.
  auto threads = debugger->Threads();
  BOOST_REQUIRE_EQUAL(threads.size(), 2);

  // Set Thread 2 as stopped.
  // We use SetThreadStopped to update internal state, then SetExecutionState to
  // trigger notification. This avoids sending the "break" notification which
  // would cause OnBreakpoint to eagerly set the active thread.
  server->SetThreadStopped(2, true);
  server->SetExecutionState(ExecutionState::S_STOPPED);

  // Wait for state change.
  BOOST_REQUIRE(debugger->WaitForStateIn({ExecutionState::S_STOPPED}, 1000));

  // Check behavior before ActiveThread() is called.
  // With lazy loading, active_thread_id_ should be unset (invalid).
  // AnyThreadID() returns the active thread if set, or the first thread if not.
  // Since it's unset, it should return Thread 1 (the first thread).
  auto any_thread = debugger->AnyThreadID();
  BOOST_REQUIRE(any_thread.has_value());
  BOOST_CHECK_EQUAL(*any_thread, 1);

  // Now call ActiveThread() to trigger lazy load.
  // This should find the stopped thread (Thread 2) and set it as active.
  auto active_thread = debugger->ActiveThread();
  BOOST_REQUIRE(active_thread != nullptr);
  BOOST_CHECK_EQUAL(active_thread->thread_id, 2);

  // Now AnyThreadID should return 2, as it is now the active thread.
  any_thread = debugger->AnyThreadID();
  BOOST_REQUIRE(any_thread.has_value());
  BOOST_CHECK_EQUAL(*any_thread, 2);
}

BOOST_AUTO_TEST_SUITE_END()
