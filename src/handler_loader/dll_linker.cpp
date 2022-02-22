#include "dll_linker.h"

#include "util/logging.h"
#include "xbox/debugger/xbdm_debugger.h"

static constexpr uint32_t kPEHeaderPointer = 0x3C;
static constexpr uint32_t kExportTableOffset = 0x78;
// https://doxygen.reactos.org/de/d20/struct__IMAGE__EXPORT__DIRECTORY.html
static constexpr uint32_t kExportNumFunctionsOffset = 0x14;
static constexpr uint32_t kExportDirectoryAddressOfFunctionsOffset = 0x1C;

std::optional<uint32_t> GetExportAddress(
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
