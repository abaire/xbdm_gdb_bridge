#include <boost/test/unit_test.hpp>
#include <memory>

#include "configure_test.h"
#include "net/select_thread.h"
#include "test_util/mock_xbdm_server/mock_xbdm_server.h"
#include "xbox/debugger/thread.h"
#include "xbox/xbdm_context.h"

using namespace xbdm_gdb_bridge;
using namespace xbdm_gdb_bridge::testing;
using namespace std::chrono_literals;

static constexpr uint32_t TRAP_FLAG = 0x100;

struct ThreadTestFixture {
  ThreadTestFixture() {
    server = std::make_unique<MockXBDMServer>(TEST_MOCK_XBDM_PORT);
    BOOST_REQUIRE(server->Start());

    select_thread_ = std::make_shared<SelectThread>("ST_ThreadTest");
    context_ = std::make_shared<XBDMContext>("Client", server->GetAddress(),
                                             select_thread_);
    select_thread_->Start();
    BOOST_REQUIRE(context_->Reconnect());
  }

  ~ThreadTestFixture() {
    if (context_) {
      context_->Shutdown();
    }
    server->Stop();
    select_thread_->Stop();
  }

  std::unique_ptr<MockXBDMServer> server;
  std::shared_ptr<XBDMContext> context_;
  std::shared_ptr<SelectThread> select_thread_;
};

#define THREAD_TEST_CASE(__name) \
  BOOST_AUTO_TEST_CASE(__name, *boost::unit_test::timeout(TEST_TIMEOUT_SECONDS))

BOOST_FIXTURE_TEST_SUITE(ThreadTests, ThreadTestFixture)

THREAD_TEST_CASE(StepInstructionSetsTrapFlagAndContinuesWithException) {
  uint32_t thread_id = server->AddThread("TestThread");
  Thread thread(thread_id);

  bool continue_received = false;
  bool set_context_sets_trap_flag = false;

  server->SetCommandHandler(
      "continue", [&](ClientTransport& client, const std::string& params) {
        continue_received = true;
        server->SendResponse(client, OK);
        return true;
      });

  server->SetCommandHandler("setcontext", [&](ClientTransport& client,
                                              const std::string& parameters) {
    RDCPMapResponse params(parameters);
    auto target = params.GetOptionalDWORD("thread");
    BOOST_REQUIRE(target && *target == thread_id);
    auto flags = params.GetOptionalDWORD("eflags");
    BOOST_REQUIRE(flags && (*flags) & TRAP_FLAG);

    set_context_sets_trap_flag = true;
    server->SendResponse(client, OK);
    return true;
  });

  BOOST_CHECK(thread.StepInstruction(*context_));
  server->AwaitQuiescence();

  BOOST_CHECK(continue_received);
  BOOST_CHECK(set_context_sets_trap_flag);
}

BOOST_AUTO_TEST_SUITE_END()
