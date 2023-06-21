#include "loader.h"

#include <algorithm>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <iostream>

#include "dll_linker.h"
#include "dxt_library.h"
#include "dyndxt_requests.h"
#include "util/logging.h"
#include "util/parsing.h"
#include "winapi/winnt.h"
#include "xbdm_exports.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"
#include "xbox/xbox_interface.h"
#include "xboxkrnl_exports.h"

// The pre-generated binaries to be installed on the XBOX to facilitate
// DynamicDXT loading.
#include "bootstrap_l1_xbox.h"
#include "bootstrap_l2_xbox.h"
#include "dynamic_dxt_loader_xbox.h"

namespace DynDXTLoader {

static constexpr uint32_t kDmAllocatePoolWithTagOrdinal = 2;
static constexpr uint32_t kDmFreePoolOrdinal = 9;
static constexpr uint32_t kDmRegisterCommandProcessorOrdinal = 30;

// DmResumeThread is used because the xbdm handler takes a single DWORD
// parameter and does minimal processing of the input and response.
static constexpr uint32_t kDmResumeThreadOrdinal = 35;

static bool SetMemoryUnsafe(const std::shared_ptr<XBDMContext>& context,
                            uint32_t address, const std::vector<uint8_t>& data);
static bool InvokeL1Bootstrap(const std::shared_ptr<XBDMContext>& context,
                              const uint32_t parameter);

static bool FetchExport(const std::shared_ptr<XBDMDebugger>& debugger,
                        uint32_t ordinal,
                        std::map<uint32_t, uint32_t>& ordinal_to_address,
                        uint32_t image_base);
static bool FetchExport(const std::shared_ptr<XBDMDebugger>& debugger,
                        uint32_t ordinal,
                        std::map<uint32_t, uint32_t>& ordinal_to_address,
                        uint32_t image_base, uint32_t& resolved_address);

Loader* Loader::singleton_ = nullptr;

bool Loader::Bootstrap(XBOXInterface& interface) {
  if (!singleton_) {
    singleton_ = new Loader();
  }

  // See if the Dynamic DXT loader is already running.
  auto request = std::make_shared<DynDXTLoader::InvokeMultiline>("ddxt!hello");
  interface.SendCommandSync(request);
  if (request->IsOK()) {
    return true;
  }

  auto load_start = std::chrono::high_resolution_clock::now();
  bool ret = singleton_->InjectLoader(interface);
  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed =
      std::chrono::duration<double, std::milli>(now - load_start).count();
  LOG(info) << "Bootstrap install " << (ret ? "succeeded" : "failed") << " in "
            << elapsed << " milliseconds.";

  if (!ret) {
    delete singleton_;
    singleton_ = nullptr;
  }
  return ret;
}

bool Loader::Load(XBOXInterface& interface, const std::string& path) {
  if (!singleton_ && !Bootstrap(interface)) {
    LOG(error) << "Failed to bootstrap handler loader.";
    return false;
  }
  return singleton_->LoadDLL(interface, path);
}

bool Loader::InjectLoader(XBOXInterface& interface) {
  auto debugger = interface.Debugger();
  if (!debugger) {
    LOG(error) << "Debugger not attached.";
    return false;
  }

  if (!FetchBaseAddress(debugger, "xbdm.dll")) {
    return false;
  }
  if (!FetchBaseAddress(debugger, "xboxkrnl.exe")) {
    return false;
  }

  module_export_names_["xbdm.dll"] = XBDM_Exports;
  module_export_names_["xboxkrnl.exe"] = XBOXKRNL_Exports;

  {
#define RESOLVE(ordinal, table, base)                   \
  if (!FetchExport(debugger, ordinal, table, base)) {   \
    LOG(error) << "Failed to resolve export " #ordinal; \
    return false;                                       \
  }

    auto xbdm_base_addr = module_base_addresses_["xbdm.dll"];
    auto& xbdm_exports = module_exports_["xbdm.dll"];

    RESOLVE(XBDM_DmResumeThread, xbdm_exports, xbdm_base_addr)
    RESOLVE(XBDM_DmAllocatePoolWithTag, xbdm_exports, xbdm_base_addr)
    RESOLVE(XBDM_DmRegisterCommandProcessor, xbdm_exports, xbdm_base_addr)

#undef RESOLVE
  }

  auto xbdm = interface.Context();

  std::vector<uint8_t> bootstrap_l1(
      kBootstrapL1,
      kBootstrapL1 + (sizeof(kBootstrapL1) / sizeof(kBootstrapL1[0])));

  const uint32_t kDmResumeThread = GetExport("xbdm.dll", XBDM_DmResumeThread);
  auto original_function =
      debugger->GetMemory(kDmResumeThread, bootstrap_l1.size());
  if (!original_function.has_value()) {
    LOG(error) << "Failed to fetch target function.";
    return false;
  }

  if (!SetMemoryUnsafe(xbdm, kDmResumeThread, bootstrap_l1)) {
    LOG(error) << "Failed to patch target function with l1 bootstrap.";
    return false;
  }

  bool ret = true;
  if (!InstallL2Loader(debugger, xbdm)) {
    ret = false;
    goto cleanup;
  }

cleanup:
  if (!SetMemoryUnsafe(xbdm, kDmResumeThread, original_function.value())) {
    LOG(error) << "Failed to restore target function.";
    return false;
  }

  if (ret) {
    if (!InstallDynamicDXTLoader(interface)) {
      return false;
    }
  }

  return ret;
}

bool Loader::InstallL2Loader(const std::shared_ptr<XBDMDebugger>& debugger,
                             const std::shared_ptr<XBDMContext>& context) {
  std::vector<uint8_t> bootstrap_l2(
      kBootstrapL2,
      kBootstrapL2 + (sizeof(kBootstrapL2) / sizeof(kBootstrapL2[0])));

  {
    // Patch up the L2 bootstrap import table.
    // Keep in sync with bootstrap_l2.asm.
    auto* import_table = reinterpret_cast<uint32_t*>(bootstrap_l2.data() +
                                                     bootstrap_l2.size() - 8);
    *import_table++ = GetExport("xbdm.dll", XBDM_DmAllocatePoolWithTag);
    *import_table++ = GetExport("xbdm.dll", XBDM_DmRegisterCommandProcessor);
  }

  uint32_t l2_entrypoint =
      L1BootstrapAllocatePool(debugger, context, bootstrap_l2.size());
  if (!l2_entrypoint) {
    LOG(error) << "Failed to allocate memory for l2 bootstrap loader.";
    return false;
  }

  // Upload the L2 bootloader.
  auto load_start = std::chrono::high_resolution_clock::now();
  if (!SetMemoryUnsafe(context, l2_entrypoint, bootstrap_l2)) {
    LOG(error) << "Failed to upload l2 bootstrap loader.";
    return 0;
  }
  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed =
      std::chrono::duration<double, std::milli>(now - load_start).count();
  LOG(info) << "L2 bootstrap installed at 0x" << std::hex << std::setfill('0')
            << std::setw(8) << l2_entrypoint << std::dec << " "
            << bootstrap_l2.size() << " bytes in " << elapsed
            << " milliseconds "
            << ((double)bootstrap_l2.size() * 1000 / elapsed) << " Bps.";

  // Instruct the L1 loader to call into the memory allocated by the call above.
  if (!SetL1LoaderExecuteMode(context)) {
    // TODO: Free pool.
    return false;
  }

  if (!InvokeL1Bootstrap(context, l2_entrypoint)) {
    // TODO: Free pool.
    LOG(error) << "Failed to initialize Dynamic DXT loader.";
    return false;
  }

  return true;
}

bool Loader::LoadDLL(XBOXInterface& interface, const std::string& path) {
  auto debugger = interface.Debugger();
  if (!debugger) {
    LOG(error) << "Debugger not attached.";
    return false;
  }

  FILE* fp = fopen(path.c_str(), "rb");
  if (!fp) {
    LOG(error) << "Failed to open '" << path << "'";
    return false;
  }

  std::vector<uint8_t> data;
  uint8_t buf[4096];
  while (!feof(fp)) {
    size_t bytes_read = fread(buf, 1, sizeof(buf), fp);
    if (bytes_read) {
      std::copy(buf, buf + bytes_read, std::back_inserter(data));
    } else if (!feof(fp) && ferror(fp)) {
      LOG(error) << "Failed to read '" << path << "'";
      return false;
    }
  }

  auto request = std::make_shared<DynDXTLoader::LoadDynDXT>(data);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    LOG(error) << *request;
    return false;
  }

  std::cout << *request << std::endl;

  return true;
}

static uint32_t L2BootstrapAllocate(XBOXInterface& interface,
                                    uint32_t image_size) {
  char alloc_request[64];
  snprintf(alloc_request, 64, "ldxt!a s=%u", image_size);
  auto request = std::make_shared<DynDXTLoader::InvokeSimple>(alloc_request);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    LOG(error) << "Failed to allocate " << image_size << " bytes for Loader. "
               << *request;
    return 0;
  }

  auto response = request->message;
  auto base_param = response.find("base=");
  if (base_param == std::string::npos) {
    LOG(error) << "Failed to parse base param from response.";
    return 0;
  }

  uint32_t target = 0;
  if (!MaybeParseHexInt(target, response.substr(base_param + 5))) {
    LOG(error) << "Invalid base param in response. " << response;
    return 0;
  }

  return target;
}

static bool L2BootstrapInstall(XBOXInterface& interface, uint32_t entrypoint,
                               const std::vector<uint8_t>& image) {
  auto load_start = std::chrono::high_resolution_clock::now();

  char install_request[64];
  snprintf(install_request, 64, "ldxt!i e=%u", entrypoint);
  auto request =
      std::make_shared<DynDXTLoader::InvokeSendBinary>(install_request, image);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    LOG(error) << "Failed to install DynDXT loader. " << *request;
    return false;
  }

  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed =
      std::chrono::duration<double, std::milli>(now - load_start).count();
  LOG(info) << "Loader installed at 0x" << std::hex << std::setfill('0')
            << std::setw(8) << entrypoint << std::dec << " " << image.size()
            << " bytes in " << elapsed << " milliseconds "
            << ((double)image.size() * 1000 / elapsed) << " Bps.";

  return true;
}

bool Loader::InstallDynamicDXTLoader(XBOXInterface& interface) {
  auto stream = std::make_shared<boost::interprocess::bufferstream>(
      (char*)kDynDXTLoader, sizeof(kDynDXTLoader));
  DXTLibrary lib(std::dynamic_pointer_cast<std::istream>(stream),
                 "BundledDXTLoader");
  if (!lib.Parse()) {
    LOG(error) << "Failed to load dynamic dxt loader DLL.";
    return false;
  }

  auto debugger = interface.Debugger();
  auto context = interface.Context();

  for (auto& dll_imports : lib.GetImports()) {
    if (!ResolveImports(debugger, dll_imports.first, dll_imports.second)) {
      return false;
    }
  }

  auto target = L2BootstrapAllocate(interface, lib.GetImageSize());
  if (!target) {
    return false;
  }

  if (!lib.Relocate(target)) {
    // TODO: Free pool.
    return false;
  }

  if (!L2BootstrapInstall(interface, lib.GetEntrypoint(), lib.GetImage())) {
    // TODO: Free pool.
    return false;
  }

  // TODO: Call TLS initializers.
  for (auto callback : lib.GetTLSInitializers()) {
    LOG(error) << "TLS callback functionality not implemented.";
    // TODO: Free pool.
    return false;
  }

  return true;
}

uint32_t Loader::L1BootstrapAllocatePool(
    const std::shared_ptr<XBDMDebugger>& debugger,
    const std::shared_ptr<XBDMContext>& context, uint32_t size) const {
  // The requested size and target address is stored in the last 4 bytes of the
  // L1 bootloader.
  const uint32_t io_address =
      GetExport("xbdm.dll", XBDM_DmResumeThread) + sizeof(kBootstrapL1) - 4;

  {
    auto data = reinterpret_cast<uint8_t*>(&size);
    std::vector<uint8_t> size_request(data, data + 4);
    if (!SetMemoryUnsafe(context, io_address, size_request)) {
      LOG(error) << "Failed to set allocation size.";
      return 0;
    }
  }

  if (!InvokeL1Bootstrap(context,
                         GetExport("xbdm.dll", XBDM_DmAllocatePoolWithTag))) {
    LOG(error) << "Failed to allocate memory.";
    return 0;
  }

  // The target address is stored in the last 4 bytes of the L1 bootloader.
  auto target_address = debugger->GetDWORD(io_address);
  if (!target_address.has_value()) {
    LOG(error) << "Failed to fetch allocated memory address.";
    return 0;
  }

  uint32_t address = target_address.value();
  return address;
}

bool Loader::SetL1LoaderExecuteMode(
    const std::shared_ptr<XBDMContext>& context) const {
  // Set the L1 loader into execute mode.
  const uint32_t io_address =
      GetExport("xbdm.dll", XBDM_DmResumeThread) + sizeof(kBootstrapL1) - 4;

  uint32_t size = 0;
  auto data = reinterpret_cast<uint8_t*>(&size);
  std::vector<uint8_t> size_request(data, data + 4);
  if (!SetMemoryUnsafe(context, io_address, size_request)) {
    LOG(error) << "Failed to set L1 loader to execute mode.";
    return false;
  }

  return true;
}

bool Loader::ResolveImports(const std::shared_ptr<XBDMDebugger>& debugger,
                            const std::string& module_name,
                            std::vector<DXTLibraryImport>& imports) {
  if (!FetchBaseAddress(debugger, module_name)) {
    return false;
  }

  uint32_t base_address = module_base_addresses_[module_name];
  auto exports = module_exports_.find(module_name);

  // FetchBaseAddress is expected to populate the export table with an empty
  // vector.
  assert(exports != module_exports_.end());

  auto& export_table = exports->second;
  for (auto& import : imports) {
    uint32_t ordinal = import.ordinal;

    // Resolve name to ordinal.
    if (!import.import_name.empty()) {
      auto name_to_ordinal_table = module_export_names_.find(module_name);
      if (name_to_ordinal_table == module_export_names_.end()) {
        LOG(error) << "Import from " << module_name << " by name "
                   << import.import_name
                   << " but no name mapping table exists for that module.";
        return false;
      }

      auto entry = name_to_ordinal_table->second.find(import.import_name);
      if (entry == name_to_ordinal_table->second.end()) {
        LOG(error) << "Import from " << module_name << " by unknown name '"
                   << import.import_name << "'.";
      }

      ordinal = entry->second;
    }

    if (!FetchExport(debugger, ordinal, export_table, base_address,
                     import.real_address)) {
      LOG(error) << "Failed to resolve import " << module_name << " " << import;
      return false;
    }
  }

  return true;
}

bool Loader::FetchBaseAddress(const std::shared_ptr<XBDMDebugger>& debugger,
                              const std::string& module_name) {
  if (module_base_addresses_.find(module_name) !=
      module_base_addresses_.end()) {
    return true;
  }

  auto module = debugger->GetModule(module_name);
  if (!module) {
    LOG(error) << "Failed to retrieve module info for '" << module_name << "'.";
    return false;
  }

  module_base_addresses_[module_name] = module->base_address;
  module_exports_[module_name] = {};
  return true;
}

uint32_t Loader::GetExport(const std::string& module, uint32_t ordinal) const {
  auto module_export = module_exports_.find(module);
  if (module_export == module_exports_.end()) {
    LOG(error) << "Failed to look up export " << module << " @ " << ordinal
               << " no such module.";
    return 0;
  }

  auto entry = module_export->second.find(ordinal);
  if (entry == module_export->second.end()) {
    LOG(error) << "Failed to look up export " << module << " @ " << ordinal
               << " no such entry.";
    return 0;
  }

  return entry->second;
}

static bool SetMemoryUnsafe(const std::shared_ptr<XBDMContext>& context,
                            uint32_t address,
                            const std::vector<uint8_t>& data) {
  if (data.size() <= SetMem::kMaximumDataSize) {
    auto request = std::make_shared<SetMem>(address, data);
    context->SendCommandSync(request);
    return request->IsOK();
  }

  auto first = data.cbegin();
  auto last = first + SetMem::kMaximumDataSize;
  while (first != data.end()) {
    std::vector<uint8_t> slice(first, last);
    auto request = std::make_shared<SetMem>(address, slice);
    context->SendCommandSync(request);
    if (!request->IsOK()) {
      return false;
    }

    first = last;
    last += SetMem::kMaximumDataSize;
    if (last > data.end()) {
      last = data.end();
    }
    address += SetMem::kMaximumDataSize;
  }
  return true;
}

static bool InvokeL1Bootstrap(const std::shared_ptr<XBDMContext>& context,
                              const uint32_t parameter) {
  auto request = std::make_shared<::Resume>(parameter);
  context->SendCommandSync(request);
  return request->IsOK();
}

static bool FetchExport(const std::shared_ptr<XBDMDebugger>& debugger,
                        uint32_t ordinal,
                        std::map<uint32_t, uint32_t>& ordinal_to_address,
                        uint32_t image_base) {
  uint32_t ignored;
  return FetchExport(debugger, ordinal, ordinal_to_address, image_base,
                     ignored);
}

static bool FetchExport(const std::shared_ptr<XBDMDebugger>& debugger,
                        uint32_t ordinal,
                        std::map<uint32_t, uint32_t>& ordinal_to_address,
                        uint32_t image_base, uint32_t& resolved_address) {
  auto existing_entry = ordinal_to_address.find(ordinal);
  if (existing_entry != ordinal_to_address.end()) {
    resolved_address = existing_entry->second;
    return true;
  }

  auto address = GetExportAddress(debugger, ordinal, image_base);
  if (!address.has_value()) {
    return false;
  }

  resolved_address = address.value();
  ordinal_to_address[ordinal] = resolved_address;

  return true;
}

}  // namespace DynDXTLoader
