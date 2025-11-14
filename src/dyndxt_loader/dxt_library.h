#ifndef XBDM_GDB_BRIDGE_DXT_LIBRARY_H
#define XBDM_GDB_BRIDGE_DXT_LIBRARY_H

#include <winapi/winnt.h>

#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Captures information about a DLL import.
struct DXTLibraryImport {
  uint32_t ordinal{0};
  // The name of the import. If this is non-empty, it must be used instead of
  // `ordinal`.
  std::string import_name;

  uint32_t function_address{0};
  uint32_t real_address{0};
};

extern std::ostream& operator<<(std::ostream& os, DXTLibraryImport const& i);

class DXTLibrary {
 public:
  DXTLibrary(std::shared_ptr<std::istream> is, std::string path);
  explicit DXTLibrary(std::string path);

  bool Parse();

  uint32_t GetImageSize() const { return image_.size(); }
  const std::vector<uint8_t>& GetImage() const { return image_; }

  std::map<std::string, std::vector<DXTLibraryImport>>& GetImports() {
    return imports_;
  }

  // Prior to calling Relocate, all import entries must be resolved by setting
  // the `real_address` field.
  bool Relocate(uint32_t image_base);

  // Return the address of the DXT entrypoint.
  // WARNING: This is only correct after Relocate().
  [[nodiscard]] uint32_t GetEntrypoint() const;

  // Return the fixed up addresses of any thread local storage initializers.
  // WARNING: This is only correct after Relocate().
  [[nodiscard]] std::vector<uint32_t> GetTLSInitializers() const;

  // Return the image base from the DLL header.
  [[nodiscard]] uint32_t GetImageBase() const;

 private:
  bool ParseDLLHeader();
  bool ExtractImportTable();
  bool ProcessSection(const IMAGE_SECTION_HEADER& header);

  bool PatchImports();

 private:
  const std::string path_;
  std::shared_ptr<std::istream> infile_;

  IMAGE_NT_HEADERS32 header_{0};
  std::vector<IMAGE_SECTION_HEADER> section_headers_;
  std::map<std::string, std::vector<DXTLibraryImport>> imports_;

  std::vector<uint8_t> image_;
};

#endif  // XBDM_GDB_BRIDGE_DXT_LIBRARY_H
