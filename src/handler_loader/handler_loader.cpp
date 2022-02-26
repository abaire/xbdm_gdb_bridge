#include "handler_loader.h"

#include <algorithm>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <iostream>

#include "bootstrap_l1.h"
#include "dll_linker.h"
#include "dxt_library.h"
#include "dyndxt_loader.h"
#include "handler_requests.h"
#include "util/logging.h"
#include "winapi/winnt.h"
#include "xbdm_exports.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"
#include "xbox/xbox_interface.h"
#include "xboxkrnl_exports.h"

static constexpr uint32_t kDmAllocatePoolWithTagOrdinal = 2;
static constexpr uint32_t kDmFreePoolOrdinal = 9;
static constexpr uint32_t kDmRegisterCommandProcessorOrdinal = 30;

// DmResumeThread is used because the xbdm handler takes a single DWORD
// parameter and does minimal processing of the input and response.
static constexpr uint32_t kDmResumeThreadOrdinal = 35;

static bool SetMemoryUnsafe(const std::shared_ptr<XBDMContext>& context,
                            uint32_t address, const std::vector<uint8_t>& data);
static bool InvokeBootstrap(const std::shared_ptr<XBDMContext>& context,
                            uint32_t parameter);

static bool FetchExport(const std::shared_ptr<XBDMDebugger>& debugger,
                        uint32_t ordinal,
                        std::map<uint32_t, uint32_t>& ordinal_to_address,
                        uint32_t image_base);
static bool FetchExport(const std::shared_ptr<XBDMDebugger>& debugger,
                        uint32_t ordinal,
                        std::map<uint32_t, uint32_t>& ordinal_to_address,
                        uint32_t image_base, uint32_t& resolved_address);

HandlerLoader* HandlerLoader::singleton_ = nullptr;

bool HandlerLoader::Bootstrap(XBOXInterface& interface) {
  if (!singleton_) {
    singleton_ = new HandlerLoader();
  }

  // See if the Dynamic DXT loader is already running.
  auto request = std::make_shared<HandlerInvokeSimple>("ddxt!hello");
  interface.SendCommandSync(request);
  if (request->IsOK()) {
    return true;
  }

  bool ret = singleton_->InjectLoader(interface);
  if (!ret) {
    delete singleton_;
    singleton_ = nullptr;
  }
  return ret;
}

bool HandlerLoader::Load(XBOXInterface& interface, const std::string& path) {
  if (!singleton_ && !Bootstrap(interface)) {
    LOG(error) << "Failed to bootstrap handler loader.";
    return false;
  }
  return singleton_->LoadDLL(interface, path);
}

bool HandlerLoader::InjectLoader(XBOXInterface& interface) {
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

  uint32_t loader_entrypoint = InstallDynamicDXTLoader(debugger, xbdm);
  if (!loader_entrypoint) {
    ret = false;
    goto cleanup;
  }

  LOG(info) << "Invoking Dynamic DXT DxtMain at 0x" << std::hex
            << loader_entrypoint;

  if (!InvokeBootstrap(xbdm, loader_entrypoint)) {
    LOG(error) << "Failed to initialize Dynamic DXT loader.";
    ret = false;
    goto cleanup;
  }

  if (!FillLoaderExportRegistry(debugger, xbdm)) {
    LOG(warning) << "Failed to populate Dynamic DXT loader export registry.";
  }

cleanup:
  if (!SetMemoryUnsafe(xbdm, kDmResumeThread, original_function.value())) {
    LOG(error) << "Failed to restore target function.";
    return false;
  }

  return ret;
}

bool HandlerLoader::LoadDLL(XBOXInterface& interface, const std::string& path) {
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

  auto request = std::make_shared<HandlerDDXTLoad>(data);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    LOG(error) << *request;
    return false;
  }

  std::cout << *request << std::endl;

  return true;
}

uint32_t HandlerLoader::InstallDynamicDXTLoader(
    const std::shared_ptr<XBDMDebugger>& debugger,
    const std::shared_ptr<XBDMContext>& context) {
  auto stream = std::make_shared<boost::interprocess::bufferstream>(
      (char*)kDynDXTLoader, sizeof(kDynDXTLoader));
  DXTLibrary lib(std::dynamic_pointer_cast<std::istream>(stream),
                 "BundledDXTLoader");
  if (!lib.Parse()) {
    LOG(error) << "Failed to load dynamic dxt loader.";
    return 0;
  }

  for (auto& dll_imports : lib.GetImports()) {
    if (!ResolveImports(debugger, dll_imports.first, dll_imports.second)) {
      return 0;
    }
  }

  uint32_t image_size = lib.GetImageSize();
  uint32_t target = AllocatePool(debugger, context, image_size);
  if (!target) {
    LOG(error) << "Failed to allocate memory for dxt loader.";
    return 0;
  }

  if (!lib.Relocate(target)) {
    // TODO: Free pool.
    return 0;
  }

  if (!SetMemoryUnsafe(context, target, lib.GetImage())) {
    LOG(error) << "Failed to upload dxt loader.";
    return 0;
  }

  for (auto callback : lib.GetTLSInitializers()) {
    LOG(error) << "TLS callback functionality not implemented.";
    // TODO: Free pool.
    return 0;
  }

  if (!SetL1LoaderExecuteMode(context)) {
    // TODO: Free pool.
    return 0;
  }

  return lib.GetEntrypoint();
}

uint32_t HandlerLoader::AllocatePool(
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

  if (!InvokeBootstrap(context,
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

bool HandlerLoader::SetL1LoaderExecuteMode(
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

bool HandlerLoader::ResolveImports(
    const std::shared_ptr<XBDMDebugger>& debugger,
    const std::string& module_name, std::vector<DXTLibraryImport>& imports) {
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

bool HandlerLoader::FetchBaseAddress(
    const std::shared_ptr<XBDMDebugger>& debugger,
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

uint32_t HandlerLoader::GetExport(const std::string& module,
                                  uint32_t ordinal) const {
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

bool HandlerLoader::FillLoaderExportRegistry(
    const std::shared_ptr<XBDMDebugger>& debugger,
    const std::shared_ptr<XBDMContext>& context) {
#define RESOLVE(ordinal, table, base)                   \
  if (!FetchExport(debugger, ordinal, table, base)) {   \
    LOG(error) << "Failed to resolve export " #ordinal; \
    return false;                                       \
  }

  for (auto& name_and_base : module_base_addresses_) {
    const std::string& module_name = name_and_base.first;
    const uint32_t base = name_and_base.second;

    const auto& named_exports = module_export_names_.find(module_name);
    const auto& ordinal_exports = module_exports_.find(module_name);
    if (ordinal_exports == module_exports_.end()) {
      continue;
    }

    auto& export_table = ordinal_exports->second;

    if (named_exports != module_export_names_.end()) {
      for (auto& export_name_and_ordinal : named_exports->second) {
        const std::string& export_name = export_name_and_ordinal.first;
        uint32_t ordinal = export_name_and_ordinal.second;

        RESOLVE(ordinal, export_table, base);
        const uint32_t address = GetExport(module_name, ordinal);

        {
          auto request = std::make_shared<HandlerDDXTExport>(
              module_name, ordinal, address, export_name);
          context->SendCommandSync(request);
          if (!request->IsOK()) {
            LOG(error) << "Failed to populate export entry " << *request;
            return false;
          }
        }
      }
    }

    // TODO: Add support for unnamed exports if necessary.
  }
#undef RESOLVE

  return true;
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

static bool InvokeBootstrap(const std::shared_ptr<XBDMContext>& context,
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
