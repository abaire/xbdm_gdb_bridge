#include <boost/test/unit_test.hpp>
#include <chrono>
#include <memory>

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

// ============================================================================
// ConnectionTests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(ConnectionTests, XBDMDebuggerFixture)

DEBUGGER_TEST_CASE(ConnectToValidServerSucceeds) {
  Bootup();
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
  Bootup();
  BOOST_REQUIRE(debugger->Attach());
  debugger->Shutdown();
  BOOST_CHECK(!debugger->IsAttached());

  BOOST_REQUIRE(debugger->Attach());
  BOOST_CHECK(debugger->IsAttached());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// BreakpointConditionTests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(BreakpointConditionTests, XBDMDebuggerFixture)

DEBUGGER_TEST_CASE(SetAndRetrieveCondition) {
  const uint32_t kAddress = 0x80001000;
  const std::string kCondition = "$eax == 0";
  const auto kType = XBDMDebugger::BreakpointType::BREAKPOINT;

  // Ensure empty initially
  BOOST_CHECK(!debugger->FindBreakpointCondition(kType, kAddress));

  // Set and Verify
  debugger->SetBreakpointCondition(kType, kAddress, kCondition);
  auto result = debugger->FindBreakpointCondition(kType, kAddress);

  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(*result, kCondition);
}

DEBUGGER_TEST_CASE(UpdateExistingCondition) {
  const uint32_t kAddress = 0x80002000;
  const auto kType = XBDMDebugger::BreakpointType::READ_WATCH;

  debugger->SetBreakpointCondition(kType, kAddress, "old_condition");
  debugger->SetBreakpointCondition(kType, kAddress, "new_condition");

  auto result = debugger->FindBreakpointCondition(kType, kAddress);
  BOOST_REQUIRE(result.has_value());
  BOOST_CHECK_EQUAL(*result, "new_condition");
}

DEBUGGER_TEST_CASE(RemoveCondition) {
  const uint32_t kAddress = 0x80003000;
  const auto kType = XBDMDebugger::BreakpointType::WRITE_WATCH;

  debugger->SetBreakpointCondition(kType, kAddress, "condition");
  BOOST_REQUIRE(debugger->FindBreakpointCondition(kType, kAddress).has_value());

  debugger->RemoveBreakpointCondition(kType, kAddress);
  BOOST_CHECK(!debugger->FindBreakpointCondition(kType, kAddress).has_value());
}

DEBUGGER_TEST_CASE(ConditionsAreIsolatedByAddressAndType) {
  const uint32_t kAddrA = 0x1000;
  const uint32_t kAddrB = 0x2000;

  // Same Type, Different Address
  debugger->SetBreakpointCondition(XBDMDebugger::BreakpointType::BREAKPOINT,
                                   kAddrA, "condA");
  debugger->SetBreakpointCondition(XBDMDebugger::BreakpointType::BREAKPOINT,
                                   kAddrB, "condB");

  // Same Address, Different Type
  debugger->SetBreakpointCondition(XBDMDebugger::BreakpointType::EXECUTE_WATCH,
                                   kAddrA, "condExec");

  // Verify A
  auto resA = debugger->FindBreakpointCondition(
      XBDMDebugger::BreakpointType::BREAKPOINT, kAddrA);
  BOOST_TEST((resA && *resA == "condA"));

  // Verify B
  auto resB = debugger->FindBreakpointCondition(
      XBDMDebugger::BreakpointType::BREAKPOINT, kAddrB);
  BOOST_TEST((resB && *resB == "condB"));

  // Verify Exec (Same address as A)
  auto resExec = debugger->FindBreakpointCondition(
      XBDMDebugger::BreakpointType::EXECUTE_WATCH, kAddrA);
  BOOST_TEST((resExec && *resExec == "condExec"));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// GuessBackTraceTests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(GuessBackTraceTests, XBDMDebuggerFixture)

DEBUGGER_TEST_CASE(GuessBackTraceFindsCalls) {
  Bootup();
  BOOST_REQUIRE(debugger->Attach());

  const uint32_t kStackBase = 0xD0001000;
  const uint32_t kStackLimit = 0xD0000000;
  const uint32_t kTextBase = 0x00010000;
  const uint32_t kTextSize = 0x1000;

  uint32_t thread_id = server->AddThread("TestThread", kTextBase, kStackBase,
                                         kTextBase, kStackLimit);
  server->SetThreadRegister(thread_id, "esp", kStackLimit);

  server->AddModule("default.xbe", kTextBase, kTextSize);
  server->AddXbeSection("default.xbe", ".text", kTextBase, kTextSize, 1);
  server->AddRegion(kTextBase, kTextSize);

  // 3. Setup Stack Memory
  // We will place 3 values on the stack:
  // [ESP]     = Valid Return Address (points to .text, preceded by CALL)
  // [ESP + 4] = Invalid Return Address (points to .text, NO CALL)
  // [ESP + 8] = Invalid Address (outside .text)

  uint32_t valid_ret_addr = kTextBase + 0x100;
  uint32_t invalid_ret_addr_no_call = kTextBase + 0x200;
  uint32_t invalid_addr_outside = kTextBase + kTextSize + 0x100;

  std::vector<uint8_t> stack_data(12);
  // Helper to write uint32 to vector
  auto write_u32 = [](std::vector<uint8_t>& data, size_t offset, uint32_t val) {
    data[offset] = val & 0xFF;
    data[offset + 1] = (val >> 8) & 0xFF;
    data[offset + 2] = (val >> 16) & 0xFF;
    data[offset + 3] = (val >> 24) & 0xFF;
  };

  write_u32(stack_data, 0, valid_ret_addr);
  write_u32(stack_data, 4, invalid_ret_addr_no_call);
  write_u32(stack_data, 8, invalid_addr_outside);

  server->AddRegion(kStackLimit, stack_data);

  // 4. Setup .text content
  // Valid Call: E8 xx xx xx xx at valid_ret_addr - 5
  std::vector<uint8_t> text_data(kTextSize, 0x90);  // NOPs
  uint32_t call_offset = valid_ret_addr - kTextBase - 5;
  text_data[call_offset] = 0xE8;  // CALL relative
  // The operand doesn't matter for the heuristic, just the opcode

  // Invalid Call: Just NOPs at invalid_ret_addr_no_call - 5

  server->SetMemoryRegion(kTextBase, text_data);

  // 5. Run GuessBackTrace
  BOOST_REQUIRE(debugger->FetchModules());
  BOOST_REQUIRE(debugger->FetchThreads());
  auto frames = debugger->GuessBackTrace(thread_id);

  // 6. Verify
  BOOST_REQUIRE_EQUAL(frames.size(), 1);
  BOOST_CHECK_EQUAL(frames[0], valid_ret_addr);
}

BOOST_AUTO_TEST_SUITE_END()
