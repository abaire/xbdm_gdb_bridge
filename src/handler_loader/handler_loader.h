#ifndef XBDM_GDB_BRIDGE_HANDLERLOADER_H
#define XBDM_GDB_BRIDGE_HANDLERLOADER_H

#include <map>
#include <memory>
#include <string>
#include <vector>

struct DXTLibraryImport;
class XBDMContext;
class XBDMDebugger;
class XBOXInterface;

//! Performs bootstrap loading of XBDM handler plugins.
class HandlerLoader {
 public:
  // Note: The target should be fully halted before calling this method.
  static bool Bootstrap(XBOXInterface& interface);

  // Loads a dynamic DXT handler DLL.
  static bool Load(XBOXInterface& interface, const std::string& path);

 private:
  bool InjectLoader(XBOXInterface& interface);

  bool LoadDLL(XBOXInterface& interface, const std::string& path);

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

  [[nodiscard]] bool SetL1LoaderExecuteMode(
      const std::shared_ptr<XBDMContext>& context) const;

  bool ResolveImports(const std::shared_ptr<XBDMDebugger>& debugger,
                      const std::string& module_name,
                      std::vector<DXTLibraryImport>& imports);

  bool FetchBaseAddress(const std::shared_ptr<XBDMDebugger>& debugger,
                        const std::string& module_name);

  [[nodiscard]] uint32_t GetExport(const std::string& module,
                                   uint32_t ordinal) const;

  bool FillLoaderExportRegistry(const std::shared_ptr<XBDMDebugger>& debugger,
                                const std::shared_ptr<XBDMContext>& context);

 private:
  static HandlerLoader* singleton_;

  // Maps module name to base address.
  std::map<std::string, uint32_t> module_base_addresses_;

  // Maps a module name to a map of export name to ordinal number.
  std::map<std::string, std::map<std::string, uint32_t>> module_export_names_;

  // Maps module name to a map of ordinal number to function address.
  std::map<std::string, std::map<uint32_t, uint32_t>> module_exports_;
};

#endif  // XBDM_GDB_BRIDGE_HANDLERLOADER_H
