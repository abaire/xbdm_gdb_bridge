#include <boost/test/unit_test.hpp>
#include <chrono>
#include <iostream>
#include <thread>

#include "configure_test.h"
#include "shell/commands.h"
#include "test_util/mock_xbdm_server/mock_server_debugger_interface_fixture.h"
#include "test_util/mock_xbdm_server/mock_xbdm_server.h"
#include "xbox/debugger/debugger_xbox_interface.h"
#include "xbox/debugger/xbdm_debugger.h"

using namespace xbdm_gdb_bridge;
using namespace xbdm_gdb_bridge::testing;
using namespace std::chrono_literals;

#define DEBUGGER_TEST_CASE(__name) \
  BOOST_AUTO_TEST_CASE(__name, *boost::unit_test::timeout(TEST_TIMEOUT_SECONDS))

BOOST_FIXTURE_TEST_SUITE(BreakTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(BreakAddrWithValidArgs) {
  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break addr 0x1000");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();

  BOOST_CHECK(server->HasBreakpoint(0x1000));
}

DEBUGGER_TEST_CASE(BreakClearAll) {
  server->AddBreakpoint(0x1000);
  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break clearall");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();

  BOOST_CHECK(!server->HasBreakpoint(0x1000));
}

DEBUGGER_TEST_CASE(BreakStart) {
  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break start");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();
}

DEBUGGER_TEST_CASE(BreakAddrRemove) {
  server->AddBreakpoint(0x1000);
  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break -addr 0x1000");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();

  BOOST_CHECK(!server->HasBreakpoint(0x1000));
}

DEBUGGER_TEST_CASE(BreakReadRemove) {
  server->AddBreakpoint(0x1000, Breakpoint::Type::READ);
  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break -read 0x1000");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();

  BOOST_CHECK(!server->HasBreakpoint(0x1000));
}

DEBUGGER_TEST_CASE(BreakWrite) {
  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break write 0x2000 4");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();

  BOOST_CHECK(server->HasBreakpoint(0x2000));
}

DEBUGGER_TEST_CASE(BreakWriteRemove) {
  server->AddBreakpoint(0x2000, Breakpoint::Type::WRITE);
  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break -write 0x2000");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();

  BOOST_CHECK(!server->HasBreakpoint(0x2000));
}

DEBUGGER_TEST_CASE(BreakExecute) {
  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break execute 0x3000 4");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();

  BOOST_CHECK(server->HasBreakpoint(0x3000));
}

DEBUGGER_TEST_CASE(BreakExecuteRemove) {
  server->AddBreakpoint(0x3000, Breakpoint::Type::EXECUTE);
  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break -execute 0x3000");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  AwaitQuiescence();

  BOOST_CHECK(!server->HasBreakpoint(0x3000));
}

DEBUGGER_TEST_CASE(BreakInvalidArgs) {
  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break invalid");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
}

BOOST_AUTO_TEST_SUITE_END()

// BreakConditionalTests

BOOST_FIXTURE_TEST_SUITE(BreakConditionalTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(BreakConditionalTrue) {
  uint32_t tid = server->AddThread("main");
  server->SetThreadRegister(tid, "eax", 0x123);

  auto dbg_iface = std::dynamic_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(dbg_iface);
  BOOST_REQUIRE(dbg_iface->AttachDebugger());
  BOOST_REQUIRE(dbg_iface->Debugger()->FetchThreads());
  dbg_iface->Debugger()->SetActiveThread(tid);

  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break addr 0x1000 IF $eax == 0x123");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK(server->HasBreakpoint(0x1000));

  bool continued = false;
  server->SetAfterCommandHandler("continue",
                                 [&](const std::string&) { continued = true; });

  server->SimulateExecutionBreakpoint(0x1000, tid);
  AwaitQuiescence();

  BOOST_CHECK(!continued);
  auto thread = dbg_iface->Debugger()->GetThread(tid);
  BOOST_REQUIRE(thread);
  BOOST_REQUIRE(thread->last_stop_reason);
  BOOST_CHECK_EQUAL(thread->last_stop_reason->type, SRT_BREAKPOINT);
}

DEBUGGER_TEST_CASE(BreakConditionalFalse) {
  uint32_t tid = server->AddThread("main");
  server->SetThreadRegister(tid, "eax", 0x123);

  auto dbg_iface = std::dynamic_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(dbg_iface);
  BOOST_REQUIRE(dbg_iface->AttachDebugger());
  BOOST_REQUIRE(dbg_iface->Debugger()->FetchThreads());
  dbg_iface->Debugger()->SetActiveThread(tid);

  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break addr 0x2000 IF $eax == 0x456");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK(server->HasBreakpoint(0x2000));

  bool continued = false;
  server->SetAfterCommandHandler("continue",
                                 [&](const std::string&) { continued = true; });

  bool go_called = false;
  server->SetAfterCommandHandler("go",
                                 [&](const std::string&) { go_called = true; });

  server->SimulateExecutionBreakpoint(0x2000, tid);
  AwaitQuiescence();

  BOOST_CHECK(continued);
  BOOST_CHECK(go_called);
}

DEBUGGER_TEST_CASE(BreakConditionalComplexTrue) {
  uint32_t tid = server->AddThread("main");
  server->SetThreadRegister(tid, "eax", 2);
  server->SetThreadRegister(tid, "ecx", 10);
  server->SetThreadRegister(tid, "edx", 20);
  server->SetThreadRegister(tid, "esi", 5);

  auto dbg_iface = std::dynamic_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(dbg_iface);
  BOOST_REQUIRE(dbg_iface->AttachDebugger());
  BOOST_REQUIRE(dbg_iface->Debugger()->FetchThreads());
  dbg_iface->Debugger()->SetActiveThread(tid);

  std::stringstream capture;
  CommandBreak cmd;
  // (2 < 3 AND (10 * 20) > 100) OR 5 == 3
  // (True AND 200 > 100) OR False
  // (True AND True) OR False
  // True OR False
  // True -> Should break
  ArgParser args(
      "break addr 0xB000 IF ($eax < 3 AND ($ecx * $edx) > 100) OR $esi == 3");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK(server->HasBreakpoint(0xB000));

  bool continued = false;
  server->SetAfterCommandHandler("continue",
                                 [&](const std::string&) { continued = true; });

  server->SimulateExecutionBreakpoint(0xB000, tid);
  AwaitQuiescence();

  BOOST_CHECK(!continued);
  auto thread = dbg_iface->Debugger()->GetThread(tid);
  BOOST_REQUIRE(thread);
  BOOST_REQUIRE(thread->last_stop_reason);
  BOOST_CHECK_EQUAL(thread->last_stop_reason->type, SRT_BREAKPOINT);
}

DEBUGGER_TEST_CASE(BreakConditionalComplexDoubleFalse) {
  uint32_t tid = server->AddThread("main");
  server->SetThreadRegister(tid, "eax", 3);
  server->SetThreadRegister(tid, "ecx", 10);
  server->SetThreadRegister(tid, "edx", 20);
  server->SetThreadRegister(tid, "esi", 5);  // False

  auto dbg_iface = std::dynamic_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(dbg_iface);
  BOOST_REQUIRE(dbg_iface->AttachDebugger());
  BOOST_REQUIRE(dbg_iface->Debugger()->FetchThreads());
  dbg_iface->Debugger()->SetActiveThread(tid);

  std::stringstream capture;
  CommandBreak cmd;
  // (5 < 3 AND (10 * 20) > 100) OR 5 == 3
  ArgParser args(
      "break addr 0xC000 IF ($eax < 3 AND ($ecx * $edx) > 100) OR $esi == 3");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK(server->HasBreakpoint(0xC000));

  bool continued = false;
  server->SetAfterCommandHandler("continue",
                                 [&](const std::string&) { continued = true; });
  bool go_called = false;
  server->SetAfterCommandHandler("go",
                                 [&](const std::string&) { go_called = true; });

  server->SimulateExecutionBreakpoint(0xC000, tid);
  AwaitQuiescence();

  BOOST_CHECK(continued);
  BOOST_CHECK(go_called);
}

DEBUGGER_TEST_CASE(BreakConditionalComplexOrTrue) {
  uint32_t tid = server->AddThread("main");
  server->SetThreadRegister(tid, "eax", 4);
  server->SetThreadRegister(tid, "ecx", 10);
  server->SetThreadRegister(tid, "edx", 20);
  server->SetThreadRegister(tid, "esi", 3);

  auto dbg_iface = std::dynamic_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(dbg_iface);
  BOOST_REQUIRE(dbg_iface->AttachDebugger());
  BOOST_REQUIRE(dbg_iface->Debugger()->FetchThreads());
  dbg_iface->Debugger()->SetActiveThread(tid);

  std::stringstream capture;
  CommandBreak cmd;
  // (5 < 3 AND (10 * 20) > 100) OR 5 == 3
  ArgParser args(
      "break addr 0xC000 IF ($eax < 3 AND ($ecx * $edx) > 100) OR $esi == 3");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK(server->HasBreakpoint(0xC000));

  bool continued = false;
  server->SetAfterCommandHandler("continue",
                                 [&](const std::string&) { continued = true; });
  bool go_called = false;
  server->SetAfterCommandHandler("go",
                                 [&](const std::string&) { go_called = true; });

  server->SimulateExecutionBreakpoint(0xC000, tid);
  AwaitQuiescence();

  BOOST_CHECK(!continued);
  auto thread = dbg_iface->Debugger()->GetThread(tid);
  BOOST_REQUIRE(thread);
  BOOST_REQUIRE(thread->last_stop_reason);
  BOOST_CHECK_EQUAL(thread->last_stop_reason->type, SRT_BREAKPOINT);
}

DEBUGGER_TEST_CASE(BreakConditionalReadTrue) {
  uint32_t tid = server->AddThread("main");
  server->SetThreadRegister(tid, "ebx", 10);

  auto dbg_iface = std::dynamic_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(dbg_iface);
  BOOST_REQUIRE(dbg_iface->AttachDebugger());
  BOOST_REQUIRE(dbg_iface->Debugger()->FetchThreads());
  dbg_iface->Debugger()->SetActiveThread(tid);

  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break read 0x3000 IF $ebx > 5");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK(server->HasBreakpoint(0x3000));

  bool continued = false;
  server->SetAfterCommandHandler("continue",
                                 [&](const std::string&) { continued = true; });

  server->SimulateReadWatchpoint(0x3000, tid);
  AwaitQuiescence();

  BOOST_CHECK(!continued);
  auto thread = dbg_iface->Debugger()->GetThread(tid);
  BOOST_REQUIRE(thread);
  BOOST_REQUIRE(thread->last_stop_reason);
  BOOST_CHECK_EQUAL(thread->last_stop_reason->type, SRT_WATCHPOINT);
}

DEBUGGER_TEST_CASE(BreakConditionalReadFalse) {
  uint32_t tid = server->AddThread("main");
  server->SetThreadRegister(tid, "ebx", 3);

  auto dbg_iface = std::dynamic_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(dbg_iface);
  BOOST_REQUIRE(dbg_iface->AttachDebugger());
  BOOST_REQUIRE(dbg_iface->Debugger()->FetchThreads());
  dbg_iface->Debugger()->SetActiveThread(tid);

  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break read 0x4000 IF $ebx > 5");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK(server->HasBreakpoint(0x4000));

  bool continued = false;
  server->SetAfterCommandHandler("continue",
                                 [&](const std::string&) { continued = true; });

  bool go_called = false;
  server->SetAfterCommandHandler("go",
                                 [&](const std::string&) { go_called = true; });

  server->SimulateReadWatchpoint(0x4000, tid);
  AwaitQuiescence();

  int retries = 20;
  while (!continued && retries-- > 0) {
    std::this_thread::sleep_for(50ms);
  }
  BOOST_CHECK(continued);
  BOOST_CHECK(go_called);
}

DEBUGGER_TEST_CASE(BreakConditionalWriteTrue) {
  uint32_t tid = server->AddThread("main");
  server->SetThreadRegister(tid, "ecx", 20);

  auto dbg_iface = std::dynamic_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(dbg_iface);
  BOOST_REQUIRE(dbg_iface->AttachDebugger());
  BOOST_REQUIRE(dbg_iface->Debugger()->FetchThreads());
  dbg_iface->Debugger()->SetActiveThread(tid);

  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break write 0x5000 IF $ecx > 10");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK(server->HasBreakpoint(0x5000));

  bool continued = false;
  server->SetAfterCommandHandler("continue",
                                 [&](const std::string&) { continued = true; });

  server->SimulateWriteWatchpoint(0x5000, tid);
  AwaitQuiescence();

  BOOST_CHECK(!continued);
  auto thread = dbg_iface->Debugger()->GetThread(tid);
  BOOST_REQUIRE(thread);
  BOOST_REQUIRE(thread->last_stop_reason);
  BOOST_CHECK_EQUAL(thread->last_stop_reason->type, SRT_WATCHPOINT);
}

DEBUGGER_TEST_CASE(BreakConditionalWriteFalse) {
  uint32_t tid = server->AddThread("main");
  server->SetThreadRegister(tid, "ecx", 5);

  auto dbg_iface = std::dynamic_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(dbg_iface);
  BOOST_REQUIRE(dbg_iface->AttachDebugger());
  BOOST_REQUIRE(dbg_iface->Debugger()->FetchThreads());
  dbg_iface->Debugger()->SetActiveThread(tid);

  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break write 0x6000 IF $ecx > 10");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK(server->HasBreakpoint(0x6000));

  bool continued = false;
  server->SetAfterCommandHandler("continue",
                                 [&](const std::string&) { continued = true; });

  bool go_called = false;
  server->SetAfterCommandHandler("go",
                                 [&](const std::string&) { go_called = true; });

  server->SimulateWriteWatchpoint(0x6000, tid);
  AwaitQuiescence();

  int retries = 20;
  while (!continued && retries-- > 0) {
    std::this_thread::sleep_for(50ms);
  }
  BOOST_CHECK(continued);
  BOOST_CHECK(go_called);
}

DEBUGGER_TEST_CASE(BreakConditionalExecuteTrue) {
  uint32_t tid = server->AddThread("main");
  server->SetThreadRegister(tid, "edx", 30);

  auto dbg_iface = std::dynamic_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(dbg_iface);
  BOOST_REQUIRE(dbg_iface->AttachDebugger());
  BOOST_REQUIRE(dbg_iface->Debugger()->FetchThreads());
  dbg_iface->Debugger()->SetActiveThread(tid);

  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break execute 0x7000 IF $edx == 30");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK(server->HasBreakpoint(0x7000));

  bool continued = false;
  server->SetAfterCommandHandler("continue",
                                 [&](const std::string&) { continued = true; });

  server->SimulateExecuteWatchpoint(0x7000, tid);
  AwaitQuiescence();

  BOOST_CHECK(!continued);
  auto thread = dbg_iface->Debugger()->GetThread(tid);
  BOOST_REQUIRE(thread);
  BOOST_REQUIRE(thread->last_stop_reason);
  BOOST_CHECK_EQUAL(thread->last_stop_reason->type, SRT_WATCHPOINT);
}

DEBUGGER_TEST_CASE(BreakConditionalExecuteFalse) {
  uint32_t tid = server->AddThread("main");
  server->SetThreadRegister(tid, "edx", 40);

  auto dbg_iface = std::dynamic_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(dbg_iface);
  BOOST_REQUIRE(dbg_iface->AttachDebugger());
  BOOST_REQUIRE(dbg_iface->Debugger()->FetchThreads());
  dbg_iface->Debugger()->SetActiveThread(tid);

  std::stringstream capture;
  CommandBreak cmd;
  ArgParser args("break execute 0x8000 IF $edx == 30");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK(server->HasBreakpoint(0x8000));

  bool continued = false;
  server->SetAfterCommandHandler("continue",
                                 [&](const std::string&) { continued = true; });

  bool go_called = false;
  server->SetAfterCommandHandler("go",
                                 [&](const std::string&) { go_called = true; });

  server->SimulateExecuteWatchpoint(0x8000, tid);
  AwaitQuiescence();

  int retries = 20;
  while (!continued && retries-- > 0) {
    std::this_thread::sleep_for(50ms);
  }
  BOOST_CHECK(continued);
  BOOST_CHECK(go_called);
}

DEBUGGER_TEST_CASE(BreakConditionalTIDTrue) {
  uint32_t tid = server->AddThread("main");

  auto dbg_iface = std::dynamic_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(dbg_iface);
  BOOST_REQUIRE(dbg_iface->AttachDebugger());
  BOOST_REQUIRE(dbg_iface->Debugger()->FetchThreads());
  dbg_iface->Debugger()->SetActiveThread(tid);

  std::stringstream capture;
  CommandBreak cmd;
  std::string cmd_str = "break addr 0x9000 IF tid == " + std::to_string(tid);
  ArgParser args(cmd_str);

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK(server->HasBreakpoint(0x9000));

  bool continued = false;
  server->SetAfterCommandHandler("continue",
                                 [&](const std::string&) { continued = true; });

  server->SimulateExecutionBreakpoint(0x9000, tid);
  AwaitQuiescence();

  BOOST_CHECK(!continued);
  auto thread = dbg_iface->Debugger()->GetThread(tid);
  BOOST_REQUIRE(thread);
  BOOST_REQUIRE(thread->last_stop_reason);
  BOOST_CHECK_EQUAL(thread->last_stop_reason->type, SRT_BREAKPOINT);
}

DEBUGGER_TEST_CASE(BreakConditionalTIDFalse) {
  uint32_t tid = server->AddThread("main");

  auto dbg_iface = std::dynamic_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(dbg_iface);
  BOOST_REQUIRE(dbg_iface->AttachDebugger());
  BOOST_REQUIRE(dbg_iface->Debugger()->FetchThreads());
  dbg_iface->Debugger()->SetActiveThread(tid);

  std::stringstream capture;
  CommandBreak cmd;
  std::string cmd_str =
      "break addr 0xA000 IF tid == " + std::to_string(tid + 1);
  ArgParser args(cmd_str);

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);
  BOOST_CHECK(server->HasBreakpoint(0xA000));

  bool continued = false;
  server->SetAfterCommandHandler("continue",
                                 [&](const std::string&) { continued = true; });

  bool go_called = false;
  server->SetAfterCommandHandler("go",
                                 [&](const std::string&) { go_called = true; });

  server->SimulateExecutionBreakpoint(0xA000, tid);
  AwaitQuiescence();

  int retries = 20;
  while (!continued && retries-- > 0) {
    std::this_thread::sleep_for(50ms);
  }
  BOOST_CHECK(continued);
  BOOST_CHECK(go_called);
}

BOOST_AUTO_TEST_SUITE_END()
