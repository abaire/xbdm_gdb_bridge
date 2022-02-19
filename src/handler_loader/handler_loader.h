#ifndef XBDM_GDB_BRIDGE_HANDLERLOADER_H
#define XBDM_GDB_BRIDGE_HANDLERLOADER_H

#include <memory>

class XBDMContext;
class XBDMDebugger;
class XBOXInterface;

//! Performs bootstrap loading of XBDM handler plugins.
class HandlerLoader {
 public:
  // Note: The target should be fully halted before calling this method.
  static bool Bootstrap(XBOXInterface& interface);

 private:
  bool InjectLoader(XBOXInterface& interface);
  bool ResolveXBDMExports(const std::shared_ptr<XBDMDebugger>& debugger,
                          uint32_t xbdm_base);
  bool ResolveKernelExports(const std::shared_ptr<XBDMDebugger>& debugger,
                            uint32_t xboxkrnl_base);
  bool InstallL2Bootstrap(const std::shared_ptr<XBDMDebugger>& debugger,
                          const std::shared_ptr<XBDMContext>& context);

 private:
  static HandlerLoader* singleton_;

  // XBDM functions.
  uint32_t resume_thread_{0};
  uint32_t allocate_pool_with_tag_{0};
  uint32_t free_pool_{0};
  uint32_t register_command_processor_{0};

  // xboxkrnl functions.
  uint32_t xe_load_section_{0};
  uint32_t xe_unload_section_{0};
  uint32_t mm_dbg_allocate_memory_{0};
  uint32_t mm_dbg_free_memory_{0};

  uint32_t l2_bootstrap_addr_{0};
};

#endif  // XBDM_GDB_BRIDGE_HANDLERLOADER_H
