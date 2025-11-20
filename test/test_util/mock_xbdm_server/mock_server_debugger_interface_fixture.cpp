#include "mock_server_debugger_interface_fixture.h"

#include <boost/test/unit_test.hpp>

#include "configure_test.h"
#include "test_util/mock_xbdm_server/mock_xbdm_server.h"
#include "xbox/debugger/debugger_xbox_interface.h"

namespace xbdm_gdb_bridge::testing {

XBDMDebuggerInterfaceFixture::XBDMDebuggerInterfaceFixture()
    : empty_args("empty_args") {
  server = std::make_unique<MockXBDMServer>(TEST_MOCK_XBDM_PORT);
  BOOST_REQUIRE(server->Start());

  interface =
      std::make_shared<DebuggerXBOXInterface>("Client", server->GetAddress());
  interface->Start();
}

XBDMDebuggerInterfaceFixture::~XBDMDebuggerInterfaceFixture() {
  if (interface) {
    interface->Stop();
    interface.reset();
  }

  server->Stop();
  server.reset();
}

void XBDMDebuggerInterfaceFixture::AwaitQuiescence() const {
  // Ping pong the peer SelectThreads to avoid a situation where one generates
  // new work for the other after a period of quiescence.
  for (int i = 0; i < 4; ++i) {
    server->AwaitQuiescence();
    interface->AwaitQuiescence();
  }
}

std::string XBDMDebuggerInterfaceFixture::Trimmed(
    const std::stringstream& captured) {
  auto s = captured.str();
  if (s.empty()) {
    return s;
  }

  // Check for \n
  if (s.back() == '\n') {
    s.pop_back();
    // If it was \r\n, remove the \r as well
    if (!s.empty() && s.back() == '\r') {
      s.pop_back();
    }
  }
  // Handle the \r-only case
  else if (s.back() == '\r') {
    s.pop_back();
  }
  return s;
}

}  // namespace xbdm_gdb_bridge::testing
