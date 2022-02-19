#ifndef XBDM_GDB_BRIDGE_HANDLERLOADER_H
#define XBDM_GDB_BRIDGE_HANDLERLOADER_H

#include <memory>

class XBOXInterface;

//! Performs bootstrap loading of XBDM handler plugins.
class HandlerLoader {
 public:
  // Note: The target should be fully halted before calling this method.
  static bool Bootstrap(XBOXInterface &interface);

 private:
  bool InjectLoader(XBOXInterface &interface);

 private:
  static HandlerLoader *singleton_;

  bool loader_injected_{false};
};

#endif  // XBDM_GDB_BRIDGE_HANDLERLOADER_H
