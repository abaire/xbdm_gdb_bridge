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

namespace DynDXTLoader {

//! Performs bootstrap loading of XBDM handler plugins.
class Loader {
 public:
  // Note: The target should be fully halted before calling this method.
  static bool Bootstrap(XBOXInterface& interface);

  // Loads a dynamic DXT handler DLL.
  static bool Load(XBOXInterface& interface, const std::string& path);

  static bool Install(XBOXInterface& interface,
                      const std::vector<uint8_t>& data);

 private:
  bool InjectLoader(XBOXInterface& interface);

  bool InstallDynDXT(XBOXInterface& interface, const std::string& path);

  bool InstallDynDXT(XBOXInterface& interface,
                     const std::vector<uint8_t>& data);

  //! Installs the L2 bootloader.
  bool InstallL2Loader(const std::shared_ptr<XBDMDebugger>& debugger,
                       const std::shared_ptr<XBDMContext>& context);

  // Installs the Dynamic DXT loader using the L2 bootloader.
  bool InstallDynamicDXTLoader(XBOXInterface& interface);

  // Invoke the L1 bootstrap to allocate memory. Note that this assumes the
  // `resume` command has already been patched with the L1 bootstrap.
  [[nodiscard]] uint32_t L1BootstrapAllocatePool(
      const std::shared_ptr<XBDMDebugger>& debugger,
      const std::shared_ptr<XBDMContext>& context, uint32_t size) const;

  [[nodiscard]] bool SetL1LoaderExecuteMode(
      const std::shared_ptr<XBDMContext>& context) const;

  // Resolves a list of import thunks to actual addresses on the XBOX.
  bool ResolveImports(XBOXInterface& interface, const std::string& module_name,
                      std::vector<DXTLibraryImport>& imports);

  bool FetchBaseAddress(const std::shared_ptr<XBDMDebugger>& debugger,
                        const std::string& module_name);

  [[nodiscard]] uint32_t GetExport(const std::string& module,
                                   uint32_t ordinal) const;

 private:
  static Loader* singleton_;

  // Maps module name to base address.
  std::map<std::string, uint32_t> module_base_addresses_;

  // Maps a module name to a map of export name to ordinal number.
  std::map<std::string, std::map<std::string, uint32_t>> module_export_names_;

  // Maps module name to a map of ordinal number to function address.
  std::map<std::string, std::map<uint32_t, uint32_t>> module_exports_;
};

}  // namespace DynDXTLoader

#endif  // XBDM_GDB_BRIDGE_HANDLERLOADER_H
