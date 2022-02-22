#include "dxt_library.h"

#include <winapi/winnt.h>

#include <boost/algorithm/string/predicate.hpp>
#include <utility>

#include "coff_loader.h"
#include "util/logging.h"

DXTLibrary::DXTLibrary(std::string path) : path_(std::move(path)) {}

bool DXTLibrary::Parse() {
  infile_.open(path_, std::ios::binary);
  if (!infile_.is_open()) {
    LOG(error) << "Failed to load DXT file from '" << path_ << "'";
    return false;
  }

  // Read the ar file header.
  if (!ParseARHeader()) {
    return false;
  }

  FileHeader file_header{};
  loader_ = std::make_shared<COFFLoader>();
  while (ParseARFileHeader(file_header) && !file_header.identifier.empty()) {
    if (file_header.identifier == "/" ||
        boost::algorithm::starts_with(file_header.identifier, "__.SYMDEF")) {
      if (!ParseARSymbolLookupTable(file_header)) {
        return false;
      }
    } else if (file_header.identifier == "//") {
      if (!ParseARExtendedFilenameTable(file_header)) {
        return false;
      }
    } else if (!ParseCOFFFileEntry(file_header)) {
      return false;
    }

    LOG(info) << file_header.identifier << " " << file_header.size;
    if (file_header.size & 0x01 && !infile_.eof()) {
      char padding;
      infile_.read(&padding, 1);
      if (!infile_.good()) {
        LOG(error) << "Failed to read expected padding byte.";
        return false;
      }
      if (padding != 0x0A) {
        LOG(error) << "Incorrect padding byte " << std::hex
                   << (uint32_t)padding;
        return false;
      }
    }
  }

  return true;
}

bool DXTLibrary::ParseARHeader() {
  char signature[8];
  infile_.read(signature, sizeof(signature));
  if (!infile_.good()) {
    LOG(error) << "Failed to load ar signature from '" << path_ << "'";
    return false;
  }

  if (memcmp(signature, "!<arch>\n", sizeof(signature)) != 0) {
    LOG(error) << "Bad signature for DXT library '" << path_ << "'";
    return false;
  }

  return true;
}

bool DXTLibrary::ParseARFileHeader(DXTLibrary::FileHeader& header) {
  char identifier[16] = {0};
  infile_.read(identifier, 16);
  if (infile_.bad()) {
    LOG(error) << "Failed to load file header identifier from '" << path_
               << "'";
    return false;
  }
  if (infile_.eof()) {
    header.identifier.erase();
    return true;
  }

  // Remove the ASCII space padding.
  for (int32_t i = 15; i >= 0; --i) {
    if (identifier[i] == 0x20) {
      identifier[i] = 0;
    }
  }
  header.identifier = identifier;
  if (header.identifier[0] == '/' && !extended_filenames_.empty()) {
    uint32_t key = strtol(header.identifier.data() + 1, nullptr, 10);
    auto entry = extended_filenames_.find(key);
    if (entry == extended_filenames_.end()) {
      LOG(warning) << "Failed to resolve probable extended filename '"
                   << header.identifier << "'";
    } else {
      header.identifier = entry->second;
    }
  }

  char modification_timestamp[16] = {0};
  infile_.read(modification_timestamp, 12);
  if (!infile_.good()) {
    LOG(error) << "Failed to load file header modification time from '" << path_
               << "'";
    return false;
  }
  header.modification_timestamp = strtol(modification_timestamp, nullptr, 10);

  char owner_id[8] = {0};
  infile_.read(owner_id, 6);
  if (!infile_.good()) {
    LOG(error) << "Failed to load file header owner id from '" << path_ << "'";
    return false;
  }
  header.owner_id = strtol(owner_id, nullptr, 10);

  char group_id[8] = {0};
  infile_.read(group_id, 6);
  if (!infile_.good()) {
    LOG(error) << "Failed to load file header group id from '" << path_ << "'";
    return false;
  }
  header.group_id = strtol(group_id, nullptr, 10);

  char file_mode[16] = {0};
  infile_.read(file_mode, 8);
  if (!infile_.good()) {
    LOG(error) << "Failed to load file mode from '" << path_ << "'";
    return false;
  }
  header.mode = strtol(file_mode, nullptr, 8);

  char file_size[16] = {0};
  infile_.read(file_size, 10);
  if (!infile_.good()) {
    LOG(error) << "Failed to load file size from '" << path_ << "'";
    return false;
  }
  header.size = strtol(file_size, nullptr, 10);

  char footer[2] = {0};
  infile_.read(footer, 2);
  if (!infile_.good()) {
    LOG(error) << "Failed to load file header terminator from '" << path_
               << "'";
    return false;
  }

  const char expected_footer[] = {0x60, 0x0A};
  if (memcmp(footer, expected_footer, 2) != 0) {
    LOG(error) << "Bad footer in file header '" << header.identifier
               << "' from '" << path_ << "'";
    return false;
  }

#ifdef __APPLE__
  // Handle BSD-style names.
  if (boost::algorithm::starts_with(header.identifier, "#1/")) {
    uint32_t length = strtol(header.identifier.data() + 3, nullptr, 10);
    auto name_buffer = new char[length];
    infile_.read(name_buffer, length);
    if (!infile_.good()) {
      LOG(error) << "Failed to load extended name from '" << path_ << "'";
      delete[] name_buffer;
      return false;
    }
    header.identifier = name_buffer;
    delete[] name_buffer;
    header.size -= length;
  }
#endif

  return true;
}

bool DXTLibrary::ParseARSymbolLookupTable(DXTLibrary::FileHeader& header) {
  // The symbol jump table is not needed for this loader and can be ignored.
  infile_.ignore(header.size);
  return infile_.good();
}

bool DXTLibrary::ParseARExtendedFilenameTable(DXTLibrary::FileHeader& header) {
  extended_filenames_.clear();

  // Flat list of strings separated by one or more 0x0A characters.
  auto flat_table = new char[header.size];
  infile_.read(flat_table, header.size);
  if (!infile_.good()) {
    LOG(error) << "Failed to read extended name table '" << header.identifier
               << "' from '" << path_ << "'";
    delete[] flat_table;
    return false;
  }

  auto table_end = flat_table + header.size;
  auto start = flat_table;

  while (start < table_end) {
    while (*start == 0x0A && start < table_end) {
      ++start;
    }
    if (start == table_end) {
      break;
    }

    auto end = start;
    while (*end != 0x0A && end < table_end) {
      ++end;
    }

    uint32_t offset = start - flat_table;
    extended_filenames_[offset] = std::string(start, end);
    start = end + 1;
  }

  delete[] flat_table;

  return true;
}

bool DXTLibrary::ParseCOFFFileEntry(DXTLibrary::FileHeader& ar_file_header) {
  if (ar_file_header.size < sizeof(IMAGE_FILE_HEADER)) {
    LOG(error) << "Bad COFF header for " << ar_file_header.identifier
               << " from '" << path_ << "'";
    return false;
  }

  return loader_->Load(infile_, ar_file_header.size, ar_file_header.identifier,
                       path_);
}
