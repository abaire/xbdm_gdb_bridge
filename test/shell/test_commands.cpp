#include <boost/test/unit_test.hpp>
#include <chrono>

#include "configure_test.h"
#include "shell/commands.h"
#include "shell/debugger_commands.h"
#include "test_util/mock_xbdm_server/mock_server_debugger_interface_fixture.h"
#include "test_util/mock_xbdm_server/mock_xbdm_server.h"
#include "xbox/debugger/debugger_expression_parser.h"
#include "xbox/debugger/debugger_xbox_interface.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"

using namespace xbdm_gdb_bridge;
using namespace xbdm_gdb_bridge::testing;
using namespace std::chrono_literals;

#define DEBUGGER_TEST_CASE(__name) \
  BOOST_AUTO_TEST_CASE(__name, *boost::unit_test::timeout(TEST_TIMEOUT_SECONDS))

// GetMem

BOOST_FIXTURE_TEST_SUITE(GetMemTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(GetMemWithNoAddressFails) {
  std::stringstream capture;
  CommandGetMem cmd;
  BOOST_REQUIRE(cmd(*interface, empty_args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture), "Missing required address argument.");
}

DEBUGGER_TEST_CASE(GetMemWithNoSizeFails) {
  std::stringstream capture;
  CommandGetMem cmd;
  ArgParser args("getmem", std::vector<std::string>{"0x12345"});

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture), "Missing required size argument.");
}

DEBUGGER_TEST_CASE(GetMemSucceeds) {
  std::vector<uint8_t> data{
      0xFF, 0xEE, 0x44, 0x11, 0x22, 0x33, 0x88, 0x99,
      0x01, 0x02, 0x03, 0x04, 0xA0, 0xA1, 0xA2, 0xA3,
  };
  server->AddRegion(0x12345, data);
  std::stringstream capture;
  CommandGetMem cmd;
  ArgParser args("getmem", std::vector<std::string>{"0x12345", "16"});

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture),
                    "ff ee 44 11 22 33 88 99 01 02 03 04 a0 a1 a2 a3 ");
}

DEBUGGER_TEST_CASE(GetMemWithoutExpressionParserFailsOnExpressions) {
  std::vector<uint8_t> data{
      0xFF, 0xEE, 0x44, 0x11, 0x22, 0x33, 0x88, 0x99,
      0x01, 0x02, 0x03, 0x04, 0xA0, 0xA1, 0xA2, 0xA3,
  };
  server->AddRegion(0x12345, data);
  std::stringstream capture;
  CommandGetMem cmd;
  ArgParser args("getmem (0x12300 + 0x45) 16");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture),
                    "Syntax error Value 0x12300 + 0x45 is not numeric");
}

DEBUGGER_TEST_CASE(GetMemSupportsSimpleExpressionsInAddress) {
  std::vector<uint8_t> data{
      0xFF, 0xEE, 0x44, 0x11, 0x22, 0x33, 0x88, 0x99,
      0x01, 0x02, 0x03, 0x04, 0xA0, 0xA1, 0xA2, 0xA3,
  };
  server->AddRegion(0x12345, data);
  std::stringstream capture;
  CommandGetMem cmd;
  ArgParser args("getmem (0x12300 + 0x45) 16");
  interface->SetExpressionParser(std::make_shared<DebuggerExpressionParser>());

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture),
                    "ff ee 44 11 22 33 88 99 01 02 03 04 a0 a1 a2 a3 ");
}

DEBUGGER_TEST_CASE(GetMemSupportsTrivialRegisterExpressionsInAddress) {
  std::vector<uint8_t> data{
      0xFF, 0xEE, 0x44, 0x11, 0x22, 0x33, 0x88, 0x99,
      0x01, 0x02, 0x03, 0x04, 0xA0, 0xA1, 0xA2, 0xA3,
  };
  server->AddRegion(0x12345, data);
  std::stringstream capture;
  CommandGetMem cmd;
  ArgParser args("getmem $eax 16");
  ThreadContext thread_context;
  thread_context.eax = 0x12345;
  interface->SetExpressionParser(
      std::make_shared<DebuggerExpressionParser>(thread_context));

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture),
                    "ff ee 44 11 22 33 88 99 01 02 03 04 a0 a1 a2 a3 ");
}

DEBUGGER_TEST_CASE(GetMemSupportsArithmeticRegisterExpressionsInAddress) {
  std::vector<uint8_t> data{
      0xFF, 0xEE, 0x44, 0x11, 0x22, 0x33, 0x88, 0x99,
      0x01, 0x02, 0x03, 0x04, 0xA0, 0xA1, 0xA2, 0xA3,
  };
  server->AddRegion(0x12345, data);
  std::stringstream capture;
  CommandGetMem cmd;
  ArgParser args("getmem ($eax + 0x45) 16");
  ThreadContext thread_context;
  thread_context.eax = 0x12300;
  interface->SetExpressionParser(
      std::make_shared<DebuggerExpressionParser>(thread_context));

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture),
                    "ff ee 44 11 22 33 88 99 01 02 03 04 a0 a1 a2 a3 ");
}

DEBUGGER_TEST_CASE(GetMemSupportsDereferencingRegisterExpressionsInAddress) {
  std::vector<uint8_t> pointer_data{
      0x45,
      0x23,
      0x01,
      0x00,
  };

  std::vector<uint8_t> data{
      0xFF, 0xEE, 0x44, 0x11, 0x22, 0x33, 0x88, 0x99,
      0x01, 0x02, 0x03, 0x04, 0xA0, 0xA1, 0xA2, 0xA3,
  };
  server->AddRegion(0x20000, pointer_data);
  server->AddRegion(0x12345, data);
  std::stringstream capture;
  CommandGetMem cmd;
  ArgParser args("getmem @$eax 16");
  ThreadContext thread_context;
  thread_context.eax = 0x20000;
  auto memory_reader =
      [&](uint32_t addr,
          uint32_t size) -> std::expected<std::vector<uint8_t>, std::string> {
    return server->GetMemoryRegion(addr, size);
  };

  interface->SetExpressionParser(std::make_shared<DebuggerExpressionParser>(
      thread_context, std::nullopt, memory_reader));

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture),
                    "ff ee 44 11 22 33 88 99 01 02 03 04 a0 a1 a2 a3 ");
}

DEBUGGER_TEST_CASE(
    GetMemSupportsArrayDereferencingRegisterExpressionsInAddress) {
  std::vector<uint8_t> pointer_data{
      0x45,
      0x23,
      0x01,
      0x00,
  };

  std::vector<uint8_t> data{
      0xFF, 0xEE, 0x44, 0x11, 0x22, 0x33, 0x88, 0x99,
      0x01, 0x02, 0x03, 0x04, 0xA0, 0xA1, 0xA2, 0xA3,
  };
  server->AddRegion(0x20004, pointer_data);
  server->AddRegion(0x12345, data);
  std::stringstream capture;
  CommandGetMem cmd;
  ArgParser args("getmem @$eax[4] 16");
  ThreadContext thread_context;
  thread_context.eax = 0x20000;
  auto memory_reader =
      [&](uint32_t addr,
          uint32_t size) -> std::expected<std::vector<uint8_t>, std::string> {
    return server->GetMemoryRegion(addr, size);
  };

  interface->SetExpressionParser(std::make_shared<DebuggerExpressionParser>(
      thread_context, std::nullopt, memory_reader));

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture),
                    "ff ee 44 11 22 33 88 99 01 02 03 04 a0 a1 a2 a3 ");
}

BOOST_AUTO_TEST_SUITE_END()

// SetMem
BOOST_FIXTURE_TEST_SUITE(SetMemTests, XBDMDebuggerInterfaceFixture)

DEBUGGER_TEST_CASE(SetMemWithNoAddressFails) {
  std::stringstream capture;
  CommandSetMem cmd;
  BOOST_REQUIRE(cmd(*interface, empty_args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture), "Missing required address argument.");
}

DEBUGGER_TEST_CASE(SetMemWithNoValueFails) {
  std::stringstream capture;
  CommandSetMem cmd;
  ArgParser args("setmem", std::vector<std::string>{"0x12345"});

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture), "Missing required data string.");
}

DEBUGGER_TEST_CASE(SetMemSucceeds) {
  std::vector<uint8_t> data{
      0xFF, 0xEE, 0x44, 0x11, 0x22, 0x33, 0x88, 0x99,
  };
  server->AddRegion(0x12345, data);
  std::stringstream capture;
  CommandSetMem cmd;
  ArgParser args("setmem",
                 std::vector<std::string>{"0x12345", "01020304A0A1A2A3"});

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  auto updated_data = server->GetMemoryRegion(0x12345, 8);
  std::vector<uint8_t> expected_data{0x01, 0x02, 0x03, 0x04,
                                     0xA0, 0xA1, 0xA2, 0xA3};
  BOOST_CHECK_EQUAL_COLLECTIONS(updated_data.begin(), updated_data.end(),
                                expected_data.begin(), expected_data.end());
}

DEBUGGER_TEST_CASE(SetMemWithoutExpressionParserFailsOnExpressions) {
  std::stringstream capture;
  CommandSetMem cmd;
  ArgParser args("setmem (0x12300 + 0x45) 01020304");

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  BOOST_CHECK_EQUAL(Trimmed(capture),
                    "Syntax error Value 0x12300 + 0x45 is not numeric");
}

DEBUGGER_TEST_CASE(SetMemSupportsSimpleExpressionsInAddress) {
  std::vector<uint8_t> data{
      0xFF, 0xEE, 0x44, 0x11, 0x22, 0x33, 0x88, 0x99,
  };
  server->AddRegion(0x12345, data);
  std::stringstream capture;
  CommandSetMem cmd;
  ArgParser args("setmem (0x12300 + 0x45) 01020304");
  interface->SetExpressionParser(std::make_shared<DebuggerExpressionParser>());

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  auto updated_data = server->GetMemoryRegion(0x12345, 4);
  std::vector<uint8_t> expected_data{0x01, 0x02, 0x03, 0x04};
  BOOST_CHECK_EQUAL_COLLECTIONS(updated_data.begin(), updated_data.end(),
                                expected_data.begin(), expected_data.end());
}

DEBUGGER_TEST_CASE(SetMemSupportsTrivialRegisterExpressionsInAddress) {
  std::vector<uint8_t> data{
      0xFF, 0xEE, 0x44, 0x11, 0x22, 0x33, 0x88, 0x99,
  };
  server->AddRegion(0x12345, data);
  std::stringstream capture;
  CommandSetMem cmd;
  ArgParser args("setmem $eax 01020304");
  ThreadContext thread_context;
  thread_context.eax = 0x12345;
  interface->SetExpressionParser(
      std::make_shared<DebuggerExpressionParser>(thread_context));

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  auto updated_data = server->GetMemoryRegion(0x12345, 4);
  std::vector<uint8_t> expected_data{0x01, 0x02, 0x03, 0x04};
  BOOST_CHECK_EQUAL_COLLECTIONS(updated_data.begin(), updated_data.end(),
                                expected_data.begin(), expected_data.end());
}

DEBUGGER_TEST_CASE(SetMemSupportsArithmeticRegisterExpressionsInAddress) {
  std::vector<uint8_t> data{
      0xFF, 0xEE, 0x44, 0x11, 0x22, 0x33, 0x88, 0x99,
  };
  server->AddRegion(0x12345, data);
  std::stringstream capture;
  CommandSetMem cmd;
  ArgParser args("setmem ($eax + 0x45) 01020304");
  ThreadContext thread_context;
  thread_context.eax = 0x12300;
  interface->SetExpressionParser(
      std::make_shared<DebuggerExpressionParser>(thread_context));

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  auto updated_data = server->GetMemoryRegion(0x12345, 4);
  std::vector<uint8_t> expected_data{0x01, 0x02, 0x03, 0x04};
  BOOST_CHECK_EQUAL_COLLECTIONS(updated_data.begin(), updated_data.end(),
                                expected_data.begin(), expected_data.end());
}

DEBUGGER_TEST_CASE(SetMemSupportsDereferencingRegisterExpressionsInAddress) {
  std::vector<uint8_t> pointer_data{
      0x45,
      0x23,
      0x01,
      0x00,
  };

  std::vector<uint8_t> data{
      0xFF, 0xEE, 0x44, 0x11, 0x22, 0x33, 0x88, 0x99,
  };
  server->AddRegion(0x20000, pointer_data);
  server->AddRegion(0x12345, data);
  std::stringstream capture;
  CommandSetMem cmd;
  ArgParser args("setmem @$eax 01020304");
  ThreadContext thread_context;
  thread_context.eax = 0x20000;
  auto memory_reader =
      [&](uint32_t addr,
          uint32_t size) -> std::expected<std::vector<uint8_t>, std::string> {
    return server->GetMemoryRegion(addr, size);
  };

  interface->SetExpressionParser(std::make_shared<DebuggerExpressionParser>(
      thread_context, std::nullopt, memory_reader));

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  auto updated_data = server->GetMemoryRegion(0x12345, 4);
  std::vector<uint8_t> expected_data{0x01, 0x02, 0x03, 0x04};
  BOOST_CHECK_EQUAL_COLLECTIONS(updated_data.begin(), updated_data.end(),
                                expected_data.begin(), expected_data.end());
}

DEBUGGER_TEST_CASE(
    SetMemSupportsArrayDereferencingRegisterExpressionsInAddress) {
  std::vector<uint8_t> pointer_data{
      0x45,
      0x23,
      0x01,
      0x00,
  };

  std::vector<uint8_t> data{
      0xFF, 0xEE, 0x44, 0x11, 0x22, 0x33, 0x88, 0x99,
  };
  server->AddRegion(0x20004, pointer_data);
  server->AddRegion(0x12345, data);
  std::stringstream capture;
  CommandSetMem cmd;
  ArgParser args("setmem @$eax[4] 01020304");
  ThreadContext thread_context;
  thread_context.eax = 0x20000;
  auto memory_reader =
      [&](uint32_t addr,
          uint32_t size) -> std::expected<std::vector<uint8_t>, std::string> {
    return server->GetMemoryRegion(addr, size);
  };

  interface->SetExpressionParser(std::make_shared<DebuggerExpressionParser>(
      thread_context, std::nullopt, memory_reader));

  BOOST_REQUIRE(cmd(*interface, args, capture) == Command::HANDLED);

  auto updated_data = server->GetMemoryRegion(0x12345, 4);
  std::vector<uint8_t> expected_data{0x01, 0x02, 0x03, 0x04};
  BOOST_CHECK_EQUAL_COLLECTIONS(updated_data.begin(), updated_data.end(),
                                expected_data.begin(), expected_data.end());
}

BOOST_AUTO_TEST_SUITE_END()
