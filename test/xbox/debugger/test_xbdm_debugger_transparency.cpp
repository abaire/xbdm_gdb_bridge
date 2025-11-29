#include <algorithm>
#include <atomic>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "configure_test.h"
#include "net/select_thread.h"
#include "rdcp/xbdm_requests.h"
#include "test_util/mock_xbdm_server/mock_xbdm_server.h"
#include "util/logging.h"
#include "xbdm_debugger_fixture.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"

using namespace xbdm_gdb_bridge;
using namespace xbdm_gdb_bridge::testing;
using namespace std::chrono_literals;

#define DEBUGGER_TEST_CASE(__name) \
  BOOST_AUTO_TEST_CASE(__name, *boost::unit_test::timeout(TEST_TIMEOUT_SECONDS))

BOOST_FIXTURE_TEST_SUITE(TransparencyTests, XBDMDebuggerFixture)

DEBUGGER_TEST_CASE(GetMemoryRemovesAndRestoresBreakpoints) {
  Bootup();
  Connect();

  std::vector<uint8_t> mem_data = {0x90, 0x90, 0x90, 0x90};
  server->AddRegion(0x1000, mem_data);
  BOOST_REQUIRE(debugger->AddBreakpoint(0x1000));
  AwaitQuiescence();

  std::vector<std::string> command_history;
  server->SetAfterCommandHandler("break", [&](const std::string& params) {
    command_history.push_back("break " + params);
  });
  server->SetAfterCommandHandler("getmem2", [&](const std::string& params) {
    command_history.push_back("getmem2 " + params);
  });
  auto memory = debugger->GetMemory(0x1000, 4);

  BOOST_REQUIRE(memory.has_value());
  BOOST_CHECK_EQUAL(memory->at(0), 0x90);

  // Expect:
  // 1. break addr=0x1000 clear
  // 2. getmem2 addr=0x1000 length=4
  // 3. break addr=0x1000
  bool found_remove = false;
  bool found_getmem = false;
  bool found_restore = false;

  auto check = [](const std::string& buffer, const std::string& substring) {
    return boost::algorithm::to_lower_copy(buffer).find(substring) !=
           std::string::npos;
  };

  for (const auto& cmd : command_history) {
    if (!found_remove) {
      found_remove = check(cmd, "break") && check(cmd, "addr=0x00001000") &&
                     check(cmd, "clear");
      continue;
    }

    if (!found_getmem) {
      BOOST_REQUIRE(!check(cmd, "break") && "extra break found before getmem2");
      found_getmem = check(cmd, "getmem2") && check(cmd, "addr=0x00001000");
      continue;
    }

    found_restore = check(cmd, "break") && check(cmd, "addr=0x00001000") &&
                    !check(cmd, "clear");
    if (found_restore) {
      break;
    }
  }

  BOOST_CHECK(found_remove);
  BOOST_CHECK(found_getmem);
  BOOST_CHECK(found_restore);
}

DEBUGGER_TEST_CASE(RebootClearsBreakpoints) {
  Bootup();
  Connect();

  static constexpr uint32_t kBreakAddr = 0x2000;
  BOOST_REQUIRE(debugger->AddBreakpoint(kBreakAddr));
  AwaitQuiescence();

  RebootSync();
  std::vector<std::string> command_history;
  server->SetAfterCommandHandler("break", [&](const std::string& params) {
    command_history.push_back("break " + params);
  });

  debugger->GetMemory(kBreakAddr, 4);
  AwaitQuiescence();

  for (const auto& cmd : command_history) {
    if (cmd.find("break") != std::string::npos) {
      BOOST_FAIL("Should not have sent any break commands after reboot: " +
                 cmd);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()
