#include <boost/test/unit_test.hpp>
#include <sstream>
#include <vector>

#include "shell/debugger_commands.h"
#include "test_util/mock_xbdm_server/mock_server_debugger_interface_fixture.h"
#include "util/parsing.h"
#include "xbox/debugger/debugger_xbox_interface.h"
#include "xbox/debugger/xbdm_debugger.h"

using namespace xbdm_gdb_bridge;
using namespace xbdm_gdb_bridge::testing;

#define DEBUGGER_TEST_CASE(__name) \
  BOOST_AUTO_TEST_CASE(__name, *boost::unit_test::timeout(10))

BOOST_FIXTURE_TEST_SUITE(DisassembleTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(DisassembleWithNoArgsNoActiveThreadFails) {
  auto debugger_interface =
      std::static_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(debugger_interface->AttachDebugger());
  AwaitQuiescence();

  // Create a clean state with no active thread
  // MockServer creates one thread with ID 1 by default.
  server->RemoveThread(1);

  // Force fetch threads to update debugger state
  auto debugger = debugger_interface->Debugger();
  debugger->FetchThreads();

  std::stringstream capture;
  DebuggerCommandDisassemble cmd;
  ArgParser args("disassemble");
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  std::string output = Trimmed(capture);
  if (output.find("No address provided and no active thread") ==
      std::string::npos) {
    BOOST_TEST_MESSAGE("Output: " << output);
  }
  BOOST_CHECK(output.find("No address provided and no active thread") !=
              std::string::npos);
}

DEBUGGER_TEST_CASE(DisassembleWithAddressSucceeds) {
  auto debugger_interface =
      std::static_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(debugger_interface->AttachDebugger());
  AwaitQuiescence();

  // NOP instructions (0x90)
  std::vector<uint8_t> memory(200, 0x90);
  server->SetMemoryRegion(0x10000, memory);

  std::stringstream capture;
  DebuggerCommandDisassemble cmd;
  ArgParser args("disassemble", std::vector<std::string>{"0x10000"});
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  std::string output = capture.str();
  if (output.find("nop") == std::string::npos) {
    BOOST_TEST_MESSAGE("Output: " << output);
  }
  // Should contain "nop"
  BOOST_CHECK(output.find("nop") != std::string::npos);
  // Should contain "90" (hex byte)
  BOOST_CHECK(output.find("90") != std::string::npos);
  // Should start at 0x10000
  BOOST_CHECK(output.find("0x10000") != std::string::npos);
}

DEBUGGER_TEST_CASE(DisassembleWithActiveThreadUsesEIP) {
  auto debugger_interface =
      std::static_pointer_cast<DebuggerXBOXInterface>(interface);
  BOOST_REQUIRE(debugger_interface->AttachDebugger());
  AwaitQuiescence();

  // NOP instructions (0x90)
  std::vector<uint8_t> memory(200, 0x90);
  server->SetMemoryRegion(0x80001000, memory);

  // Create a thread stopped at 0x80001000
  uint32_t tid = server->AddThread("Main", 0x80001000);
  server->SetThreadRegister(tid, "Eip", 0x80001000);

  auto debugger = debugger_interface->Debugger();
  BOOST_REQUIRE(debugger != nullptr);

  debugger->FetchThreads();
  debugger->SetActiveThread(tid);

  std::stringstream capture;
  DebuggerCommandDisassemble cmd;
  ArgParser args("disassemble");
  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  std::string output = capture.str();
  if (output.find("nop") == std::string::npos) {
    BOOST_TEST_MESSAGE("Output: " << output);
  }
  BOOST_CHECK(output.find("nop") != std::string::npos);
  BOOST_CHECK(output.find("90") != std::string::npos);
  BOOST_CHECK(output.find("0x80001000") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
