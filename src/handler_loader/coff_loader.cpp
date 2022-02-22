#include "coff_loader.h"

#include "util/logging.h"

#define IMAGE_SCN_CNT_CODE 0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA 0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define IMAGE_SCN_LNK_INFO 0x00000200
#define IMAGE_SCN_LNK_REMOVE 0x00000800
#define IMAGE_SCN_GPREL 0x00008000
#define IMAGE_SCN_ALIGN_MASK 0x00F00000
#define IMAGE_SCN_LNK_NRELOC_OVFL 0x01000000
#define IMAGE_SCN_MEM_DISCARDABLE 0x02000000
#define IMAGE_SCN_MEM_NOT_CACHED 0x04000000
#define IMAGE_SCN_MEM_NOT_PAGED 0x08000000
#define IMAGE_SCN_MEM_SHARED 0x10000000
#define IMAGE_SCN_MEM_EXECUTE 0x20000000
#define IMAGE_SCN_MEM_READ 0x40000000
#define IMAGE_SCN_MEM_WRITE 0x80000000

#define IMAGE_SYM_UNDEFINED 0
#define IMAGE_SYM_ABSOLUTE -1
#define IMAGE_SYM_DEBUG -2

#pragma pack(push, 2)
struct COFFSymbolTableEntry {
  union {
    char short_name[8];
    struct {
      uint32_t zeroes;
      uint32_t offset;
    } long_name;
  } name;
  uint32_t value;
  int16_t section_number;
  uint16_t type;
  uint8_t storage_class;
  uint8_t number_of_aux_symbols;

  [[nodiscard]] bool HasLongName() const { return name.long_name.zeroes == 0; }
};
#pragma pack(pop)

bool COFFLoader::Load(std::ifstream &stream, uint32_t file_size,
                      const std::string &file_identifier,
                      const std::string &library_path) {
  uint32_t file_offset = 0;
  IMAGE_FILE_HEADER coff_header;
  stream.read(reinterpret_cast<char *>(&coff_header), sizeof(coff_header));
  file_offset += sizeof(coff_header);
  if (stream.bad()) {
    LOG(error) << "Failed to load file content for " << file_identifier
               << " from '" << library_path << "'";
    return false;
  }

  if (coff_header.Machine != IMAGE_FILE_MACHINE_I386) {
    LOG(error) << "Content for " << file_identifier
               << " was not built for i386 and cannot be used";
    return false;
  }

  if (coff_header.Characteristics & IMAGE_FILE_RELOCS_STRIPPED) {
    LOG(error)
        << "Content for " << file_identifier << " in '" << library_path
        << "' had relocation information stripped and cannot be processed";
    return false;
  }

  for (auto section = 0; section < coff_header.NumberOfSections; ++section) {
    IMAGE_SECTION_HEADER section_header;
    stream.read(reinterpret_cast<char *>(&section_header),
                sizeof(section_header));
    file_offset += sizeof(section_header);
    if (stream.bad()) {
      LOG(error) << "Failed to load COFF section header for " << file_identifier
                 << " from '" << library_path << "'";
      return false;
    }

    sections.emplace_back(section_header);
  }

  uint32_t body_size = file_size - file_offset;
  auto *body = new char[body_size];
  stream.read(body, body_size);
  if (stream.bad()) {
    LOG(error) << "Failed to load COFF file body for " << file_identifier
               << " from '" << library_path << "'";
    delete[] body;
    return false;
  }

  auto *body_end = body;
  for (auto &section : sections) {
    if (!section.Parse(body, file_offset)) {
      delete[] body;
      return false;
    }
  }

  auto symbol_table_start =
      body + coff_header.PointerToSymbolTable - file_offset;

  // Parse the string table so symbol names can be resolved.
  string_table.clear();
  string_offset_index.clear();
  auto read_offset = symbol_table_start +
                     coff_header.NumberOfSymbols * sizeof(COFFSymbolTableEntry);
  auto string_table_size = *reinterpret_cast<const uint32_t *>(read_offset);

  read_offset += 4;
  for (uint32_t i = 0; i < string_table_size - 4;) {
    string_offset_index[i + 4] = string_table.size();
    string_table.emplace_back(read_offset);
    uint32_t length = string_table.back().size() + 1;
    read_offset += length;
    i += length;
  }

  if (coff_header.PointerToSymbolTable) {
    if (!ParseSymbolTable(symbol_table_start, coff_header.NumberOfSymbols)) {
      delete[] body;
      return false;
    }
  }

  delete[] body;

  PopulateSectionNames();

  return true;
}

bool COFFLoader::ParseSymbolTable(const char *read_offset,
                                  uint32_t num_symbols) {
  symbol_table.clear();
  auto entry = reinterpret_cast<const COFFSymbolTableEntry *>(read_offset);
  for (auto i = 0; i < num_symbols; ++i, ++entry) {
    Symbol symbol;

    symbol.name = "<BROKEN>";
    if (entry->HasLongName()) {
      auto string_entry =
          string_offset_index.find(entry->name.long_name.offset);
      if (string_entry == string_offset_index.end()) {
        LOG(error) << "Failed to look up " << entry->name.long_name.offset
                   << " in string table.";
      } else {
        symbol.name = string_table[string_entry->second];
      }
    } else {
      char buf[16] = {0};
      memcpy(buf, entry->name.short_name, sizeof(entry->name.short_name));
      symbol.name = std::string(buf);
    }

    symbol.data.value = entry->value;
    symbol.data.section_number = entry->section_number;
    symbol.data.type = entry->type;
    symbol.data.storage_class = entry->storage_class;
    symbol.parent_symbol_index = -1;
    symbol_table.emplace_back(symbol);

    int32_t parent_symbol_index = static_cast<int32_t>(symbol_table.size()) - 1;
    for (auto aux = 0; aux < entry->number_of_aux_symbols; ++aux) {
      ++entry;
      ++i;

      Symbol aux_symbol;
      memcpy(aux_symbol._raw, reinterpret_cast<const char *>(entry),
             sizeof(aux_symbol._raw));
      aux_symbol.name = symbol.name;
      char buf[16] = {0};
      snprintf(buf, 15, "_aux%d", aux);
      aux_symbol.name += buf;
      aux_symbol.parent_symbol_index = parent_symbol_index;
      symbol_table.emplace_back(aux_symbol);
      symbol_table[parent_symbol_index].aux_symbols.push_back(
          static_cast<int32_t>(symbol_table.size()) - 1);
    };
  }

  return true;
}

void COFFLoader::PopulateSectionNames() {
  for (auto &section : sections) {
    char name[16] = {0};
    memcpy(name, section.header.Name, sizeof(section.header.Name));
    section.name = name;

    if (name[0] == '/') {
      const char *offset = name + 1;
      auto table_offset = strtol(offset, nullptr, 10);
      auto entry = string_offset_index.find(table_offset);
      if (entry == string_offset_index.end()) {
        LOG(error) << "Failed to look up " << name << " in string table.";
      } else {
        section.name = string_table[entry->second];
      }
    }
  }
}

void COFFLoader::ResolveSymbolTable(std::map<std::string, uint32_t> &externs) {
  resolved_symbol_table.clear();
  for (auto &symbol : symbol_table) {
    uint32_t resolved_address = 0;

    if (symbol.data.section_number == IMAGE_SYM_DEBUG) {
      // Silently ignore debug entries.
      return;
    }

    if (symbol.data.section_number == IMAGE_SYM_ABSOLUTE) {
      LOG(warning) << "Absolute address for " << symbol.name;
      resolved_address = symbol.data.value;
    } else if (symbol.data.section_number == IMAGE_SYM_UNDEFINED) {
      if (!symbol.IsAuxSection()) {
        externs[symbol.name] = resolved_symbol_table.size();
      }
    } else {
      if (symbol.data.section_number - 1 >= sections.size()) {
        assert(!"Invalid section number");
      }
      assert(symbol.data.section_number - 1 < sections.size());
      auto &section_target = sections[symbol.data.section_number - 1];
      resolved_address = section_target.virtual_address + symbol.data.value;
    }

    resolved_symbol_table.emplace_back(resolved_address);
  }
}

bool COFFLoader::Relocate() {
  for (auto &section : sections) {
    if (section.relocations.empty()) {
      continue;
    }

    for (auto &relocation : section.relocations) {
      auto &symbol = symbol_table[relocation.symbol_table_index];

      uint32_t value = resolved_symbol_table[relocation.symbol_table_index];

      auto data = section.body.data();

      switch (relocation.type) {
        case COFFSection::Relocation::IMAGE_REL_I386_ABSOLUTE:
          assert(!"TODO: Implement IMAGE_REL_I386_ABSOLUTE");
          break;

        case COFFSection::Relocation::IMAGE_REL_I386_DIR32:
          memcpy(data + relocation.virtual_address, &value, sizeof(value));
          break;

        case COFFSection::Relocation::IMAGE_REL_I386_DIR32NB:
          assert(!"TODO: Implement IMAGE_REL_I386_DIR32NB");
          break;

        case COFFSection::Relocation::IMAGE_REL_I386_SECTION:
          assert(!"TODO: Implement IMAGE_REL_I386_SECTION");
          break;

        case COFFSection::Relocation::IMAGE_REL_I386_SECREL:
          assert(!"TODO: Implement IMAGE_REL_I386_SECREL");
          break;

        case COFFSection::Relocation::IMAGE_REL_I386_TOKEN:
          assert(!"TODO: Implement IMAGE_REL_I386_TOKEN");
          break;

        case COFFSection::Relocation::IMAGE_REL_I386_SECREL7:
          assert(!"TODO: Implement IMAGE_REL_I386_SECREL7");
          break;

        case COFFSection::Relocation::IMAGE_REL_I386_REL32: {
          uint32_t patch_address =
              section.virtual_address + relocation.virtual_address + 4;
          int32_t relative_address =
              static_cast<int32_t>(value) - static_cast<int32_t>(patch_address);
          memcpy(data + relocation.virtual_address, &relative_address,
                 sizeof(relative_address));
          break;
        }
      }
    }
  }

  return true;
}

COFFSection::COFFSection(const IMAGE_SECTION_HEADER &section_header)
    : header(section_header) {}

bool COFFSection::Parse(const char *file_body, uint32_t base_offset) {
  body.clear();
  relocations.clear();

  remove = (header.Characteristics & IMAGE_SCN_LNK_REMOVE) != 0;
  if (remove) {
    return true;
  }

  contains_code = (header.Characteristics & IMAGE_SCN_CNT_CODE) != 0;
  contains_initialized_data =
      (header.Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA) != 0;
  contains_uninitialized_data =
      (header.Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA) != 0;

  assert(!(header.Characteristics & IMAGE_SCN_LNK_INFO));
  assert(!(header.Characteristics & IMAGE_SCN_LNK_NRELOC_OVFL));

  const char *body_start = file_body + header.PointerToRawData - base_offset;
  const char *body_end = body_start + header.SizeOfRawData;
  body.insert(body.end(), body_start, body_end);

  global_pointer_relative = (header.Characteristics & IMAGE_SCN_GPREL) != 0;

  uint32_t log_align =
      ((header.Characteristics & IMAGE_SCN_ALIGN_MASK) >> 20) - 1;
  alignment = 1 << log_align;

  executable = (header.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
  readable = (header.Characteristics & IMAGE_SCN_MEM_READ) != 0;
  writable = (header.Characteristics & IMAGE_SCN_MEM_WRITE) != 0;

  if (!header.PointerToRelocations || !header.NumberOfRelocations) {
    return true;
  }

  const char *relocation_start =
      file_body + header.PointerToRelocations - base_offset;
  for (auto i = 0; i < header.NumberOfRelocations; ++i) {
    auto v_address = reinterpret_cast<const uint32_t *>(relocation_start);
    relocation_start += 4;
    auto symbol_table_index =
        reinterpret_cast<const uint32_t *>(relocation_start);
    relocation_start += 4;
    auto type = reinterpret_cast<const uint16_t *>(relocation_start);
    relocation_start += 2;

    Relocation r{*v_address, *symbol_table_index,
                 static_cast<Relocation::Type>(*type)};
    relocations.emplace_back(r);
  }

  return true;
}

bool COFFSection::ShouldLoad() const { return !remove && !body.empty(); }

uint32_t COFFSection::VirtualSize() const { return header.SizeOfRawData; }

const std::string &COFFSection::Name() const { return name; }
