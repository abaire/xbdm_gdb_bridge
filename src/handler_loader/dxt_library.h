#ifndef XBDM_GDB_BRIDGE_DXT_LIBRARY_H
#define XBDM_GDB_BRIDGE_DXT_LIBRARY_H

#include <fstream>
#include <map>
#include <memory>
#include <string>

class COFFLoader;

class DXTLibrary {
 public:
#pragma pack(push, 1)
  struct FileHeader {
    std::string identifier;
    uint32_t modification_timestamp;
    uint32_t owner_id;
    uint32_t group_id;
    uint32_t mode;
    uint32_t size;
  };
#pragma pack(pop)

 public:
  explicit DXTLibrary(std::string path);

  bool Parse();
  std::shared_ptr<COFFLoader> GetLoader() { return loader_; }

 private:
  bool ParseARHeader();
  bool ParseARFileHeader(FileHeader &header);
  bool ParseARSymbolLookupTable(FileHeader &header);
  bool ParseARExtendedFilenameTable(FileHeader &header);
  bool ParseCOFFFileEntry(FileHeader &ar_file_header);

 private:
  const std::string path_;
  std::ifstream infile_;

  std::map<uint32_t, std::string> extended_filenames_;
  std::shared_ptr<COFFLoader> loader_;
};

#endif  // XBDM_GDB_BRIDGE_DXT_LIBRARY_H
