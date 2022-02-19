#include "handler_loader.h"

#include <boost/endian/conversion.hpp>
#include <iostream>

#include "bootstrap_l1.h"
#include "bootstrap_l2.h"
#include "bootstrap_l2_imports.h"
#include "bootstrap_l2_thunk.h"
#include "util/logging.h"
#include "xbdm_exports.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"
#include "xbox/xbox_interface.h"
#include "xboxkrnl_exports.h"

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
    ResolveXBDMExports(debugger, module->base_address);
  }
  {
    auto module = GetModule(debugger, "xboxkrnl.exe");
    if (!module) {
      LOG(error) << "Failed to retrieve xboxkrnl module info.";
      return false;
    }
    ResolveKernelExports(debugger, module->base_address);
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

  auto original_function = debugger->GetMemory(resume_thread_, max_overwrite);
  if (!original_function.has_value()) {
    LOG(error) << "Failed to fetch target function.";
    return false;
  }

  if (!SetMemoryUnsafe(xbdm, resume_thread_, bootstrap_l1)) {
    LOG(error) << "Failed to patch target function with l1 bootstrap.";
    return false;
  }

  bool ret = InstallL2Bootstrap(debugger, xbdm);
  if (!ret) {
    goto cleanup;
  }

  if (!SetMemoryUnsafe(xbdm, resume_thread_, bootstrap_l2_thunk)) {
    LOG(error) << "Failed to patch target function with l2 thunk.";
    ret = false;
    goto cleanup;
  }

  if (!InvokeBootstrap(xbdm, l2_bootstrap_addr_)) {
    LOG(error) << "Failed to initialize L2 bootstrap.";
    ret = false;
    goto cleanup;
  }

cleanup:
  if (!SetMemoryUnsafe(xbdm, resume_thread_, original_function.value())) {
    LOG(error) << "Failed to restore target function.";
    return false;
  }

  return ret;
}

#define RESOLVE(export, variable)                                    \
  {                                                                  \
    auto address = GetExportAddress(debugger, (export), image_base); \
    if (!address.has_value()) {                                      \
      LOG(error) << "Failed to fetch " #export;                      \
      return false;                                                  \
    }                                                                \
    (variable) = address.value();                                    \
  }

bool HandlerLoader::ResolveXBDMExports(
    const std::shared_ptr<XBDMDebugger>& debugger, uint32_t image_base) {
  RESOLVE(XBDM_DmResumeThread, resume_thread_)
  RESOLVE(XBDM_DmAllocatePoolWithTag, allocate_pool_with_tag_)
  RESOLVE(XBDM_DmFreePool, free_pool_)
  RESOLVE(XBDM_DmRegisterCommandProcessor, register_command_processor_)

  return true;
}

bool HandlerLoader::ResolveKernelExports(
    const std::shared_ptr<XBDMDebugger>& debugger, uint32_t image_base) {
  RESOLVE(XBOX_XeLoadSection, xe_load_section_)
  RESOLVE(XBOX_XeUnloadSection, xe_unload_section_)
  RESOLVE(XBOX_MmDbgAllocateMemory, mm_dbg_allocate_memory_)
  RESOLVE(XBOX_MmDbgFreeMemory, mm_dbg_free_memory_)

  return true;
}

#undef RESOLVE

bool HandlerLoader::InstallL2Bootstrap(
    const std::shared_ptr<XBDMDebugger>& debugger,
    const std::shared_ptr<XBDMContext>& context) {
  // Invoke the L1 bootstrap to allocate a new memory block into which the L2
  // bootstrap can be loaded. Note that this assumes the `resume` command has
  // already been patched with the L1 bootstrap.
  if (!InvokeBootstrap(context, allocate_pool_with_tag_)) {
    LOG(error) << "Failed to allocate memory for L2 bootstrap.";
    return false;
  }

  // The target address is stored in the last 4 bytes of the L1 bootloader.
  const uint32_t result_address = resume_thread_ + sizeof(kBootstrapL1) - 4;
  auto target_address = debugger->GetDWORD(result_address);
  if (!target_address.has_value()) {
    LOG(error) << "Failed to fetch L2 bootstrap target address.";
    return false;
  }

  std::vector<uint8_t> bootstrap_l2(
      kBootstrapL2,
      kBootstrapL2 + (sizeof(kBootstrapL2) / sizeof(kBootstrapL2[0])));

  l2_bootstrap_addr_ = target_address.value();

  if (!SetMemoryUnsafe(context, l2_bootstrap_addr_, bootstrap_l2)) {
    LOG(error) << "Failed to load l2 bootstrap at " << std::hex
               << l2_bootstrap_addr_;
    return false;
  }

  LOG(info) << "L2 loader installed at " << std::hex << l2_bootstrap_addr_;

  // Populate the bootstrap import table.
  uint32_t import_table[BOOTSTRAP_L2_IMPORT_TABLE_SIZE] = {0};
#define IMPORT(offset, value) \
  import_table[offset] = boost::endian::native_to_little(value)
  IMPORT(DmAllocatePoolWithTagOffset, allocate_pool_with_tag_);
  IMPORT(DmFreePoolOffset, free_pool_);
  IMPORT(DmRegisterCommandProcessorOffset, register_command_processor_);

  IMPORT(XeLoadSectionOffset, xe_load_section_);
  IMPORT(XeUnloadSectionOffset, xe_unload_section_);
  IMPORT(MmDbgAllocateMemoryOffset, mm_dbg_allocate_memory_);
  IMPORT(MmDbgFreeMemoryOffset, mm_dbg_free_memory_);

#undef IMPORT

  auto data = reinterpret_cast<uint8_t*>(import_table);
  std::vector<uint8_t> import_data(data,
                                   data + BOOTSTRAP_L2_IMPORT_TABLE_SIZE * 4);

  const uint32_t import_address =
      l2_bootstrap_addr_ + BOOTSTRAP_L2_IMPORT_TABLE_START;
  if (!SetMemoryUnsafe(context, import_address, import_data)) {
    LOG(error) << "Failed to populate l2 bootstrap imports at " << std::hex
               << import_address;
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
