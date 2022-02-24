#include "dxt_library.h"

#include <boost/algorithm/string/predicate.hpp>
#include <utility>

#include "util/logging.h"

typedef struct IMAGE_BASE_RELOCATION {
  DWORD VirtualAddress;
  DWORD SizeOfBlock;
} IMAGE_BASE_RELOCATION;

typedef struct IMAGE_IMPORT_BY_NAME {
  USHORT Hint;
  UCHAR Name[1];
} IMAGE_IMPORT_BY_NAME;

// From https://docs.microsoft.com/en-us/windows/win32/debug/pe-format
#define IMAGE_REL_BASED_ABSOLUTE 0
#define IMAGE_REL_BASED_HIGHLOW 3

#define IMAGE_SNAP_BY_ORDINAL(ordinal) (((ordinal)&0x80000000) != 0)

DXTLibrary::DXTLibrary(std::shared_ptr<std::istream> is, std::string path)
    : path_(std::move(path)) {
  infile_ = std::move(is);
}

DXTLibrary::DXTLibrary(std::string path) : path_(std::move(path)) {
  auto infile = std::make_shared<std::ifstream>();
  infile->open(path_, std::ios::binary);
  if (!infile->is_open()) {
    LOG(error) << "Failed to load DXT file from '" << path_ << "'";
  }
  infile_ = infile;
}

bool DXTLibrary::Parse() {
  if (!infile_->good()) {
  }
  if (!ParseDLLHeader()) {
    return false;
  }

  image_.reserve(header_.OptionalHeader.SizeOfImage);
  image_.resize(header_.OptionalHeader.SizeOfImage);

  infile_->seekg(0);
  if (!infile_->good()) {
    LOG(error) << "Failed to seek to start in '" << path_ << "'";
    return false;
  }
  infile_->read(reinterpret_cast<char *>(image_.data()),
                header_.OptionalHeader.SizeOfHeaders);

  for (auto &section_header : section_headers_) {
    if (!ProcessSection(section_header)) {
      image_.clear();
      return false;
    }
  }

  if (!ExtractImportTable()) {
    return false;
  }

  return true;
}

bool DXTLibrary::Relocate(uint32_t image_base) {
  if (image_base == header_.OptionalHeader.ImageBase) {
    return true;
  }

  int32_t image_delta = 0;
  if (image_base > header_.OptionalHeader.ImageBase) {
    image_delta =
        static_cast<int32_t>(image_base - header_.OptionalHeader.ImageBase);
  } else {
    image_delta = -1 * static_cast<int32_t>(header_.OptionalHeader.ImageBase -
                                            image_base);
  }

  auto &directory =
      header_.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
  if (!directory.Size) {
    LOG(error) << "No relocation data in '" << path_ << "'";
    return false;
  }

  auto relocation_start = image_.data() + directory.VirtualAddress;
  auto block =
      reinterpret_cast<const IMAGE_BASE_RELOCATION *>(relocation_start);

  // TODO: Prevent reads beyond end of table.
  while (block->VirtualAddress > 0) {
    auto dest = image_.data() + block->VirtualAddress;
    auto entry = reinterpret_cast<uint16_t *>(relocation_start +
                                              sizeof(relocation_start));
    auto relocation_end = relocation_start + block->SizeOfBlock;
    while (reinterpret_cast<uint8_t *>(entry) < relocation_end) {
      uint32_t type = *entry >> 12;
      uint32_t rva_offset = *entry & 0x0FFF;
      ++entry;

      switch (type) {
        case IMAGE_REL_BASED_ABSOLUTE:
          // Skip padding entries.
          continue;

        case IMAGE_REL_BASED_HIGHLOW: {
          auto target = reinterpret_cast<uint32_t *>(dest + rva_offset);
          *target += image_delta;
          break;
        }

        default:
          LOG(error) << "Unhandled relocation type " << type << " at "
                     << rva_offset << " in block with address 0x" << std::hex
                     << block->VirtualAddress << " in '" << path_ << "'";
          return false;
      }
    }

    relocation_start = relocation_end;
    block = reinterpret_cast<const IMAGE_BASE_RELOCATION *>(relocation_start);
  }

  header_.OptionalHeader.ImageBase = image_base;
  return PatchImports();
}

uint32_t DXTLibrary::GetEntrypoint() const {
  return header_.OptionalHeader.ImageBase +
         header_.OptionalHeader.AddressOfEntryPoint;
}

std::vector<uint32_t> DXTLibrary::GetTLSInitializers() const {
  std::vector<uint32_t> ret;
  auto &directory =
      header_.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
  if (!directory.Size) {
    return ret;
  }

  // TODO: Verify this behavior and rebase.
  auto init = reinterpret_cast<const IMAGE_TLS_DIRECTORY_32 *>(
      image_.data() + directory.VirtualAddress);
  auto callback = reinterpret_cast<uint32_t *>(init->AddressOfCallBacks);
  while (callback && *callback) {
    ret.push_back(*callback++);
  }
  if (!ret.empty()) {
    assert(!"TODO: Implement me.");
  }
  return ret;
}

bool DXTLibrary::PatchImports() {
  for (auto &dll_imports : imports_) {
    for (auto &import : dll_imports.second) {
      if (!import.real_address) {
        LOG(error) << "Unresolved import " << dll_imports.first
                   << "::" << import;
        return false;
      }

      auto function =
          reinterpret_cast<uint32_t *>(image_.data() + import.function_address);
      *function = import.real_address;
    }
  }

  return true;
}

bool DXTLibrary::ParseDLLHeader() {
  IMAGE_DOS_HEADER dos_header;
  infile_->read(reinterpret_cast<char *>(&dos_header), sizeof(dos_header));
  if (!infile_->good()) {
    LOG(error) << "Failed to load DOS header from '" << path_ << "'";
    return false;
  }

  if (dos_header.e_magic != IMAGE_DOS_SIGNATURE) {
    LOG(error) << "Bad DOS signature for DXT library '" << path_ << "'";
    return false;
  }

  if (dos_header.e_lfanew > sizeof(dos_header)) {
    infile_->ignore(
        static_cast<uint32_t>(dos_header.e_lfanew - sizeof(dos_header)));
    if (!infile_->good()) {
      LOG(error) << "Failed to skip to PE header in '" << path_ << "'";
      return false;
    }
  }

  infile_->read(reinterpret_cast<char *>(&header_), sizeof(header_));
  if (!infile_->good()) {
    LOG(error) << "Failed to load NT header from '" << path_ << "'";
    return false;
  }

  if (header_.Signature != IMAGE_NT_SIGNATURE) {
    LOG(error) << "Bad NT signature for DXT library '" << path_ << "'";
    return false;
  }

  if (header_.FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
    LOG(error) << "Incorrect machine target for DXT library '" << path_
               << "' - must be i386";
    return false;
  }

  // By default the nxdk produces IMAGE_SUBSYSTEM_WINDOWS_GUI targets.
  //  if (header_.OptionalHeader.Subsystem != IMAGE_SUBSYSTEM_XBOX) {
  //    LOG(error) << "Incorrect subsystem target for DXT library '" << path_ <<
  //    "' - must be XBOX"; return false;
  //  }

  if (!(header_.OptionalHeader.DllCharacteristics &
        IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE)) {
    LOG(error) << "Dynamic base flag not set for DXT library '" << path_ << "'";
    return false;
  }

  if (header_.FileHeader.SizeOfOptionalHeader >
      sizeof(header_.OptionalHeader)) {
    infile_->ignore(
        static_cast<uint32_t>(header_.FileHeader.SizeOfOptionalHeader -
                              sizeof(header_.OptionalHeader)));
    if (!infile_->good()) {
      LOG(error) << "Failed to skip to section table in '" << path_ << "'";
      return false;
    }
  }

  for (auto i = 0; i < header_.FileHeader.NumberOfSections; ++i) {
    IMAGE_SECTION_HEADER section_header;
    infile_->read(reinterpret_cast<char *>(&section_header),
                  sizeof(section_header));
    if (!infile_->good()) {
      LOG(error) << "Failed to read section table entry in '" << path_ << "'";
      return false;
    }

    section_headers_.emplace_back(section_header);
  }

  return true;
}

bool DXTLibrary::ExtractImportTable() {
  imports_.clear();
  auto &directory =
      header_.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (!directory.Size) {
    return true;
  }

  auto descriptor_start = image_.data() + directory.VirtualAddress;
  auto descriptor =
      reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR *>(descriptor_start);
  // TODO: Prevent reads beyond end of table.
  for (; descriptor->Name; ++descriptor) {
    const char *name =
        reinterpret_cast<char *>(image_.data() + descriptor->Name);

    if (descriptor->ForwarderChain) {
      LOG(error) << "DLL forwarding is not supported at '" << path_ << "'";
      return false;
    }

    uint32_t *thunk;
    uint32_t function_address = descriptor->FirstThunk;
    if (descriptor->DUMMYUNIONNAME.OriginalFirstThunk) {
      thunk = reinterpret_cast<uint32_t *>(
          image_.data() + descriptor->DUMMYUNIONNAME.OriginalFirstThunk);
    } else {
      thunk = reinterpret_cast<uint32_t *>(image_.data() + function_address);
    }

    if (imports_.find(name) == imports_.end()) {
      imports_[name] = {};
    }

    for (; *thunk; ++thunk, function_address += 4) {
      DXTLibraryImport import;

      if (IMAGE_SNAP_BY_ORDINAL(*thunk)) {
        import.ordinal = *thunk & 0xFFFF;
      } else {
        import.ordinal = 0;
        auto name_data = reinterpret_cast<const IMAGE_IMPORT_BY_NAME *>(
            image_.data() + *thunk);
        import.import_name = reinterpret_cast<const char *>(name_data->Name);
      }

      import.function_address = function_address;
      imports_[name].emplace_back(import);
    }
  }

  return true;
}

bool DXTLibrary::ProcessSection(const IMAGE_SECTION_HEADER &header) {
  char name_buf[16] = {0};
  memcpy(name_buf, header.Name, sizeof(header.Name));

  if (!header.SizeOfRawData) {
    // The section likely defines uninitialized data and can be skipped. image_
    // will already reserve space.
    return true;
  }

  infile_->seekg(header.PointerToRawData);
  if (!infile_->good()) {
    LOG(error) << "Failed to seek to section body at 0x" << std::hex
               << header.PointerToRawData << " in '" << path_ << "'";
    return false;
  }

  char *dest = reinterpret_cast<char *>(image_.data()) + header.VirtualAddress;
  infile_->read(dest, header.SizeOfRawData);

  return true;
}

std::ostream &operator<<(std::ostream &os, DXTLibraryImport const &i) {
  if (i.import_name.empty()) {
    return os << "@" << i.ordinal;
  }

  return os << i.import_name;
}