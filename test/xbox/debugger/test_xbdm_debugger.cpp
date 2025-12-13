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

namespace {

void WriteInt(std::vector<uint8_t>& data, size_t offset, uint32_t val) {
  if (offset + 4 > data.size()) {
    return;
  }
  data[offset] = val & 0xFF;
  data[offset + 1] = (val >> 8) & 0xFF;
  data[offset + 2] = (val >> 16) & 0xFF;
  data[offset + 3] = (val >> 24) & 0xFF;
}

void DefineCall(std::vector<uint8_t>& text_data, uint32_t text_base,
                uint32_t ret_addr, uint32_t call_target) {
  const uint32_t call_instruction_addr = ret_addr - text_base - 5;
  if (call_instruction_addr + 5 > text_data.size()) {
    return;
  }
  text_data[call_instruction_addr] = 0xE8;
  WriteInt(text_data, call_instruction_addr + 1, call_target - ret_addr);
}

void DefineIndirectCall(std::vector<uint8_t>& text_data, uint32_t text_base,
                        uint32_t ret_addr) {
  const uint32_t call_instruction_addr = ret_addr - text_base - 2;
  if (call_instruction_addr + 2 > text_data.size()) {
    return;
  }
  text_data[call_instruction_addr] = 0xFF;
  text_data[call_instruction_addr + 1] = 0xD0;
}

}  // namespace

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

DEBUGGER_TEST_CASE(GuessBackTraceStopsAndResumesExecution) {
  Bootup();
  uint32_t tid = server->AddThread("test_thread");
  Connect();
  server->AwaitQuiescence();
  BOOST_REQUIRE(server->GetExecutionState() == S_STARTED);

  bool stopped_check = false;
  server->SetCommandHandler(
      "getcontext",
      [this, &stopped_check](ClientTransport& client, const std::string&) {
        BOOST_TEST(server->GetExecutionState() == S_STOPPED);
        stopped_check = true;
        server->SendResponse(client, ERR_UNEXPECTED, "Test Hook Rejection");
        return true;
      });

  debugger->GuessBackTrace(tid);
  server->AwaitQuiescence();

  BOOST_TEST(stopped_check);
  BOOST_TEST(server->GetExecutionState() == S_STARTED);
}

DEBUGGER_TEST_CASE(GuessBackTraceDoesNotResumeIfInitiallyStopped) {
  Bootup();
  uint32_t tid = server->AddThread("test_thread");
  Connect();

  // Stop the server explicitly
  {
    auto stop = std::make_shared<Stop>();
    context_->SendCommandSync(stop);
    BOOST_REQUIRE(stop->IsOK());
  }

  server->AwaitQuiescence();
  BOOST_REQUIRE(server->GetExecutionState() == S_STOPPED);

  bool stopped_check = false;
  server->SetCommandHandler(
      "getcontext",
      [this, &stopped_check](ClientTransport& client, const std::string&) {
        BOOST_TEST(server->GetExecutionState() == S_STOPPED);
        stopped_check = true;
        server->SendResponse(client, ERR_UNEXPECTED, "Test Hook Rejection");
        return true;
      });

  debugger->GuessBackTrace(tid);
  server->AwaitQuiescence();

  BOOST_TEST(stopped_check);
  BOOST_TEST(server->GetExecutionState() == S_STOPPED);
}

DEBUGGER_TEST_CASE(GuessBackTraceFindsCalls) {
  Bootup();
  BOOST_REQUIRE(debugger->Attach());

  constexpr uint32_t kStackBase = 0xD0001000;
  constexpr uint32_t kStackLimit = 0xD0000000;
  constexpr uint32_t kTextBase = 0x00010000;
  constexpr uint32_t kTextSize = 0x1000;
  constexpr uint32_t kFunctionStart = kTextBase + 0x20;
  constexpr uint32_t kCurrentEIP = kFunctionStart + 0x20;

  uint32_t thread_id = server->AddThread("TestThread", kTextBase, kStackBase,
                                         kTextBase, kStackLimit);
  server->SetThreadRegister(thread_id, "esp", kStackLimit);
  server->SetThreadRegister(thread_id, "eip", kCurrentEIP);
  server->AddModule("default.xbe", kTextBase, kTextSize);
  server->AddXbeSection("default.xbe", ".text", kTextBase, kTextSize, 1);
  server->AddRegion(kTextBase, kTextSize);
  uint32_t valid_ret_addr = kTextBase + 0x100;
  std::vector<uint8_t> stack_data(4);
  WriteInt(stack_data, 0, valid_ret_addr);
  server->AddRegion(kStackLimit, stack_data);
  // Valid Call: E8 xx xx xx xx at valid_ret_addr - 5
  std::vector<uint8_t> text_data(kTextSize, 0x90);  // NOPs
  DefineCall(text_data, kTextBase, valid_ret_addr, kFunctionStart);
  server->SetMemoryRegion(kTextBase, text_data);
  server->AwaitQuiescence();

  BOOST_REQUIRE(debugger->FetchModules());
  BOOST_REQUIRE(debugger->FetchThreads());
  auto frames = debugger->GuessBackTrace(thread_id);
  server->AwaitQuiescence();

  BOOST_REQUIRE_EQUAL(frames.size(), 1);
  BOOST_CHECK_EQUAL(frames[0].address, valid_ret_addr);
  BOOST_CHECK_EQUAL(frames[0].is_indirect_call, false);
  BOOST_CHECK(frames[0].call_target.has_value());
  BOOST_CHECK_EQUAL(*frames[0].call_target, kFunctionStart);
  BOOST_CHECK_EQUAL(frames[0].is_suspicious, false);
}

DEBUGGER_TEST_CASE(GuessBackTraceAnnotatesFarDirectCalls) {
  Bootup();
  BOOST_REQUIRE(debugger->Attach());

  constexpr uint32_t kStackBase = 0xD0001000;
  constexpr uint32_t kStackLimit = 0xD0000000;
  constexpr uint32_t kTextBase = 0x00010000;
  constexpr uint32_t kTextSize = 0x2000;  // Larger text to allow far calls
  uint32_t thread_id = server->AddThread("TestThread", kTextBase, kStackBase,
                                         kTextBase, kStackLimit);
  server->SetThreadRegister(thread_id, "esp", kStackLimit);
  // Set EIP far away from function start.
  constexpr uint32_t kFunctionStart = kTextBase + 0x20;
  constexpr uint32_t kCurrentEIP = kFunctionStart + 0x800;
  server->SetThreadRegister(thread_id, "eip", kCurrentEIP);
  server->AddModule("default.xbe", kTextBase, kTextSize);
  server->AddXbeSection("default.xbe", ".text", kTextBase, kTextSize, 1);
  server->AddRegion(kTextBase, kTextSize);
  std::vector<uint8_t> stack_data(4);
  constexpr uint32_t kFarRetAddr = kTextBase + 0x1000;
  WriteInt(stack_data, 0, kFarRetAddr);
  server->AddRegion(kStackLimit, stack_data);

  std::vector<uint8_t> text_data(kTextSize, 0x90);
  DefineCall(text_data, kTextBase, kFarRetAddr, kFunctionStart);
  server->SetMemoryRegion(kTextBase, text_data);
  server->AwaitQuiescence();

  BOOST_REQUIRE(debugger->FetchModules());
  BOOST_REQUIRE(debugger->FetchThreads());
  auto frames = debugger->GuessBackTrace(thread_id);
  server->AwaitQuiescence();

  // Should track the frame but mark it as suspicious
  BOOST_REQUIRE_EQUAL(frames.size(), 1);
  BOOST_CHECK_EQUAL(frames[0].address, kFarRetAddr);
  BOOST_CHECK_EQUAL(frames[0].is_indirect_call, false);
  BOOST_CHECK(frames[0].call_target.has_value());
  BOOST_CHECK_EQUAL(*frames[0].call_target, kFunctionStart);
  BOOST_CHECK_EQUAL(frames[0].is_suspicious, true);
}

DEBUGGER_TEST_CASE(GuessBackTraceAcceptsIndirectCalls) {
  Bootup();
  BOOST_REQUIRE(debugger->Attach());

  constexpr uint32_t kStackBase = 0xD0001000;
  constexpr uint32_t kStackLimit = 0xD0000000;
  constexpr uint32_t kTextBase = 0x00010000;
  constexpr uint32_t kTextSize = 0x1000;

  uint32_t thread_id = server->AddThread("TestThread", kTextBase, kStackBase,
                                         kTextBase, kStackLimit);
  server->SetThreadRegister(thread_id, "esp", kStackLimit);
  server->SetThreadRegister(thread_id, "eip", kTextBase + 0x50);
  server->AddModule("default.xbe", kTextBase, kTextSize);
  server->AddXbeSection("default.xbe", ".text", kTextBase, kTextSize, 1);
  server->AddRegion(kTextBase, kTextSize);

  constexpr uint32_t kRetAddr = kTextBase + 0x100;
  std::vector<uint8_t> stack_data(4);
  WriteInt(stack_data, 0, kRetAddr);
  server->AddRegion(kStackLimit, stack_data);

  // Text setup: Indirect Call (FF D0 -> call eax)
  // At ret_addr - 2.
  std::vector<uint8_t> text_data(kTextSize, 0x90);
  DefineIndirectCall(text_data, kTextBase, kRetAddr);
  server->SetMemoryRegion(kTextBase, text_data);
  server->AwaitQuiescence();

  BOOST_REQUIRE(debugger->FetchModules());
  BOOST_REQUIRE(debugger->FetchThreads());
  auto frames = debugger->GuessBackTrace(thread_id);
  server->AwaitQuiescence();

  BOOST_REQUIRE_EQUAL(frames.size(), 1);
  BOOST_CHECK_EQUAL(frames[0].address, kRetAddr);
  BOOST_CHECK_EQUAL(frames[0].is_indirect_call, true);
  BOOST_CHECK(!frames[0].call_target.has_value());
  BOOST_CHECK_EQUAL(frames[0].is_suspicious, false);
}

DEBUGGER_TEST_CASE(GuessBackTraceHandlesWeakMatches) {
  Bootup();
  BOOST_REQUIRE(debugger->Attach());

  constexpr uint32_t kStackBase = 0xD0001000;
  constexpr uint32_t kStackLimit = 0xD0000000;
  constexpr uint32_t kTextBase = 0x00010000;
  constexpr uint32_t kTextSize = 0x1000;
  constexpr uint32_t kFunctionStart = kTextBase + 0x20;
  constexpr uint32_t kCurrentEIP = kFunctionStart + 0x20;

  uint32_t thread_id = server->AddThread("TestThread", kTextBase, kStackBase,
                                         kTextBase, kStackLimit);
  server->SetThreadRegister(thread_id, "esp", kStackLimit);
  server->SetThreadRegister(thread_id, "eip", kCurrentEIP);
  server->AddModule("default.xbe", kTextBase, kTextSize);
  server->AddXbeSection("default.xbe", ".text", kTextBase, kTextSize, 1);
  server->AddRegion(kTextBase, kTextSize);

  // Stack Layout:
  // [ESP]   -> Valid Ret Addr (Targets kFunctionStart)
  // [ESP+4] -> Weak Ret Addr (Targets kUnrelatedFunction)
  // [ESP+8] -> Valid Ret Addr 2 (Targets instruction before Valid Ret Addr)

  constexpr uint32_t kValidRetAddr1 = kTextBase + 0x100;
  constexpr uint32_t kWeakRetAddr = kTextBase + 0x200;
  constexpr uint32_t kValidRetAddr2 = kTextBase + 0x300;
  constexpr uint32_t kUnrelatedFunction = kTextBase + 0x900;

  std::vector<uint8_t> stack_data(12);
  WriteInt(stack_data, 0, kValidRetAddr1);
  WriteInt(stack_data, 4, kWeakRetAddr);
  WriteInt(stack_data, 8, kValidRetAddr2);
  server->AddRegion(kStackLimit, stack_data);

  std::vector<uint8_t> text_data(kTextSize, 0x90);

  DefineCall(text_data, kTextBase, kValidRetAddr1, kFunctionStart);
  DefineCall(text_data, kTextBase, kWeakRetAddr, kUnrelatedFunction);
  // 10 is arbitrary, just needs to be something before the call
  DefineCall(text_data, kTextBase, kValidRetAddr2, kValidRetAddr1 - 10);

  server->SetMemoryRegion(kTextBase, text_data);
  server->AwaitQuiescence();

  BOOST_REQUIRE(debugger->FetchModules());
  BOOST_REQUIRE(debugger->FetchThreads());
  auto frames = debugger->GuessBackTrace(thread_id);
  server->AwaitQuiescence();

  BOOST_REQUIRE_EQUAL(frames.size(), 3);

  // Frame 0: Valid Ret Addr 1
  BOOST_CHECK_EQUAL(frames[0].address, kValidRetAddr1);
  BOOST_CHECK_EQUAL(frames[0].is_suspicious, false);

  // Frame 1: Weak Ret Addr
  BOOST_CHECK_EQUAL(frames[1].address, kWeakRetAddr);
  BOOST_CHECK_EQUAL(frames[1].is_suspicious, true);

  // Frame 2: Valid Ret Addr 2
  BOOST_CHECK_EQUAL(frames[2].address, kValidRetAddr2);
  BOOST_CHECK_EQUAL(frames[2].is_suspicious, false);
}

DEBUGGER_TEST_CASE(GuessBackTraceVerifiesIndirectCalls) {
  Bootup();
  BOOST_REQUIRE(debugger->Attach());

  constexpr uint32_t kStackBase = 0xD0001000;
  constexpr uint32_t kStackLimit = 0xD0000000;
  constexpr uint32_t kTextBase = 0x00010000;
  constexpr uint32_t kTextSize = 0x1000;
  constexpr uint32_t kFunctionStart = kTextBase + 0x20;
  constexpr uint32_t kCurrentEIP = kFunctionStart + 0x20;

  uint32_t thread_id = server->AddThread("TestThread", kTextBase, kStackBase,
                                         kTextBase, kStackLimit);
  server->SetThreadRegister(thread_id, "esp", kStackLimit);
  server->SetThreadRegister(thread_id, "eip", kCurrentEIP);
  server->AddModule("default.xbe", kTextBase, kTextSize);
  server->AddXbeSection("default.xbe", ".text", kTextBase, kTextSize, 1);
  server->AddRegion(kTextBase, kTextSize);

  uint32_t indirect_ret_addr = kTextBase + 0x100;
  uint32_t valid_ret_addr_2 = kTextBase + 0x200;

  std::vector<uint8_t> stack_data(8);
  WriteInt(stack_data, 0, indirect_ret_addr);
  WriteInt(stack_data, 4, valid_ret_addr_2);
  server->AddRegion(kStackLimit, stack_data);

  std::vector<uint8_t> text_data(kTextSize, 0x90);

  // Call 1: Indirect Call (FF D0 -> call eax) at indirect_ret_addr - 2
  DefineIndirectCall(text_data, kTextBase, indirect_ret_addr);

  // Call 2: Direct Call targeting the function containing the indirect call.
  // Say the indirect call is in a function starting at kTextBase + 0x80.
  uint32_t func_with_indirect = kTextBase + 0x80;
  DefineCall(text_data, kTextBase, valid_ret_addr_2, func_with_indirect);

  server->SetMemoryRegion(kTextBase, text_data);
  server->AwaitQuiescence();

  BOOST_REQUIRE(debugger->FetchModules());
  BOOST_REQUIRE(debugger->FetchThreads());
  auto frames = debugger->GuessBackTrace(thread_id);

  BOOST_REQUIRE_EQUAL(frames.size(), 2);
  // Frame 0: Indirect Call - Should be confirmed (not suspicious)
  BOOST_CHECK_EQUAL(frames[0].address, indirect_ret_addr);
  BOOST_CHECK_EQUAL(frames[0].is_indirect_call, true);
  BOOST_CHECK_EQUAL(frames[0].is_suspicious, false);

  // Frame 1: Direct Call - Confirmed Base
  BOOST_CHECK_EQUAL(frames[1].address, valid_ret_addr_2);
  BOOST_CHECK_EQUAL(frames[1].is_suspicious, false);
}

DEBUGGER_TEST_CASE(GuessBackTraceRejectsSpuriousIndirectCalls) {
  Bootup();
  BOOST_REQUIRE(debugger->Attach());

  constexpr uint32_t kStackBase = 0xD0001000;
  constexpr uint32_t kStackLimit = 0xD0000000;
  constexpr uint32_t kTextBase = 0x00010000;
  constexpr uint32_t kTextSize = 0x1000;
  constexpr uint32_t kFunctionStart = kTextBase + 0x20;
  constexpr uint32_t kCurrentEIP = kFunctionStart + 0x20;

  uint32_t thread_id = server->AddThread("TestThread", kTextBase, kStackBase,
                                         kTextBase, kStackLimit);
  server->SetThreadRegister(thread_id, "esp", kStackLimit);
  server->SetThreadRegister(thread_id, "eip", kCurrentEIP);
  server->AddModule("default.xbe", kTextBase, kTextSize);
  server->AddXbeSection("default.xbe", ".text", kTextBase, kTextSize, 1);
  server->AddRegion(kTextBase, kTextSize);

  uint32_t indirect_ret_addr = kTextBase + 0x100;
  uint32_t valid_ret_addr_2 = kTextBase + 0x200;

  std::vector<uint8_t> stack_data(8);
  WriteInt(stack_data, 0, indirect_ret_addr);
  WriteInt(stack_data, 4, valid_ret_addr_2);
  server->AddRegion(kStackLimit, stack_data);

  std::vector<uint8_t> text_data(kTextSize, 0x90);

  DefineIndirectCall(text_data, kTextBase, indirect_ret_addr);
  DefineCall(text_data, kTextBase, valid_ret_addr_2, kFunctionStart);

  server->SetMemoryRegion(kTextBase, text_data);
  server->AwaitQuiescence();

  BOOST_REQUIRE(debugger->FetchModules());
  BOOST_REQUIRE(debugger->FetchThreads());
  auto frames = debugger->GuessBackTrace(thread_id);

  BOOST_REQUIRE_EQUAL(frames.size(), 2);
  // Frame 0: Indirect Call - Should be SUSPICIOUS because Frame 1 confirmed the
  // Base (FunctionStart) excluding the indirect call.
  BOOST_CHECK_EQUAL(frames[0].address, indirect_ret_addr);
  BOOST_CHECK_EQUAL(frames[0].is_indirect_call, true);
  BOOST_CHECK_EQUAL(frames[0].is_suspicious, true);

  // Frame 1: Direct Call - Confirmed Base
  BOOST_CHECK_EQUAL(frames[1].address, valid_ret_addr_2);
  BOOST_CHECK_EQUAL(frames[1].is_suspicious, false);
}

DEBUGGER_TEST_CASE(GuessBackTraceCorrectlyResolvesMidChainConflicts) {
  Bootup();
  BOOST_REQUIRE(debugger->Attach());

  constexpr uint32_t kStackBase = 0xD0001000;
  constexpr uint32_t kStackLimit = 0xD0000000;
  constexpr uint32_t kTextBase = 0x00010000;
  constexpr uint32_t kTextSize = 0x1000;
  constexpr uint32_t kFunctionStart = kTextBase + 0x20;
  constexpr uint32_t kCurrentEIP = kFunctionStart + 0x20;

  uint32_t thread_id = server->AddThread("TestThread", kTextBase, kStackBase,
                                         kTextBase, kStackLimit);
  server->SetThreadRegister(thread_id, "esp", kStackLimit);
  server->SetThreadRegister(thread_id, "eip", kCurrentEIP);
  server->AddModule("default.xbe", kTextBase, kTextSize);
  server->AddXbeSection("default.xbe", ".text", kTextBase, kTextSize, 1);
  server->AddRegion(kTextBase, kTextSize);

  // Scenario:
  // [A] Confirmed EIP.
  // [Z] Weak Match (Starts speculative chain).
  // [NextIsZ] Valid relative to Z.
  // [NextIsA] Valid relative to A.

  // Z Chain should be SUSPICIOUS. NextIsA should be VALID.

  uint32_t z_ret_addr = kTextBase + 0x200;
  uint32_t next_is_z_ret_addr = kTextBase + 0x300;
  uint32_t next_is_a_ret_addr = kTextBase + 0x400;

  std::vector<uint8_t> stack_data(12);
  WriteInt(stack_data, 0, z_ret_addr);
  WriteInt(stack_data, 4, next_is_z_ret_addr);
  WriteInt(stack_data, 8, next_is_a_ret_addr);
  server->AddRegion(kStackLimit, stack_data);

  std::vector<uint8_t> text_data(kTextSize, 0x90);

  uint32_t z_call_site = z_ret_addr - 5;
  uint32_t x_target = kTextBase + 0x800;
  DefineCall(text_data, kTextBase, z_ret_addr, x_target);
  DefineCall(text_data, kTextBase, next_is_z_ret_addr, z_call_site);
  DefineCall(text_data, kTextBase, next_is_a_ret_addr, kFunctionStart);

  server->SetMemoryRegion(kTextBase, text_data);
  server->AwaitQuiescence();

  BOOST_REQUIRE(debugger->FetchModules());
  BOOST_REQUIRE(debugger->FetchThreads());
  auto frames = debugger->GuessBackTrace(thread_id);

  BOOST_REQUIRE_EQUAL(frames.size(), 3);

  BOOST_CHECK_EQUAL(frames[0].address, z_ret_addr);
  BOOST_CHECK_EQUAL(frames[0].is_suspicious, true);
  BOOST_CHECK_EQUAL(frames[1].address, next_is_z_ret_addr);
  BOOST_CHECK_EQUAL(frames[1].is_suspicious, true);
  BOOST_CHECK_EQUAL(frames[2].address, next_is_a_ret_addr);
  BOOST_CHECK_EQUAL(frames[2].is_suspicious, false);
}

BOOST_AUTO_TEST_SUITE_END()
