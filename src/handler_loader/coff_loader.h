#ifndef XBDM_GDB_BRIDGE_COFF_LOADER_H
#define XBDM_GDB_BRIDGE_COFF_LOADER_H

#include <winapi/winnt.h>

#include <fstream>
#include <map>
#include <string>
#include <vector>

struct COFFSection {
  struct Relocation {
    enum Type {
      IMAGE_REL_I386_ABSOLUTE = 0x00,
      IMAGE_REL_I386_DIR32 = 0x06,
      IMAGE_REL_I386_DIR32NB = 0x07,
      IMAGE_REL_I386_SECTION = 0x0A,
      IMAGE_REL_I386_SECREL = 0x0B,
      IMAGE_REL_I386_TOKEN = 0x0C,
      IMAGE_REL_I386_SECREL7 = 0x0D,
      IMAGE_REL_I386_REL32 = 0x14,
    };
    uint32_t virtual_address;
    uint32_t symbol_table_index;
    Type type;
  };

  explicit COFFSection(const IMAGE_SECTION_HEADER &section_header);
  bool Parse(const char *body, uint32_t base_offset);

  [[nodiscard]] bool ShouldLoad() const;
  [[nodiscard]] uint32_t VirtualSize() const;
  [[nodiscard]] const std::string &Name() const;

  void SetVirtualAddress(uint32_t address) { virtual_address = address; }

  IMAGE_SECTION_HEADER header;
  std::vector<uint8_t> body;

  std::string name;
  uint32_t virtual_address{0};

  uint32_t alignment{0};

  bool executable{false};
  bool readable{false};
  bool writable{false};

  bool contains_code{false};
  bool contains_initialized_data{false};
  bool contains_uninitialized_data{false};

  bool global_pointer_relative{false};
  bool remove{false};

  std::vector<Relocation> relocations;
};

struct COFFLoader {
  struct Symbol {
    std::string name;

    union {
      struct {
        uint32_t value;
        int16_t section_number;
        uint16_t type;
        uint8_t storage_class;
      } data;

      char _raw[18];

      struct {
        uint32_t size;
        uint16_t num_relocations;
        uint16_t num_line_numbers;
        uint32_t checksum;
        uint16_t section_number;
        uint8_t selection_number;
      } section;
    };

    // For aux symbols, this will be set to the index of the symbol to which
    // this aux symbol belongs.
    int32_t parent_symbol_index{-1};
    // For parents of aux symbols, this will contain the indices of aux Symbol
    // entries.
    std::vector<int32_t> aux_symbols;

    [[nodiscard]] bool IsAuxSection() const { return parent_symbol_index >= 0; }
  };

  bool Load(std::ifstream &stream, uint32_t file_size,
            const std::string &file_identifier,
            const std::string &library_path);

  // Resolves the available addresses in the symbol table.
  // For extern definitions, populates the given map of
  //   {extern_name: resolved_symbol_table__index}.
  void ResolveSymbolTable(std::map<std::string, uint32_t> &externs);

  // Apply relocations to sections. Sections must have virtual addresses
  // assigned prior to calling this method.
  bool Relocate();

  std::vector<COFFSection> sections;
  std::vector<Symbol> symbol_table;
  std::vector<std::string> string_table;
  std::map<uint32_t, uint32_t> string_offset_index;

  // The fixed up address of each symbol in the symbol table.
  std::vector<uint32_t> resolved_symbol_table;

 private:
  bool ParseSymbolTable(const char *symbol_table, uint32_t num_symbols);
  void PopulateSectionNames();
};

#endif  // XBDM_GDB_BRIDGE_COFF_LOADER_H
