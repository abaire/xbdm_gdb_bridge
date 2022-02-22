#include "handler_loader.h"

#include <boost/endian/conversion.hpp>
#include <iostream>

#include "bootstrap_l1.h"
#include "bootstrap_l2_thunk.h"
#include "coff_loader.h"
#include "dxt_library.h"
#include "util/logging.h"
#include "winapi/winnt.h"
#include "xbdm_exports.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"
#include "xbox/xbox_interface.h"
#include "xboxkrnl_exports.h"

// The entrypoint for the loaded program.
static constexpr const char kEntrypointName[] = "_DxtMain";

static constexpr uint32_t kPEHeaderPointer = 0x3C;
static constexpr uint32_t kExportTableOffset = 0x78;
// https://doxygen.reactos.org/de/d20/struct__IMAGE__EXPORT__DIRECTORY.html
static constexpr uint32_t kExportNumFunctionsOffset = 0x14;
static constexpr uint32_t kExportDirectoryAddressOfFunctionsOffset = 0x1C;

static constexpr uint32_t kDmAllocatePoolWithTagOrdinal = 2;
static constexpr uint32_t kDmFreePoolOrdinal = 9;
static constexpr uint32_t kDmRegisterCommandProcessorOrdinal = 30;

// DmResumeThread is used because the xbdm handler takes a single DWORD
// parameter and does minimal processing of the input and response.
static constexpr uint32_t kDmResumeThreadOrdinal = 35;

static std::optional<uint32_t> GetExportAddress(
    const std::shared_ptr<XBDMDebugger>& debugger, uint32_t ordinal,
    uint32_t image_base);
static std::shared_ptr<Module> GetModule(
    const std::shared_ptr<XBDMDebugger>& debugger,
    const std::string& module_name);
static bool SetMemoryUnsafe(const std::shared_ptr<XBDMContext>& context,
                            uint32_t address, const std::vector<uint8_t>& data);
static bool InvokeBootstrap(const std::shared_ptr<XBDMContext>& context,
                            uint32_t parameter);

static bool FetchExport(const std::shared_ptr<XBDMDebugger>& debugger,
                        uint32_t ordinal,
                        std::map<uint32_t, uint32_t>& ordinal_to_address,
                        uint32_t image_base);

HandlerLoader* HandlerLoader::singleton_ = nullptr;

bool HandlerLoader::Bootstrap(XBOXInterface& interface) {
  if (!singleton_) {
    singleton_ = new HandlerLoader();
  }
  return singleton_->InjectLoader(interface);
}

bool HandlerLoader::InjectLoader(XBOXInterface& interface) {
  auto debugger = interface.Debugger();
  if (!debugger) {
    LOG(error) << "Debugger not attached.";
    return false;
  }

  {
    auto module = GetModule(debugger, "xbdm.dll");
    if (!module) {
      LOG(error) << "Failed to retrieve XBDM module info.";
      return false;
    }
    xbdm_base_addr_ = module->base_address;

#define RESOLVE(ordinal, table, base)                   \
  if (!FetchExport(debugger, ordinal, table, base)) {   \
    LOG(error) << "Failed to resolve export " #ordinal; \
    return false;                                       \
  }

    RESOLVE(XBDM_DmResumeThread, xbdm_exports_, xbdm_base_addr_)
    RESOLVE(XBDM_DmAllocatePoolWithTag, xbdm_exports_, xbdm_base_addr_)

#undef RESOLVE
  }

  {
    auto module = GetModule(debugger, "xboxkrnl.exe");
    if (!module) {
      LOG(error) << "Failed to retrieve xboxkrnl module info.";
      return false;
    }
    xboxkrnl_base_addr_ = module->base_address;
  }

  auto xbdm = interface.Context();

  std::vector<uint8_t> bootstrap_l1(
      kBootstrapL1,
      kBootstrapL1 + (sizeof(kBootstrapL1) / sizeof(kBootstrapL1[0])));

  std::vector<uint8_t> bootstrap_l2_thunk(
      kBootstrapL2Thunk, kBootstrapL2Thunk + (sizeof(kBootstrapL2Thunk) /
                                              sizeof(kBootstrapL2Thunk[0])));

  uint32_t max_overwrite =
      std::max(bootstrap_l1.size(), bootstrap_l2_thunk.size());

  const uint32_t kDmResumeThread = xbdm_exports_[XBDM_DmResumeThread];
  auto original_function = debugger->GetMemory(kDmResumeThread, max_overwrite);
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

  if (!SetMemoryUnsafe(xbdm, kDmResumeThread, bootstrap_l2_thunk)) {
    LOG(error) << "Failed to patch target function with DynDXT thunk.";
    ret = false;
    goto cleanup;
  }

  LOG(info) << "Invoking dyndxt lib at " << std::hex << loader_entrypoint;

  if (!InvokeBootstrap(xbdm, loader_entrypoint)) {
    LOG(error) << "Failed to initialize Dynamic DXT loader.";
    ret = false;
    goto cleanup;
  }

cleanup:
  if (!SetMemoryUnsafe(xbdm, kDmResumeThread, original_function.value())) {
    LOG(error) << "Failed to restore target function.";
    return false;
  }

  return ret;
}

uint32_t HandlerLoader::InstallDynamicDXTLoader(
    const std::shared_ptr<XBDMDebugger>& debugger,
    const std::shared_ptr<XBDMContext>& context) {
  // DONOTSUBMIT
  DXTLibrary lib(
      "/mnt/linux_data/development/xbox/nxdk_dyndxt/cmake-build-release/"
      "dynamic_dxt_loader.ar");
  //  DXTLibrary lib(
  //      "/Users/abaire/development/xbox/nxdk_dyndxt/cmake-build-release/"
  //      "dynamic_dxt_loader.ar");
  if (!lib.Parse()) {
    LOG(error) << "Failed to load dynamic dxt loader.";
    return 0;
  }

  auto loader = lib.GetLoader();
  for (auto& section : loader->sections) {
    if (!section.ShouldLoad()) {
      continue;
    }

    // TODO: Track allocations and free them on error.
    uint32_t target = AllocatePool(debugger, context, section.VirtualSize());
    if (!target) {
      LOG(error) << "Failed to allocate memory for section " << section.Name()
                 << ".";
      return 0;
    }

    section.SetVirtualAddress(target);
  }

  std::map<std::string, uint32_t> externs;
  loader->ResolveSymbolTable(externs);

  if (!externs.empty() && !ResolveExterns(debugger, loader, externs)) {
    LOG(error) << "Failed to resolve imports.";
    return 0;
  }

  if (!loader->Relocate()) {
    LOG(error) << "Relocation failed.";
    return 0;
  }

  for (auto& section : loader->sections) {
    if (!section.ShouldLoad()) {
      continue;
    }

    LOG(info) << "Loading " << section.Name() << " at " << std::hex
              << section.virtual_address;
    if (!SetMemoryUnsafe(context, section.virtual_address, section.body)) {
      LOG(error) << "Failed to load section " << section.Name() << " at "
                 << std::hex << section.virtual_address;
      return 0;
    }
  }

  uint32_t entrypoint = 0;
  uint32_t index = 0;
  for (const auto& symbol : loader->symbol_table) {
    if (symbol.name == kEntrypointName) {
      entrypoint = loader->resolved_symbol_table[index];
      break;
    }
    ++index;
  }

  return entrypoint;
}

uint32_t HandlerLoader::AllocatePool(
    const std::shared_ptr<XBDMDebugger>& debugger,
    const std::shared_ptr<XBDMContext>& context, uint32_t size) const {
  // The requested size and target address is stored in the last 4 bytes of the
  // L1 bootloader.
  const uint32_t io_address = xbdm_exports_.find(XBDM_DmResumeThread)->second +
                              sizeof(kBootstrapL1) - 4;

  {
    auto data = reinterpret_cast<uint8_t*>(&size);
    std::vector<uint8_t> size_request(data, data + 4);
    if (!SetMemoryUnsafe(context, io_address, size_request)) {
      LOG(error) << "Failed to set allocation size.";
      return 0;
    }
  }

  if (!InvokeBootstrap(
          context, xbdm_exports_.find(XBDM_DmAllocatePoolWithTag)->second)) {
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

bool HandlerLoader::ResolveExterns(
    const std::shared_ptr<XBDMDebugger>& debugger,
    const std::shared_ptr<COFFLoader>& loader,
    std::map<std::string, uint32_t>& externs) {
  for (const auto& entry : externs) {
    LOG(info) << "Resolving extern " << entry.first;

    auto lookup = [&debugger, &loader, &entry](
                      const std::map<std::string, uint32_t>& name_to_ordinal,
                      std::map<uint32_t, uint32_t>& ordinal_to_address,
                      const uint32_t base_address) {
      const auto& ordinal_pair = name_to_ordinal.find(entry.first);
      if (ordinal_pair == name_to_ordinal.end()) {
        return false;
      }
      auto ordinal = ordinal_pair->second;
      auto existing_resolution = ordinal_to_address.find(ordinal);
      if (existing_resolution != ordinal_to_address.end()) {
        loader->resolved_symbol_table[entry.second] =
            existing_resolution->second;
        return true;
      }

      if (!FetchExport(debugger, ordinal, ordinal_to_address, base_address)) {
        LOG(error) << "Failed to resolve " << entry.first;
        return false;
      }

      loader->resolved_symbol_table[entry.second] = ordinal_to_address[ordinal];
      return true;
    };

    if (lookup(XBDM_Exports, xbdm_exports_, xbdm_base_addr_)) {
      continue;
    }
    if (lookup(XBOXKRNL_Exports, xboxkrnl_exports_, xboxkrnl_base_addr_)) {
      continue;
    }

    LOG(error) << "Failed to resolve import for " << entry.first;
    return false;
  }
  return true;
}

static std::optional<uint32_t> GetExportAddress(
    const std::shared_ptr<XBDMDebugger>& debugger, uint32_t ordinal,
    uint32_t image_base) {
  assert(ordinal > 0);
  auto pe_header = debugger->GetDWORD(image_base + kPEHeaderPointer);
  if (!pe_header.has_value()) {
    LOG(error) << "Failed to load PE header offset.";
    return std::nullopt;
  }

  auto export_table =
      debugger->GetDWORD(image_base + pe_header.value() + kExportTableOffset);
  if (!export_table.has_value()) {
    LOG(error) << "Failed to load export table offset.";
    return std::nullopt;
  }

  uint32_t export_table_base = image_base + export_table.value();

  auto export_count =
      debugger->GetDWORD(export_table_base + kExportNumFunctionsOffset);
  if (!export_count.has_value()) {
    LOG(error) << "Failed to load export table count.";
    return std::nullopt;
  }

  uint32_t num_exports = export_count.value();
  uint32_t index = ordinal - 1;
  if (index >= num_exports) {
    LOG(error) << "Invalid export ordinal " << ordinal
               << " larger than table size " << export_count.value();
    return std::nullopt;
  }

  auto export_address_offset = debugger->GetDWORD(
      export_table_base + kExportDirectoryAddressOfFunctionsOffset);
  if (!export_address_offset.has_value()) {
    LOG(error) << "Failed to load export table address table.";
    return std::nullopt;
  }

  auto function_address = debugger->GetDWORD(
      image_base + export_address_offset.value() + index * 4);
  if (!function_address.has_value()) {
    LOG(error) << "Failed to retrieve function address.";
    return std::nullopt;
  }
  return image_base + function_address.value();
}

static std::shared_ptr<Module> GetModule(
    const std::shared_ptr<XBDMDebugger>& debugger,
    const std::string& module_name) {
  auto modules = debugger->Modules();
  for (auto& module : modules) {
    if (module->name == module_name) {
      return module;
    }
  }
  return nullptr;
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
                        const uint32_t image_base) {
  auto address = GetExportAddress(debugger, ordinal, image_base);
  if (!address.has_value()) {
    return false;
  }
  ordinal_to_address[ordinal] = address.value();
  return true;
}
