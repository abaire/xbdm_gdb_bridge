#ifndef XBDM_GDB_BRIDGE_MOCK_SERVER_DEBUGGER_INTERFACE_FIXTURE_H
#define XBDM_GDB_BRIDGE_MOCK_SERVER_DEBUGGER_INTERFACE_FIXTURE_H

#include <memory>
#include <vector>

#include "test_util/mock_xbdm_server/mock_xbdm_server.h"
#include "util/parsing.h"

class XBOXInterface;

namespace xbdm_gdb_bridge::testing {

struct XBDMDebuggerInterfaceFixture {
  XBDMDebuggerInterfaceFixture();
  virtual ~XBDMDebuggerInterfaceFixture();

  /**
   * Blocks until the server and interface are no longer processing commands.
   */
  void AwaitQuiescence() const;

  /** Returns the contents of a stringstream without the trailing std::endl. */
  static std::string Trimmed(const std::stringstream& captured);

  std::shared_ptr<XBOXInterface> interface;
  std::unique_ptr<MockXBDMServer> server;
  uint16_t port = 0;

  const ArgParser empty_args;
};

}  // namespace xbdm_gdb_bridge::testing

#endif  // XBDM_GDB_BRIDGE_MOCK_SERVER_DEBUGGER_INTERFACE_FIXTURE_H
