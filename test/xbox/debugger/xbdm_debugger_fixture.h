#ifndef XBDM_DEBUGGER_FIXTURE_H
#define XBDM_DEBUGGER_FIXTURE_H

#include <boost/test/unit_test.hpp>
#include <condition_variable>
#include <memory>

#include "configure_test.h"
#include "net/select_thread.h"
#include "test_util/mock_xbdm_server/mock_xbdm_server.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"

using namespace xbdm_gdb_bridge;
using namespace xbdm_gdb_bridge::testing;

struct XBDMDebuggerFixture {
  XBDMDebuggerFixture() {
    server = std::make_unique<MockXBDMServer>(TEST_MOCK_XBDM_PORT);
    server->AddExecutionStateCallback(S_STARTED, [this]() {
      execution_state_condition_variable_.notify_all();
    });

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

  void AwaitQuiescence() const {
    // Ping pong the peer SelectThreads to avoid a situation where one generates
    // new work for the other after a period of quiescence.
    for (int i = 0; i < 4; ++i) {
      server->AwaitQuiescence();
      select_thread_->AwaitQuiescence();
    }
  }

  void Bootup(uint32_t max_wait_milliseconds = 10000) {
    server->SimulateBootToDashboard();
    BOOST_REQUIRE(AwaitState(S_STARTED, max_wait_milliseconds));
  }

  void RebootSync(uint32_t max_wait_milliseconds = 10000) {
    server->SimulateReboot();
    BOOST_REQUIRE(AwaitState(S_STARTED, max_wait_milliseconds));
  }

  bool AwaitState(ExecutionState state,
                  uint32_t max_wait_milliseconds = 10000) {
    std::unique_lock lock(execution_state_mutex_);
    return execution_state_condition_variable_.wait_for(
        lock, std::chrono::milliseconds(max_wait_milliseconds),
        [this, state]() { return server->GetExecutionState() == state; });
  }

  std::unique_ptr<MockXBDMServer> server;
  std::unique_ptr<XBDMDebugger> debugger;
  uint16_t port = 0;

  std::shared_ptr<XBDMContext> context_;
  std::shared_ptr<SelectThread> select_thread_;

 private:
  std::mutex execution_state_mutex_;
  std::condition_variable execution_state_condition_variable_;
};

#endif  // XBDM_DEBUGGER_FIXTURE_H
