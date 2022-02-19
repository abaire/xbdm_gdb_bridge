#include "handler_loader.h"

#include <iostream>

#include "bootstrap_l1.h"
#include "util/logging.h"
#include "xbox/debugger/xbdm_debugger.h"
#include "xbox/xbdm_context.h"
#include "xbox/xbox_interface.h"

static constexpr uint32_t kPEHeaderPointer = 0x3C;
static constexpr uint32_t kExportTableOffset = 0x78;
// https://doxygen.reactos.org/de/d20/struct__IMAGE__EXPORT__DIRECTORY.html
static constexpr uint32_t kExportNumFunctionsOffset = 0x14;
static constexpr uint32_t kExportDirectoryAddressOfFunctionsOffset = 0x1C;

static constexpr uint32_t kDmAllocatePoolWithTagOrdinal = 2;

// DmResumeThread is used because the xbdm handler takes a single DWORD
// parameter and does minimal processing of the input and response.
static constexpr uint32_t kDmResumeThreadOrdinal = 35;

static std::optional<uint32_t> GetExportAddress(
    const std::shared_ptr<XBDMDebugger>& debugger, uint32_t ordinal,
    uint32_t image_base);
static std::shared_ptr<Module> GetXBDMModule(
    const std::shared_ptr<XBDMDebugger>& debugger);
static bool SetEIP(XBDMContext& ctx, const std::shared_ptr<Thread>& thread,
                   uint32_t address);
static bool SetMemoryUnsafe(const std::shared_ptr<XBDMContext>& context,
                            uint32_t address, const std::vector<uint8_t>& data);
static bool InvokeBootstrapL1(const std::shared_ptr<XBDMContext>& context,
                              const uint32_t allocate_pool_with_tag_address,
                              uint32_t& allocated_address);
static bool InstallL2Bootstrap(const std::shared_ptr<XBDMDebugger>& debugger,
                               const std::shared_ptr<XBDMContext>& context,
                               const uint32_t l1_bootloader_address,
                               const uint32_t allocate_pool_with_tag_address);

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

  // Technique inspired by https://github.com/XboxDev/xboxpy
  auto xbdm_module = GetXBDMModule(debugger);
  if (!xbdm_module) {
    LOG(error) << "Failed to retrieve XBDM module info.";
    return false;
  }
  const uint32_t xbdm_base = xbdm_module->base_address;

  auto resume_thread_address =
      GetExportAddress(debugger, kDmResumeThreadOrdinal, xbdm_base);
  if (!resume_thread_address.has_value()) {
    LOG(error) << "Failed to fetch hook address.";
    return false;
  }

  auto allocate_pool_with_tag_address =
      GetExportAddress(debugger, kDmAllocatePoolWithTagOrdinal, xbdm_base);
  if (!allocate_pool_with_tag_address.has_value()) {
    LOG(error) << "Failed to fetch DmAllocatePoolWithTag address.";
    return false;
  }

  auto xbdm = interface.Context();

  std::vector<uint8_t> bootstrap_l1(
      kBootstrapL1,
      kBootstrapL1 + (sizeof(kBootstrapL1) / sizeof(kBootstrapL1[0])));
  auto original_function =
      debugger->GetMemory(resume_thread_address.value(), bootstrap_l1.size());
  if (!original_function.has_value()) {
    LOG(error) << "Failed to fetch target function.";
    return false;
  }

  if (!SetMemoryUnsafe(xbdm, resume_thread_address.value(), bootstrap_l1)) {
    LOG(error) << "Failed to patch target function.";
    return false;
  }

  bool ret = InstallL2Bootstrap(debugger, xbdm, resume_thread_address.value(),
                                allocate_pool_with_tag_address.value());

  if (!SetMemoryUnsafe(xbdm, resume_thread_address.value(),
                       original_function.value())) {
    LOG(error) << "Failed to restore target function.";
    return false;
  }

  return ret;
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

static std::shared_ptr<Module> GetXBDMModule(
    const std::shared_ptr<XBDMDebugger>& debugger) {
  auto modules = debugger->Modules();
  for (auto& module : modules) {
    if (module->name == "xbdm.dll") {
      return module;
    }
  }
  return nullptr;
}

static bool SetEIP(XBDMContext& ctx, const std::shared_ptr<Thread>& thread,
                   uint32_t address) {
  thread->context->eip = address;

  {
    auto request = std::make_shared<::SetContext>(thread->thread_id,
                                                  thread->context.value());
    ctx.SendCommandSync(request);
    if (!request->IsOK()) {
      LOG(error) << "Failed to update thread EIP.";
      return false;
    }
  }

  return true;
}

static bool SetMemoryUnsafe(const std::shared_ptr<XBDMContext>& context,
                            uint32_t address,
                            const std::vector<uint8_t>& data) {
  auto request = std::make_shared<SetMem>(address, data);
  context->SendCommandSync(request);
  return request->IsOK();
}

// Invoke the L1 bootstrap to allocate a new memory block into which the L2
// bootstrap can be loaded. Note that this assumes the `resume` command has
// already been patched with the L1 bootstrap.
static bool InvokeBootstrapL1(const std::shared_ptr<XBDMContext>& context,
                              const uint32_t allocate_pool_with_tag_address) {
  auto request = std::make_shared<::Resume>(allocate_pool_with_tag_address);
  context->SendCommandSync(request);
  return request->IsOK();
}

static bool InstallL2Bootstrap(const std::shared_ptr<XBDMDebugger>& debugger,
                               const std::shared_ptr<XBDMContext>& context,
                               const uint32_t l1_bootloader_address,
                               const uint32_t allocate_pool_with_tag_address) {
  LOG(warning) << "L1 bootstrap @" << std::hex << l1_bootloader_address;
  LOG(warning) << "Installing L2 bootstrap. DmAllocatePoolWithTag @" << std::hex
               << allocate_pool_with_tag_address;
  // b002ae95 - DmAllocatePool
  // b002a962 - DmAllocatePoolWithTag
  if (!InvokeBootstrapL1(context, allocate_pool_with_tag_address)) {
    LOG(error) << "Failed to allocate memory for L2 bootstrap.";
    return false;
  }

  // The target address is stored in the last 4 bytes of the L1 bootloader.
  const uint32_t result_address =
      l1_bootloader_address + sizeof(kBootstrapL1) - 4;
  auto target_address = debugger->GetDWORD(result_address);
  if (!target_address.has_value()) {
    LOG(error) << "Failed to fetch L2 bootstrap target address.";
    return false;
  }

  uint32_t l2_bootstrap_addr = target_address.value();
  LOG(info) << "Bootstrap address " << std::hex << l2_bootstrap_addr;

  return false;
}