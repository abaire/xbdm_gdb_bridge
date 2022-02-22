#ifndef XBDM_GDB_BRIDGE_HANDLERLOADER_H
#define XBDM_GDB_BRIDGE_HANDLERLOADER_H

#include <map>
#include <memory>
#include <string>

class COFFLoader;
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

  // Injects the dynamic dxt loader, returning the address of the entrypoint
  // method or 0 on error.
  uint32_t InstallDynamicDXTLoader(
      const std::shared_ptr<XBDMDebugger>& debugger,
      const std::shared_ptr<XBDMContext>& context);

  // Invoke the L1 bootstrap to allocate memory. Note that this assumes the
  // `resume` command has already been patched with the L1 bootstrap.
  [[nodiscard]] uint32_t AllocatePool(
      const std::shared_ptr<XBDMDebugger>& debugger,
      const std::shared_ptr<XBDMContext>& context, uint32_t size) const;

  bool ResolveExterns(const std::shared_ptr<XBDMDebugger>& debugger,
                      const std::shared_ptr<COFFLoader>&,
                      std::map<std::string, uint32_t>& externs);

 private:
  static HandlerLoader* singleton_;

  uint32_t l2_bootstrap_addr_{0};

  uint32_t xbdm_base_addr_{0};
  uint32_t xboxkrnl_base_addr_{0};

  std::map<uint32_t, uint32_t> xbdm_exports_;
  std::map<uint32_t, uint32_t> xboxkrnl_exports_;
};

#endif  // XBDM_GDB_BRIDGE_HANDLERLOADER_H
