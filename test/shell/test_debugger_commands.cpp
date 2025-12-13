#include <boost/test/unit_test.hpp>
#include <chrono>
#include <memory>

#include "configure_test.h"
#include "net/select_thread.h"
#include "shell/debugger_commands.h"
#include "test_util/mock_xbdm_server/mock_server_debugger_interface_fixture.h"
#include "test_util/mock_xbdm_server/mock_xbdm_server.h"
#include "xbox/debugger/debugger_xbox_interface.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"

using namespace xbdm_gdb_bridge;
using namespace xbdm_gdb_bridge::testing;
using namespace std::chrono_literals;

#define DEBUGGER_TEST_CASE(__name) \
  BOOST_AUTO_TEST_CASE(__name, *boost::unit_test::timeout(TEST_TIMEOUT_SECONDS))

// RunTests

BOOST_FIXTURE_TEST_SUITE(RunTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(RunWithNoPathFails) {
  std::stringstream capture;
  DebuggerCommandRun cmd;
  BOOST_REQUIRE(cmd(*interface, empty_args, capture) == Command::HANDLED);
  AwaitQuiescence();

  BOOST_CHECK_EQUAL(Trimmed(capture), "Missing required path argument.");
}

DEBUGGER_TEST_CASE(RunWithInvalidPathFails) {
  server->SetCommandHandler(
      "title", [this](ClientTransport& client, const std::string& params) {
        server->SendResponse(client, ERR_ACCESS_DENIED);
        return true;
      });

  std::stringstream capture;
  DebuggerCommandRun cmd;
  ArgParser args("run", std::vector<std::string>{"e:\\test.xbe"});
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();

  BOOST_CHECK_EQUAL(Trimmed(capture), "Failed to launch XBE");
}

DEBUGGER_TEST_CASE(RunWithValidPathSucceeds) {
  std::stringstream capture;
  DebuggerCommandRun cmd;
  ArgParser args("run", std::vector<std::string>{"e:\\test.xbe"});
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();

  BOOST_CHECK_EQUAL(Trimmed(capture), "");
}

BOOST_AUTO_TEST_SUITE_END()

// LaunchWaitTests

BOOST_FIXTURE_TEST_SUITE(LaunchWaitTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(LaunchWaitWithNoPathFails) {
  std::stringstream capture;
  DebuggerCommandLaunchWait cmd;
  BOOST_REQUIRE(cmd(*interface, empty_args, capture) == Command::HANDLED);
  AwaitQuiescence();

  BOOST_CHECK_EQUAL(Trimmed(capture), "Missing required path argument.");
}

DEBUGGER_TEST_CASE(LaunchWaitWithInvalidPathFails) {
  server->SetCommandHandler(
      "title", [this](ClientTransport& client, const std::string& params) {
        server->SendResponse(client, ERR_ACCESS_DENIED);
        return true;
      });

  std::stringstream capture;
  DebuggerCommandLaunchWait cmd;
  ArgParser args("/launchwait", std::vector<std::string>{"e:\\test.xbe"});
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();

  BOOST_CHECK_EQUAL(Trimmed(capture), "Failed to launch XBE");
}

DEBUGGER_TEST_CASE(LaunchWaitWithValidPathSucceeds) {
  server->AddModule("test.exe", 0x80000000, 0x10000);
  server->AddXbeSection("test.exe", ".test", 0x1000, 100, 0);
  server->AddRegion(0x00010000, 0x00001000, 0x00000004);
  server->AddRegion(0x80000000, 0x10000, 2);

  server->AddExecutionStateCallback(S_STARTED, [this]() {
    server->SimulateExecutionBreakpoint(0x80000000, 1);
  });

  server->SetAfterCommandHandler("stopon", [this](const std::string&) {
    server->SetExecutionState(S_STOPPED);
  });

  std::stringstream capture;
  DebuggerCommandLaunchWait cmd;
  ArgParser args("/launchwait", std::vector<std::string>{"e:\\test.xbe"});
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();

  BOOST_CHECK_EQUAL(Trimmed(capture), "");
}

BOOST_AUTO_TEST_SUITE_END()

// GetThreadsTests

BOOST_FIXTURE_TEST_SUITE(GetThreadsTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(GetThreadsWithThreads) {
  auto debugger_interface =
      std::static_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(debugger_interface->AttachDebugger());
  server->AddThread("1", 0x1234, 0x45670000, 0x89AB);
  server->AddThread("2", 0x2222, 0x20000000, 0x2200);
  server->AddThread("3", 0x3333, 0x30000000, 0x3300);

  std::stringstream capture;
  DebuggerCommandGetThreads cmd;
  BOOST_REQUIRE(cmd(*interface, empty_args, capture) == Command::HANDLED);
  AwaitQuiescence();

  std::string expected =
      "Thread 1\n"
      "Priority 9\n"
      "Suspend count 0\n"
      "Start:  0x00060000\n"
      "Base:  0xd0000000\n"
      "Limit:  0xcfff0000\n"
      "Thread local base:  0xd0001000\n"
      "\n"
      "Thread 2\n"
      "Priority 9\n"
      "Suspend count 0\n"
      "Start:  0x000089ab\n"
      "Base:  0x45670000\n"
      "Limit:  0x45660000\n"
      "Thread local base:  0xd0001000\n"
      "\n"
      "Thread 3\n"
      "Priority 9\n"
      "Suspend count 0\n"
      "Start:  0x00002200\n"
      "Base:  0x20000000\n"
      "Limit:  0x1fff0000\n"
      "Thread local base:  0xd0001000\n"
      "\n"
      "Thread 4\n"
      "Priority 9\n"
      "Suspend count 0\n"
      "Start:  0x00003300\n"
      "Base:  0x30000000\n"
      "Limit:  0x2fff0000\n"
      "Thread local base:  0xd0001000\n";
  BOOST_CHECK_EQUAL(Trimmed(capture), expected);
}

DEBUGGER_TEST_CASE(GetThreadsWithActiveThread) {
  auto debugger_interface =
      std::static_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(debugger_interface->AttachDebugger());
  server->AddThread("Something", 0x1234, 0x45670000, 0x89AB);
  uint32_t active_tid = server->AddThread("Active", 0x2222, 0x20000000, 0x2200);
  server->AddThread("AnotherThread", 0x3333, 0x30000000, 0x3300);

  server->SimulateExecutionBreakpoint(0x1000, active_tid);
  AwaitQuiescence();
  auto debugger = debugger_interface->Debugger();
  debugger->FetchThreads();
  debugger->SetActiveThread(active_tid);

  std::stringstream capture;
  DebuggerCommandGetThreads cmd;
  BOOST_REQUIRE(cmd(*interface, empty_args, capture) == Command::HANDLED);
  AwaitQuiescence();

  std::string expected =
      "Thread 1\n"
      "Priority 9\n"
      "Suspend count 0\n"
      "Start:  0x00060000\n"
      "Base:  0xd0000000\n"
      "Limit:  0xcfff0000\n"
      "Thread local base:  0xd0001000\n"
      "\n"
      "Thread 2\n"
      "Priority 9\n"
      "Suspend count 0\n"
      "Start:  0x000089ab\n"
      "Base:  0x45670000\n"
      "Limit:  0x45660000\n"
      "Thread local base:  0xd0001000\n"
      "\n"
      "[Active thread]\n"
      "Thread 3\n"
      "Priority 9\n"
      "Suspend count 0\n"
      "Start:  0x00002200\n"
      "Base:  0x20000000\n"
      "Limit:  0x1fff0000\n"
      "Thread local base:  0xd0001000\n"
      "\n"
      "Thread 4\n"
      "Priority 9\n"
      "Suspend count 0\n"
      "Start:  0x00003300\n"
      "Base:  0x30000000\n"
      "Limit:  0x2fff0000\n"
      "Thread local base:  0xd0001000\n";
  BOOST_CHECK_EQUAL(Trimmed(capture), expected);
}

BOOST_AUTO_TEST_SUITE_END()

// WhichThread

BOOST_FIXTURE_TEST_SUITE(WhichThreadTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(WhichThreadWithNoAddressFails) {
  auto debugger_interface =
      std::static_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(debugger_interface->AttachDebugger());
  std::stringstream capture;
  DebuggerCommandWhichThread cmd;
  BOOST_REQUIRE(cmd(*interface, empty_args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture), "Missing required `address` argument.");
}

DEBUGGER_TEST_CASE(WhichThreadFindsThreadWithStack) {
  uint32_t active_tid = server->AddThread("Active", 0x2222, 0xCFFFF00A, 0x2200);
  auto debugger_interface =
      std::static_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(debugger_interface->AttachDebugger());
  std::stringstream capture;
  DebuggerCommandWhichThread cmd;
  ArgParser args("whichthread", std::vector<std::string>{"0xCFFFF00A"});

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  std::string expected_thread_name = "Thread " + std::to_string(active_tid - 1);
  std::string output = Trimmed(capture);
  BOOST_CHECK(output.find(expected_thread_name) != std::string::npos);
}

DEBUGGER_TEST_CASE(WhichThreadFailsWhenNoThreadHasStack) {
  auto debugger_interface =
      std::static_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(debugger_interface->AttachDebugger());
  std::stringstream capture;
  DebuggerCommandWhichThread cmd;
  ArgParser args("whichthread 0x10000000");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture),
                    "Failed to find a thread with a stack containing 10000000");
}

DEBUGGER_TEST_CASE(WhichThreadFailsWhenDebuggerNotAttached) {
  std::stringstream capture;
  DebuggerCommandWhichThread cmd;
  ArgParser args("whichthread 0xCFFFF004");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK_EQUAL(Trimmed(capture), "Debugger not attached.");
}

BOOST_AUTO_TEST_SUITE_END()

// GuessBackTraceTests

BOOST_FIXTURE_TEST_SUITE(GuessBackTraceTests, XBDMDebuggerInterfaceFixture)

namespace {

void WriteInt(std::vector<uint8_t>& data, size_t offset, uint32_t val) {
  if (offset + 4 > data.size()) return;
  data[offset] = val & 0xFF;
  data[offset + 1] = (val >> 8) & 0xFF;
  data[offset + 2] = (val >> 16) & 0xFF;
  data[offset + 3] = (val >> 24) & 0xFF;
}

void DefineCall(std::vector<uint8_t>& text_data, uint32_t text_base,
                uint32_t ret_addr, uint32_t call_target) {
  const uint32_t call_instruction_addr = ret_addr - text_base - 5;
  if (call_instruction_addr + 5 > text_data.size()) return;
  text_data[call_instruction_addr] = 0xE8;
  WriteInt(text_data, call_instruction_addr + 1, call_target - ret_addr);
}

}  // namespace

DEBUGGER_TEST_CASE(GuessBackTraceRendersChains) {
  auto debugger_interface =
      std::static_pointer_cast<DebuggerXBOXInterface>(interface);
  constexpr uint32_t kTextBase = 0x00010000;
  constexpr uint32_t kTextSize = 0x1000;
  constexpr uint32_t kStackBase = 0xD0001000;
  constexpr uint32_t kStackLimit = 0xD0000000;

  uint32_t thread_id = server->AddThread("TestThread", kTextBase, kStackBase,
                                         kTextBase, kStackLimit);
  server->SetThreadRegister(thread_id, "esp", kStackLimit);

  server->AddModule("default.xbe", kTextBase, kTextSize);
  server->AddXbeSection("default.xbe", ".text", kTextBase, kTextSize, 1);
  server->AddRegion(kTextBase, kTextSize);

  BOOST_REQUIRE(debugger_interface->AttachDebugger());

  constexpr uint32_t kFunctionStart = kTextBase + 0x20;
  constexpr uint32_t kCurrentEIP = kFunctionStart + 0x20;
  server->SetThreadRegister(thread_id, "eip", kCurrentEIP);

  // Setup:
  // [ESP]   -> Valid Ret Addr 1 (Targets kFunctionStart) -> Chain 0
  // [ESP+4] -> Weak Ret Addr (Targets kUnrelatedFunction) -> Chain 1
  // (Suspicious) [ESP+8] -> Valid Ret Addr 2 (Targets Valid Ret Addr 1) ->
  // Chain 0

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
  // Fix: Target exact call site (ret - 5)
  DefineCall(text_data, kTextBase, kValidRetAddr2, kValidRetAddr1 - 5);

  server->SetMemoryRegion(kTextBase, text_data);
  server->AwaitQuiescence();

  std::stringstream capture;
  DebuggerCommandGuessBackTrace cmd;
  ArgParser args("guessbacktrace", {std::to_string(thread_id)});
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();

  std::string output = capture.str();

  // Verify EIP
  BOOST_CHECK(output.find("EIP: 0x") != std::string::npos);

  // Verify interleaved chains with indentation
  // Expected roughly:
  // EIP: ...
  // # 0 ...  (Chain 0)
  // #  1 ... (Chain 1)
  // # 0 ...  (Chain 0)

  auto pos_eip = output.find("EIP: 0x");
  auto pos_c0_1 = output.find("# 0 ", pos_eip + 1);
  auto pos_c1 = output.find("#  1 ", pos_c0_1 + 1);
  auto pos_c0_2 = output.find("# 0 ", pos_c1 + 1);

  BOOST_CHECK(pos_eip != std::string::npos);
  BOOST_CHECK(pos_c0_1 != std::string::npos);
  BOOST_CHECK(pos_c1 != std::string::npos);
  BOOST_CHECK(pos_c0_2 != std::string::npos);
}

DEBUGGER_TEST_CASE(GuessBackTraceRendersManyChains) {
  auto debugger_interface =
      std::static_pointer_cast<DebuggerXBOXInterface>(interface);
  constexpr uint32_t kTextBase = 0x00010000;
  constexpr uint32_t kTextSize = 0x2000;
  constexpr uint32_t kStackBase = 0xD0001000;
  constexpr uint32_t kStackLimit = 0xD0000000;

  uint32_t thread_id = server->AddThread("TestThread", kTextBase, kStackBase,
                                         kTextBase, kStackLimit);
  server->SetThreadRegister(thread_id, "esp", kStackLimit);

  server->AddModule("default.xbe", kTextBase, kTextSize);
  server->AddXbeSection("default.xbe", ".text", kTextBase, kTextSize, 1);
  server->AddRegion(kTextBase, kTextSize);

  BOOST_REQUIRE(debugger_interface->AttachDebugger());

  constexpr uint32_t kFunctionStart = kTextBase + 0x20;
  constexpr uint32_t kCurrentEIP = kFunctionStart + 0x20;
  server->SetThreadRegister(thread_id, "eip", kCurrentEIP);

  // Setup chains 0, 1, 2
  // [ESP]    -> Chain 0
  // [ESP+4]  -> Chain 1
  // [ESP+8]  -> Chain 2
  // [ESP+12] -> Chain 0 (re-entry)

  constexpr uint32_t kRetAddr0 = kTextBase + 0x100;
  constexpr uint32_t kRetAddr1 = kTextBase + 0x200;
  constexpr uint32_t kRetAddr2 = kTextBase + 0x300;
  constexpr uint32_t kRetAddr0_2 = kTextBase + 0x400;

  // Calls
  constexpr uint32_t kFunc0 = kTextBase + 0x1000;
  constexpr uint32_t kFunc1 = kTextBase + 0x1100;
  constexpr uint32_t kFunc2 = kTextBase + 0x1200;

  std::vector<uint8_t> stack_data(16);
  WriteInt(stack_data, 0, kRetAddr0);
  WriteInt(stack_data, 4, kRetAddr1);
  WriteInt(stack_data, 8, kRetAddr2);
  WriteInt(stack_data, 12, kRetAddr0_2);
  server->AddRegion(kStackLimit, stack_data);

  std::vector<uint8_t> text_data(kTextSize, 0x90);
  DefineCall(text_data, kTextBase, kRetAddr0, kFunctionStart);
  DefineCall(text_data, kTextBase, kRetAddr1, kFunc1);
  DefineCall(text_data, kTextBase, kRetAddr2, kFunc2);
  DefineCall(text_data, kTextBase, kRetAddr0_2, kRetAddr0 - 5);

  server->SetMemoryRegion(kTextBase, text_data);
  server->AwaitQuiescence();

  std::stringstream capture;
  DebuggerCommandGuessBackTrace cmd;
  ArgParser args("guessbacktrace", {std::to_string(thread_id)});
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();

  std::string output = capture.str();

  // Verify indentation levels
  // Chain 0: "# 0 "
  // Chain 1: "#  1 "
  // Chain 2: "#   2 "

  BOOST_CHECK(output.find("# 0 ") != std::string::npos);
  BOOST_CHECK(output.find("#  1 ") != std::string::npos);
  BOOST_CHECK(
      output.find("#   2 ") !=
      std::string::npos);  // Indentation should be 2 spaces for id 2? No, id+1
                           // spaces total (1 fixed + id) space?.
  auto pos_c0 = output.find("# 0 ");
  auto pos_c1 = output.find("#  1 ", pos_c0 + 1);
  auto pos_c2 = output.find("#   2 ", pos_c1 + 1);
  auto pos_c0_2 = output.find("# 0 ", pos_c2 + 1);

  BOOST_CHECK(pos_c0 != std::string::npos);
  BOOST_CHECK(pos_c1 != std::string::npos);
  BOOST_CHECK(pos_c2 != std::string::npos);
  BOOST_CHECK(pos_c0_2 != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
