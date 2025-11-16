#include <boost/test/unit_test.hpp>
#include <chrono>
#include <memory>

#include "mock_xbdm_server/mock_xbdm_server.h"
#include "net/select_thread.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"

using namespace xbdm_gdb_bridge;
using namespace xbdm_gdb_bridge::testing;
using namespace std::chrono_literals;

struct XBDMDebuggerFixture {
  XBDMDebuggerFixture() {
    server = std::make_unique<MockXBDMServer>(0);
    BOOST_REQUIRE(server->Start());

    select_thread_ = std::make_shared<SelectThread>();
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

  void Connect() {
    BOOST_REQUIRE(debugger->Attach());
    BOOST_REQUIRE(debugger->IsAttached());
  }

  std::unique_ptr<MockXBDMServer> server;
  std::unique_ptr<XBDMDebugger> debugger;
  uint16_t port = 0;

 private:
  std::shared_ptr<XBDMContext> context_;
  std::shared_ptr<SelectThread> select_thread_;
};

// ============================================================================
// Connection Tests
// ============================================================================

BOOST_FIXTURE_TEST_SUITE(ConnectionTests, XBDMDebuggerFixture)

BOOST_AUTO_TEST_CASE(ConnectToValidServer, *boost::unit_test::timeout(2)) {
  BOOST_REQUIRE(debugger->Attach());
  BOOST_CHECK(debugger->IsAttached());

  debugger->Shutdown();
  BOOST_CHECK(!debugger->IsAttached());
}

// BOOST_FIXTURE_TEST_CASE(ConnectToInvalidServer, XBDMDebuggerFixture) {
//  XBDMDebugger debugger("127.0.0.1", 1);  // Invalid port
//  BOOST_CHECK(!debugger.Connect());
//  BOOST_CHECK(!debugger.IsConnected());
//}
//
// BOOST_FIXTURE_TEST_CASE(ReconnectAfterDisconnect, XBDMDebuggerFixture) {
//  XBDMDebugger debugger("127.0.0.1", kTestPort);
//
//  BOOST_REQUIRE(debugger.Connect());
//  debugger.Disconnect();
//  BOOST_CHECK(!debugger.IsConnected());
//
//  BOOST_REQUIRE(debugger.Connect());
//  BOOST_CHECK(debugger.IsConnected());
//}
//
// BOOST_FIXTURE_TEST_CASE(ConnectionTimeout, XBDMDebuggerFixture) {
//  server->Stop();  // Stop server to trigger timeout
//
//  XBDMDebugger debugger("127.0.0.1", kTestPort);
//  debugger.SetConnectionTimeout(1000);  // 1 second timeout
//
//  BOOST_CHECK(!debugger.Connect());
//}

BOOST_AUTO_TEST_SUITE_END()

#if 0
// ============================================================================
// Memory Operations Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(MemoryOperations)

BOOST_FIXTURE_TEST_CASE(ReadMemorySuccess, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  uint32_t test_addr = 0x80000000;
  std::vector<uint8_t> expected = {0xDE, 0xAD, 0xBE, 0xEF};
  server->SetMemoryRegion(test_addr, expected);

  std::vector<uint8_t> result;
  BOOST_REQUIRE(debugger->ReadMemory(test_addr, expected.size(), result));
  BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(),
                                result.begin(), result.end());
}

BOOST_FIXTURE_TEST_CASE(ReadMemoryInvalidAddress, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  uint32_t invalid_addr = 0xFFFFFFFF;
  std::vector<uint8_t> result;

  BOOST_CHECK(!debugger->ReadMemory(invalid_addr, 4, result));
}

BOOST_FIXTURE_TEST_CASE(ReadMemoryLargeBlock, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  uint32_t test_addr = 0x80000000;
  std::vector<uint8_t> expected(4096);
  for (size_t i = 0; i < expected.size(); ++i) {
    expected[i] = static_cast<uint8_t>(i % 256);
  }
  server->SetMemoryRegion(test_addr, expected);

  std::vector<uint8_t> result;
  BOOST_REQUIRE(debugger->ReadMemory(test_addr, expected.size(), result));
  BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(),
                                result.begin(), result.end());
}

BOOST_FIXTURE_TEST_CASE(WriteMemorySuccess, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  uint32_t test_addr = 0x80001000;
  std::vector<uint8_t> data = {0x12, 0x34, 0x56, 0x78};

  BOOST_REQUIRE(debugger->WriteMemory(test_addr, data));

  // Verify by reading back
  auto read_back = server->GetMemoryRegion(test_addr, data.size());
  BOOST_CHECK_EQUAL_COLLECTIONS(data.begin(), data.end(),
                                read_back.begin(), read_back.end());
}

BOOST_FIXTURE_TEST_CASE(WriteMemoryInvalidAddress, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  uint32_t invalid_addr = 0xFFFFFFFF;
  std::vector<uint8_t> data = {0x12, 0x34, 0x56, 0x78};

  BOOST_CHECK(!debugger->WriteMemory(invalid_addr, data));
}

BOOST_FIXTURE_TEST_CASE(ReadWriteMemoryRoundtrip, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  uint32_t test_addr = 0x80002000;
  std::vector<uint8_t> original = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

  BOOST_REQUIRE(debugger->WriteMemory(test_addr, original));

  std::vector<uint8_t> read_back;
  BOOST_REQUIRE(debugger->ReadMemory(test_addr, original.size(), read_back));
  BOOST_CHECK_EQUAL_COLLECTIONS(original.begin(), original.end(),
                                read_back.begin(), read_back.end());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Thread Management Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(ThreadManagement)

BOOST_FIXTURE_TEST_CASE(GetThreadList, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  auto thread1 = server->AddThread("MainThread", 0x80000000);
  auto thread2 = server->AddThread("WorkerThread", 0x80001000);

  std::vector<ThreadInfo> threads;
  BOOST_REQUIRE(debugger->GetThreadList(threads));

  BOOST_CHECK_GE(threads.size(), 2);

  bool found_main = false;
  bool found_worker = false;
  for (const auto& thread : threads) {
    if (thread.name == "MainThread") found_main = true;
    if (thread.name == "WorkerThread") found_worker = true;
  }

  BOOST_CHECK(found_main);
  BOOST_CHECK(found_worker);
}

BOOST_FIXTURE_TEST_CASE(SwitchThread, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  auto thread_id = server->AddThread("TestThread", 0x80005000);

  BOOST_CHECK(debugger->SetCurrentThread(thread_id));
  BOOST_CHECK_EQUAL(debugger->GetCurrentThreadId(), thread_id);
}

BOOST_FIXTURE_TEST_CASE(SwitchToInvalidThread, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  uint32_t invalid_thread_id = 9999;
  BOOST_CHECK(!debugger->SetCurrentThread(invalid_thread_id));
}

BOOST_FIXTURE_TEST_CASE(GetThreadContext, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  auto thread_id = server->AddThread("TestThread", 0x80000100);
  server->SetThreadRegister(thread_id, "eax", 0x12345678);
  server->SetThreadRegister(thread_id, "ebx", 0x9ABCDEF0);

  ThreadContext context;
  BOOST_REQUIRE(debugger->GetThreadContext(thread_id, context));

  BOOST_CHECK_EQUAL(context.eax, 0x12345678);
  BOOST_CHECK_EQUAL(context.ebx, 0x9ABCDEF0);
}

BOOST_FIXTURE_TEST_CASE(SetThreadContext, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  auto thread_id = server->AddThread("TestThread", 0x80000200);

  ThreadContext context;
  context.eax = 0xAABBCCDD;
  context.ebx = 0x11223344;
  context.eip = 0x80000500;

  BOOST_REQUIRE(debugger->SetThreadContext(thread_id, context));

  // Verify by reading back
  ThreadContext read_back;
  BOOST_REQUIRE(debugger->GetThreadContext(thread_id, read_back));
  BOOST_CHECK_EQUAL(read_back.eax, context.eax);
  BOOST_CHECK_EQUAL(read_back.ebx, context.ebx);
  BOOST_CHECK_EQUAL(read_back.eip, context.eip);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Breakpoint Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(BreakpointOperations)

BOOST_FIXTURE_TEST_CASE(SetBreakpoint, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  uint32_t bp_addr = 0x80001000;
  BOOST_REQUIRE(debugger->SetBreakpoint(bp_addr));
  BOOST_CHECK(server->HasBreakpoint(bp_addr));
}

BOOST_FIXTURE_TEST_CASE(ClearBreakpoint, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  uint32_t bp_addr = 0x80001000;
  BOOST_REQUIRE(debugger->SetBreakpoint(bp_addr));
  BOOST_REQUIRE(server->HasBreakpoint(bp_addr));

  BOOST_REQUIRE(debugger->ClearBreakpoint(bp_addr));
  BOOST_CHECK(!server->HasBreakpoint(bp_addr));
}

BOOST_FIXTURE_TEST_CASE(SetMultipleBreakpoints, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  std::vector<uint32_t> addresses = {0x80001000, 0x80002000, 0x80003000};

  for (uint32_t addr : addresses) {
    BOOST_REQUIRE(debugger->SetBreakpoint(addr));
  }

  for (uint32_t addr : addresses) {
    BOOST_CHECK(server->HasBreakpoint(addr));
  }
}

BOOST_FIXTURE_TEST_CASE(ClearNonexistentBreakpoint, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  uint32_t bp_addr = 0x80001000;
  // Should not fail even if breakpoint doesn't exist
  BOOST_CHECK(debugger->ClearBreakpoint(bp_addr));
}

BOOST_FIXTURE_TEST_CASE(SetHardwareBreakpoint, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  uint32_t bp_addr = 0x80001000;
  BOOST_REQUIRE(debugger->SetHardwareBreakpoint(bp_addr));
  BOOST_CHECK(server->HasBreakpoint(bp_addr));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Execution Control Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(ExecutionControl)

BOOST_FIXTURE_TEST_CASE(HaltExecution, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  server->SetExecutionState(true);
  BOOST_REQUIRE(server->IsExecutionRunning());

  BOOST_REQUIRE(debugger->Halt());
  BOOST_CHECK(!server->IsExecutionRunning());
}

BOOST_FIXTURE_TEST_CASE(ContinueExecution, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  server->SetExecutionState(false);
  BOOST_REQUIRE(!server->IsExecutionRunning());

  BOOST_REQUIRE(debugger->Continue());
  BOOST_CHECK(server->IsExecutionRunning());
}

BOOST_FIXTURE_TEST_CASE(StepExecution, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  auto thread_id = server->AddThread("TestThread", 0x80000000);
  server->SetThreadRegister(thread_id, "eip", 0x80000000);

  BOOST_REQUIRE(debugger->Step(thread_id));

  // Verify EIP changed
  ThreadContext context;
  BOOST_REQUIRE(debugger->GetThreadContext(thread_id, context));
  // EIP should have advanced (exact value depends on instruction)
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Module Management Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(ModuleManagement)

BOOST_FIXTURE_TEST_CASE(GetModuleList, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  server->AddModule("xboxkrnl.exe", 0x80010000, 0x100000);
  server->AddModule("default.xbe", 0x80100000, 0x200000);

  std::vector<ModuleInfo> modules;
  BOOST_REQUIRE(debugger->GetModuleList(modules));

  BOOST_CHECK_GE(modules.size(), 2);

  bool found_kernel = false;
  bool found_xbe = false;
  for (const auto& module : modules) {
    if (module.name == "xboxkrnl.exe") {
      found_kernel = true;
      BOOST_CHECK_EQUAL(module.base_address, 0x80010000);
      BOOST_CHECK_EQUAL(module.size, 0x100000);
    }
    if (module.name == "default.xbe") {
      found_xbe = true;
      BOOST_CHECK_EQUAL(module.base_address, 0x80100000);
      BOOST_CHECK_EQUAL(module.size, 0x200000);
    }
  }

  BOOST_CHECK(found_kernel);
  BOOST_CHECK(found_xbe);
}

BOOST_FIXTURE_TEST_CASE(GetModuleByName, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  server->AddModule("test.xbe", 0x80200000, 0x50000);

  ModuleInfo module;
  BOOST_REQUIRE(debugger->GetModuleByName("test.xbe", module));
  BOOST_CHECK_EQUAL(module.name, "test.xbe");
  BOOST_CHECK_EQUAL(module.base_address, 0x80200000);
  BOOST_CHECK_EQUAL(module.size, 0x50000);
}

BOOST_FIXTURE_TEST_CASE(GetModuleByAddress, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  server->AddModule("test.xbe", 0x80200000, 0x50000);

  ModuleInfo module;
  // Address within module
  BOOST_REQUIRE(debugger->GetModuleByAddress(0x80220000, module));
  BOOST_CHECK_EQUAL(module.name, "test.xbe");
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Error Handling Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(ErrorHandling)

BOOST_FIXTURE_TEST_CASE(OperationWhileDisconnected, XBDMDebuggerFixture) {
  XBDMDebugger debugger("127.0.0.1", kTestPort);

  std::vector<uint8_t> data;
  BOOST_CHECK(!debugger.ReadMemory(0x80000000, 4, data));
  BOOST_CHECK(!debugger.WriteMemory(0x80000000, {0x12, 0x34}));
  BOOST_CHECK(!debugger.SetBreakpoint(0x80000000));
  BOOST_CHECK(!debugger.Halt());
}

BOOST_FIXTURE_TEST_CASE(ServerDisconnectDuringOperation, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  // Stop server
  server->Stop();

  // Operations should fail gracefully
  std::vector<uint8_t> data;
  BOOST_CHECK(!debugger->ReadMemory(0x80000000, 4, data));
}

BOOST_FIXTURE_TEST_CASE(MalformedResponse, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  // Set custom handler that returns malformed response
  server->SetCommandHandler("getmem",
                            [](const std::string&, const std::map<std::string, std::string>&) {
                              return "INVALID RESPONSE FORMAT\r\n";
                            });

  std::vector<uint8_t> data;
  BOOST_CHECK(!debugger->ReadMemory(0x80000000, 4, data));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Register Operations Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(RegisterOperations)

BOOST_FIXTURE_TEST_CASE(ReadRegister, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  auto thread_id = server->AddThread("TestThread", 0x80000000);
  server->SetThreadRegister(thread_id, "eax", 0xDEADBEEF);

  uint32_t value;
  BOOST_REQUIRE(debugger->ReadRegister(thread_id, "eax", value));
  BOOST_CHECK_EQUAL(value, 0xDEADBEEF);
}

BOOST_FIXTURE_TEST_CASE(WriteRegister, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  auto thread_id = server->AddThread("TestThread", 0x80000000);

  BOOST_REQUIRE(debugger->WriteRegister(thread_id, "ebx", 0x12345678));

  uint32_t value;
  BOOST_REQUIRE(debugger->ReadRegister(thread_id, "ebx", value));
  BOOST_CHECK_EQUAL(value, 0x12345678);
}

BOOST_FIXTURE_TEST_CASE(ReadAllRegisters, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  auto thread_id = server->AddThread("TestThread", 0x80000000);
  server->SetThreadRegister(thread_id, "eax", 0x11111111);
  server->SetThreadRegister(thread_id, "ebx", 0x22222222);
  server->SetThreadRegister(thread_id, "ecx", 0x33333333);

  std::map<std::string, uint32_t> registers;
  BOOST_REQUIRE(debugger->ReadAllRegisters(thread_id, registers));

  BOOST_CHECK_EQUAL(registers["eax"], 0x11111111);
  BOOST_CHECK_EQUAL(registers["ebx"], 0x22222222);
  BOOST_CHECK_EQUAL(registers["ecx"], 0x33333333);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Stress Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(StressTests)

BOOST_FIXTURE_TEST_CASE(MultipleSequentialReads, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
  uint32_t base_addr = 0x80000000;

  for (int i = 0; i < 100; ++i) {
    uint32_t addr = base_addr + (i * 4);
    server->SetMemoryRegion(addr, test_data);

    std::vector<uint8_t> result;
    BOOST_REQUIRE(debugger->ReadMemory(addr, test_data.size(), result));
    BOOST_CHECK_EQUAL_COLLECTIONS(test_data.begin(), test_data.end(),
                                  result.begin(), result.end());
  }
}

BOOST_FIXTURE_TEST_CASE(LargeMemoryTransfer, XBDMDebuggerFixture) {
  SetupConnectedDebugger();

  // Test with 1MB of data
  std::vector<uint8_t> large_data(1024 * 1024);
  for (size_t i = 0; i < large_data.size(); ++i) {
    large_data[i] = static_cast<uint8_t>(i & 0xFF);
  }

  uint32_t addr = 0x80000000;
  server->SetMemoryRegion(addr, large_data);

  std::vector<uint8_t> result;
  BOOST_REQUIRE(debugger->ReadMemory(addr, large_data.size(), result));
  BOOST_CHECK_EQUAL_COLLECTIONS(large_data.begin(), large_data.end(),
                                result.begin(), result.end());
}

BOOST_AUTO_TEST_SUITE_END()
#endif
