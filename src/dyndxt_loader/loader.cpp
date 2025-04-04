#include "loader.h"

#include <algorithm>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <iostream>
#include <list>
#include <map>
#include <vector>

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
#include "ResolveExportList.h"
#include "bootstrap_l1_xbox.h"
#include "bootstrap_l2_xbox.h"
#include "dynamic_dxt_loader_xbox.h"

namespace DynDXTLoader {

constexpr const char kLoggingTagTracer[] = "DDXTLOADER";
#define LOG_LOADER(lvl) LOG_TAGGED(lvl, kLoggingTagTracer)

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
  LOG_LOADER(info) << "Bootstrap install " << (ret ? "succeeded" : "failed")
                   << " in " << elapsed << " milliseconds.";

  if (!ret) {
    delete singleton_;
    singleton_ = nullptr;
  }
  return ret;
}

bool Loader::Load(XBOXInterface& interface, const std::string& path) {
  if (!singleton_ && !Bootstrap(interface)) {
    LOG_LOADER(error) << "Failed to bootstrap handler loader.";
    return false;
  }
  return singleton_->InstallDynDXT(interface, path);
}

bool Loader::Install(XBOXInterface& interface,
                     const std::vector<uint8_t>& data) {
  if (!singleton_ && !Bootstrap(interface)) {
    LOG_LOADER(error) << "Failed to bootstrap handler loader.";
    return false;
  }
  return singleton_->InstallDynDXT(interface, data);
}

bool Loader::InjectLoader(XBOXInterface& interface) {
  auto debugger = interface.Debugger();
  if (!debugger) {
    LOG_LOADER(error) << "Debugger not attached.";
    return false;
  }

  if (!FetchBaseAddress(debugger, "xbdm.dll")) {
    LOG_LOADER(error)
        << "Failed to fetch xbdm.dll module info. Is the debugger /attach'ed?";
    return false;
  }
  if (!FetchBaseAddress(debugger, "xboxkrnl.exe")) {
    return false;
  }

  module_export_names_["xbdm.dll"] = XBDM_Exports;
  module_export_names_["xboxkrnl.exe"] = XBOXKRNL_Exports;

  {
#define RESOLVE(ordinal, table, base)                          \
  if (!FetchExport(debugger, ordinal, table, base)) {          \
    LOG_LOADER(error) << "Failed to resolve export " #ordinal; \
    return false;                                              \
  }

    auto xbdm_base_addr = module_base_addresses_["xbdm.dll"];
    auto& xbdm_exports = module_exports_["xbdm.dll"];

    RESOLVE(XBDM_DmResumeThread, xbdm_exports, xbdm_base_addr)
    RESOLVE(XBDM_DmAllocatePoolWithTag, xbdm_exports, xbdm_base_addr)
    RESOLVE(XBDM_DmFreePool, xbdm_exports, xbdm_base_addr)
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
    LOG_LOADER(error) << "Failed to fetch target function.";
    return false;
  }

  if (!SetMemoryUnsafe(xbdm, kDmResumeThread, bootstrap_l1)) {
    LOG_LOADER(error) << "Failed to patch target function with l1 bootstrap.";
    return false;
  }

  bool ret = true;
  if (!InstallL2Loader(debugger, xbdm)) {
    ret = false;
    goto cleanup;
  }

cleanup:
  if (!SetMemoryUnsafe(xbdm, kDmResumeThread, original_function.value())) {
    LOG_LOADER(error) << "Failed to restore target function.";
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
                                                     bootstrap_l2.size() - 12);
    *import_table++ = GetExport("xbdm.dll", XBDM_DmFreePool);
    *import_table++ = GetExport("xbdm.dll", XBDM_DmAllocatePoolWithTag);
    *import_table++ = GetExport("xbdm.dll", XBDM_DmRegisterCommandProcessor);
  }

  uint32_t l2_entrypoint =
      L1BootstrapAllocatePool(debugger, context, bootstrap_l2.size());
  if (!l2_entrypoint) {
    LOG_LOADER(error) << "Failed to allocate memory for l2 bootstrap loader.";
    return false;
  }

  // Upload the L2 bootloader.
  auto load_start = std::chrono::high_resolution_clock::now();
  if (!SetMemoryUnsafe(context, l2_entrypoint, bootstrap_l2)) {
    LOG_LOADER(error) << "Failed to upload l2 bootstrap loader.";
    return false;
  }
  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed =
      std::chrono::duration<double, std::milli>(now - load_start).count();
  LOG_LOADER(info) << "L2 bootstrap installed at 0x" << std::hex
                   << std::setfill('0') << std::setw(8) << l2_entrypoint
                   << std::dec << " " << bootstrap_l2.size() << " bytes in "
                   << elapsed << " milliseconds "
                   << ((double)bootstrap_l2.size() * 1000 / elapsed) << " Bps.";

  // Instruct the L1 loader to call into the memory allocated by the call above.
  if (!SetL1LoaderExecuteMode(context)) {
    // TODO: Free pool.
    return false;
  }

  if (!InvokeL1Bootstrap(context, l2_entrypoint)) {
    // TODO: Free pool.
    LOG_LOADER(error) << "Failed to initialize Dynamic DXT loader.";
    return false;
  }

  return true;
}

bool Loader::InstallDynDXT(XBOXInterface& interface, const std::string& path) {
  auto debugger = interface.Debugger();
  if (!debugger) {
    LOG_LOADER(error) << "Debugger not attached.";
    return false;
  }

  FILE* fp = fopen(path.c_str(), "rb");
  if (!fp) {
    LOG_LOADER(error) << "Failed to open '" << path << "'";
    return false;
  }

  std::vector<uint8_t> data;
  uint8_t buf[4096];
  while (!feof(fp)) {
    size_t bytes_read = fread(buf, 1, sizeof(buf), fp);
    if (bytes_read) {
      std::copy(buf, buf + bytes_read, std::back_inserter(data));
    } else if (!feof(fp) && ferror(fp)) {
      LOG_LOADER(error) << "Failed to read '" << path << "'";
      return false;
    }
  }

  return InstallDynDXT(interface, data);
}

bool Loader::InstallDynDXT(XBOXInterface& interface,
                           const std::vector<uint8_t>& data) {
  if (data.empty()) {
    LOG_LOADER(error) << "Empty DynDXT data.";
    return false;
  }

  auto load_start = std::chrono::high_resolution_clock::now();
  auto request = std::make_shared<LoadDynDXT>(data);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    LOG_LOADER(error) << *request;
    return false;
  }

  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed =
      std::chrono::duration<double, std::milli>(now - load_start).count();
  LOG_LOADER(info) << "DynDXT upload (" << data.size() << " bytes) took "
                   << elapsed << " milliseconds "
                   << ((double)data.size() * 1000 / elapsed) << " Bps.";

  std::cout << *request << std::endl;

  return true;
}

static uint32_t L2BootstrapAllocate(XBOXInterface& interface,
                                    uint32_t image_size) {
  char args[32];
  snprintf(args, 32, " s=0x%x", image_size);
  auto request = std::make_shared<DynDXTLoader::InvokeSimple>("ldxt!a", args);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    LOG_LOADER(error) << "Failed to allocate " << image_size
                      << " bytes for Loader. " << *request;
    return 0;
  }

  auto response = request->message;
  auto base_param = response.find("base=");
  if (base_param == std::string::npos) {
    LOG_LOADER(error) << "Failed to parse base param from response.";
    return 0;
  }

  uint32_t target = 0;
  if (!MaybeParseHexInt(target, response.substr(base_param + 5))) {
    LOG_LOADER(error) << "Invalid base param in response. " << response;
    return 0;
  }

  return target;
}

static bool L2BootstrapInstall(XBOXInterface& interface, uint32_t entrypoint,
                               const std::vector<uint8_t>& image) {
  auto load_start = std::chrono::high_resolution_clock::now();

  char args[32];
  snprintf(args, 32, " e=0x%x", entrypoint);
  auto request = std::make_shared<DynDXTLoader::InvokeSendKnownSizeBinary>(
      "ldxt!i", image, args);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    LOG_LOADER(error) << "Failed to install DynDXT loader. " << *request;
    return false;
  }

  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed =
      std::chrono::duration<double, std::milli>(now - load_start).count();
  LOG_LOADER(info) << "Loader installed at 0x" << std::hex << std::setfill('0')
                   << std::setw(8) << entrypoint << std::dec << " "
                   << image.size() << " bytes in " << elapsed
                   << " milliseconds "
                   << ((double)image.size() * 1000 / elapsed) << " Bps.";

  return true;
}

bool Loader::InstallDynamicDXTLoader(XBOXInterface& interface) {
  auto stream = std::make_shared<boost::interprocess::bufferstream>(
      (char*)kDynDXTLoader, sizeof(kDynDXTLoader));
  DXTLibrary lib(std::dynamic_pointer_cast<std::istream>(stream),
                 "BundledDXTLoader");
  if (!lib.Parse()) {
    LOG_LOADER(error) << "Failed to load dynamic dxt loader DLL.";
    return false;
  }

  auto debugger = interface.Debugger();
  auto context = interface.Context();

  for (auto& dll_imports : lib.GetImports()) {
    if (!ResolveImports(interface, dll_imports.first, dll_imports.second)) {
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
    LOG_LOADER(error) << "TLS callback functionality not implemented.";
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
      LOG_LOADER(error) << "Failed to set allocation size.";
      return 0;
    }
  }

  if (!InvokeL1Bootstrap(context,
                         GetExport("xbdm.dll", XBDM_DmAllocatePoolWithTag))) {
    LOG_LOADER(error) << "Failed to allocate memory.";
    return 0;
  }

  // The target address is stored in the last 4 bytes of the L1 bootloader.
  auto target_address = debugger->GetDWORD(io_address);
  if (!target_address.has_value()) {
    LOG_LOADER(error) << "Failed to fetch allocated memory address.";
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
    LOG_LOADER(error) << "Failed to set L1 loader to execute mode.";
    return false;
  }

  return true;
}

static std::list<
    std::map<uint32_t, std::vector<ResolveExportList::ResolveRequest>>>
SplitResolutionTable(
    const std::map<uint32_t, std::vector<ResolveExportList::ResolveRequest>>&
        resolution_table) {
#define COMMAND_LEN sizeof("ldxt!r")
  // " b=0x00000000"
  // " o=0x00000000"
#define ADDR_LEN 13
  auto ret = std::list<
      std::map<uint32_t, std::vector<ResolveExportList::ResolveRequest>>>();

  uint32_t remaining_length = 0;
  auto new_table = [&ret, &remaining_length]() -> auto {
    remaining_length = MAXIMUM_SEND_LENGTH - COMMAND_LEN;
    ret.emplace_back();
    return &ret.back();
  };
  auto* current = new_table();

  auto new_entry = [&current](uint32_t image_base) -> auto {
    (*current)[image_base] = std::vector<ResolveExportList::ResolveRequest>();
    return &(*current)[image_base];
  };

  for (auto& table_it : resolution_table) {
    remaining_length -= ADDR_LEN;
    auto* values = new_entry(table_it.first);

    for (auto& req_it : table_it.second) {
      remaining_length -= ADDR_LEN;
      values->emplace_back(req_it);

      // Make sure there's enough room to handle a new base address, splitting
      // the table if not.
      if (remaining_length <= ADDR_LEN) {
        current = new_table();
        values = new_entry(table_it.first);
      }
    }
  }

  return ret;
}

//! Performs bulk export resolution via ResolveExportList.
static bool BulkResolve(
    XBOXInterface& interface, const std::string& module_name,
    std::map<uint32_t, std::vector<ResolveExportList::ResolveRequest>>&
        resolution_table) {
  auto split_requests = SplitResolutionTable(resolution_table);

  for (auto& table : split_requests) {
    auto request = std::make_shared<ResolveExportList>(table);
    interface.SendCommandSync(request);
    if (!request->IsOK()) {
      LOG_LOADER(error) << "Failed to perform bulk import resolution "
                        << *request;
      return false;
    }

    // Verify that each import in the resolution table received a valid address.
    bool ret = true;
    for (const auto& base_to_requests : table) {
      for (const auto& resolve_request : base_to_requests.second) {
        if (!resolve_request.out->real_address) {
          LOG_LOADER(error) << "Failed to resolve import " << module_name << " "
                            << *resolve_request.out;
          ret = false;
        }
      }
    }
    if (!ret) {
      return false;
    }
  }

  return true;
}

bool Loader::ResolveImports(XBOXInterface& interface,
                            const std::string& module_name,
                            std::vector<DXTLibraryImport>& imports) {
  if (!FetchBaseAddress(interface.Debugger(), module_name)) {
    return false;
  }

  uint32_t base_address = module_base_addresses_[module_name];
  auto exports = module_exports_.find(module_name);

  // FetchBaseAddress is expected to populate the export table with an empty
  // vector.
  assert(exports != module_exports_.end());

  auto resolution_table =
      std::map<uint32_t, std::vector<ResolveExportList::ResolveRequest>>();
  auto& export_table = exports->second;
  for (auto& import : imports) {
    uint32_t ordinal = import.ordinal;

    // Resolve name to ordinal.
    if (!import.import_name.empty()) {
      auto name_to_ordinal_table = module_export_names_.find(module_name);
      if (name_to_ordinal_table == module_export_names_.end()) {
        LOG_LOADER(error)
            << "Import from " << module_name << " by name "
            << import.import_name
            << " but no name mapping table exists for that module.";
        return false;
      }

      auto entry = name_to_ordinal_table->second.find(import.import_name);
      if (entry == name_to_ordinal_table->second.end()) {
        LOG_LOADER(error) << "Import from " << module_name
                          << " by unknown name '" << import.import_name << "'.";
      }

      ordinal = entry->second;
    }

    auto existing_entry = export_table.find(ordinal);
    if (existing_entry != export_table.end()) {
      import.real_address = existing_entry->second;
      continue;
    }

    import.real_address = 0;
    auto it = resolution_table.find(base_address);
    if (it == resolution_table.end()) {
      auto new_vector = std::vector<ResolveExportList::ResolveRequest>();
      new_vector.emplace_back(ordinal, &import);
      resolution_table.emplace(base_address, new_vector);
    } else {
      it->second.emplace_back(ordinal, &import);
    }
  }

  if (resolution_table.empty()) {
    return true;
  }

  return BulkResolve(interface, module_name, resolution_table);
}

bool Loader::FetchBaseAddress(const std::shared_ptr<XBDMDebugger>& debugger,
                              const std::string& module_name) {
  if (module_base_addresses_.find(module_name) !=
      module_base_addresses_.end()) {
    return true;
  }

  auto module = debugger->GetModule(module_name);
  if (!module) {
    LOG_LOADER(error) << "Failed to retrieve module info for '" << module_name
                      << "'.";
    return false;
  }

  module_base_addresses_[module_name] = module->base_address;
  module_exports_[module_name] = {};
  return true;
}

uint32_t Loader::GetExport(const std::string& module, uint32_t ordinal) const {
  auto module_export = module_exports_.find(module);
  if (module_export == module_exports_.end()) {
    LOG_LOADER(error) << "Failed to look up export " << module << " @ "
                      << ordinal << " no such module.";
    return 0;
  }

  auto entry = module_export->second.find(ordinal);
  if (entry == module_export->second.end()) {
    LOG_LOADER(error) << "Failed to look up export " << module << " @ "
                      << ordinal << " no such entry.";
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
