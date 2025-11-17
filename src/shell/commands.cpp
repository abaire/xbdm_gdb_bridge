#include "commands.h"

#include <SDL_image.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "file_util.h"
#include "screenshot_converter.h"
#include "util/parsing.h"
#include "xbox/debugger/debugger_xbox_interface.h"

static void SendAndPrintMessage(
    XBOXInterface& interface,
    const std::shared_ptr<RDCPProcessedRequest>& request, std::ostream& out) {
  interface.SendCommandSync(request);
  out << *request << std::endl;
}

Command::Result CommandBreak::operator()(XBOXInterface& interface,
                                         const std::vector<std::string>& args,
                                         std::ostream& out) {
  ArgParser parser(args, true);
  if (!parser.HasCommand()) {
    PrintUsage();
    return HANDLED;
  }

  if (parser.IsCommand("start")) {
    SendAndPrintMessage(interface, std::make_shared<BreakAtStart>(), out);
    return HANDLED;
  }

  if (parser.IsCommand("clearall")) {
    SendAndPrintMessage(interface, std::make_shared<BreakClearAll>(), out);
    return HANDLED;
  }

  bool clear = parser.ShiftPrefixModifier('-');

  if (parser.IsCommand("a", "addr", "Address")) {
    uint32_t address;
    if (!parser.Parse(0, address)) {
      out << "Missing required Address argument." << std::endl;
      PrintUsage();
      return HANDLED;
    }
    SendAndPrintMessage(interface,
                        std::make_shared<BreakAddress>(address, clear), out);
    return HANDLED;
  }

  if (parser.IsCommand("r", "read")) {
    uint32_t address;
    if (!parser.Parse(0, address)) {
      out << "Missing required Address argument." << std::endl;
      PrintUsage();
      return HANDLED;
    }
    int32_t size = 1;
    parser.Parse(1, size);
    SendAndPrintMessage(
        interface, std::make_shared<BreakOnRead>(address, size, clear), out);
    return HANDLED;
  }

  if (parser.IsCommand("w", "write")) {
    uint32_t address;
    if (!parser.Parse(0, address)) {
      out << "Missing required Address argument." << std::endl;
      PrintUsage();
      return HANDLED;
    }
    int32_t size = 1;
    parser.Parse(1, size);
    SendAndPrintMessage(
        interface, std::make_shared<BreakOnWrite>(address, size, clear), out);
    return HANDLED;
  }

  if (parser.IsCommand("e", "exec", "execute")) {
    uint32_t address;
    if (!parser.Parse(0, address)) {
      out << "Missing required Address argument." << std::endl;
      PrintUsage();
      return HANDLED;
    }
    int32_t size = 1;
    parser.Parse(1, size);
    SendAndPrintMessage(
        interface, std::make_shared<BreakOnExecute>(address, size, clear), out);
    return HANDLED;
  }

  out << "Invalid mode " << args.front() << std::endl;
  PrintUsage();
  return HANDLED;
}

Command::Result CommandBye::operator()(XBOXInterface& interface,
                                       const std::vector<std::string>&,
                                       std::ostream& out) {
  SendAndPrintMessage(interface, std::make_shared<Bye>(), out);
  return HANDLED;
}

Command::Result CommandContinue::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    out << "Missing required thread_id argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  bool exception = false;
  parser.Parse(1, exception);

  SendAndPrintMessage(interface,
                      std::make_shared<Continue>(thread_id, exception), out);
  return HANDLED;
}

Command::Result CommandDebugOptions::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  if (args.empty()) {
    SendAndPrintMessage(interface, std::make_shared<GetDebugOptions>(), out);
    return HANDLED;
  }

  ArgParser parser(args);
  bool enable_crashdump = parser.ArgExists("c", "crashdump");
  bool enable_dcptrace = parser.ArgExists("d", "dpctrace");
  SendAndPrintMessage(
      interface,
      std::make_shared<SetDebugOptions>(enable_crashdump, enable_dcptrace),
      out);
  return HANDLED;
}

Command::Result CommandDebugger::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  ArgParser parser(args);
  bool disable = parser.ArgExists("d", "disable", "off");
  SendAndPrintMessage(interface, std::make_shared<Debugger>(disable), out);
  return HANDLED;
}

// Command::Result CommandDebugMode::operator()(
//     XBOXInterface &interface, const std::vector<std::string> &args) {
//   SendAndPrintMessage(interface, std::make_shared<DebugMode>());
//   return HANDLED;
// }

Command::Result CommandDedicate::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  if (args.empty()) {
    SendAndPrintMessage(interface, std::make_shared<Dedicate>(nullptr), out);
  } else {
    SendAndPrintMessage(interface, std::make_shared<Dedicate>(args[0].c_str()),
                        out);
  }
  return HANDLED;
}

Command::Result CommandDelete::operator()(XBOXInterface& interface,
                                          const std::vector<std::string>& args,
                                          std::ostream& out) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    out << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  path = EnsureXFATStylePath(path);
  bool recursive = parser.ArgExists("-r");
  if (recursive) {
    bool exists;
    bool is_directory;
    if (CheckRemotePath(interface, path, exists, is_directory, out) &&
        is_directory) {
      DeleteRecursively(interface, path, out);
      return HANDLED;
    }
  }

  SendAndPrintMessage(interface, std::make_shared<Delete>(path, recursive),
                      out);
  return HANDLED;
}

Command::Result CommandDirList::operator()(XBOXInterface& interface,
                                           const std::vector<std::string>& args,
                                           std::ostream& out) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    out << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  path = EnsureXFATStylePath(path);

  // Prevent access denied errors when trying to list the base of a drive path.
  if (path.back() == ':') {
    path.push_back('\\');
  }

  std::list<DirList::Entry> directories;
  std::list<DirList::Entry> files;
  if (!FetchDirectoryEntries(interface, path, directories, files, out)) {
    return HANDLED;
  }
  auto comparator = [](const DirList::Entry& a, const DirList::Entry& b) {
    return boost::algorithm::to_lower_copy(a.name) <
           boost::algorithm::to_lower_copy(b.name);
  };
  directories.sort(comparator);
  files.sort(comparator);

  for (auto& entry : directories) {
    out << "           " << entry.name << "\\" << std::endl;
  }
  for (auto& entry : files) {
    out << std::setw(10) << entry.filesize << " " << entry.name << std::endl;
  }

  return HANDLED;
}

Command::Result CommandDebugMonitorVersion::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  auto request = std::make_shared<DebugMonitorVersion>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    out << request->version << std::endl;
  }
  return HANDLED;
}

Command::Result CommandDriveFreeSpace::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  ArgParser parser(args);
  std::string drive_letter;
  if (!parser.Parse(0, drive_letter)) {
    out << "Missing required drive_letter argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  auto request = std::make_shared<DriveFreeSpace>(drive_letter);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    out << "total: " << request->total_bytes
        << " total free: " << request->free_bytes
        << " free to caller: " << request->free_to_caller << std::endl;
  }
  return HANDLED;
}

Command::Result CommandDriveList::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  auto request = std::make_shared<DriveList>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    for (auto& letter : request->drives) {
      out << letter << std::endl;
    }
  }

  return HANDLED;
}

Command::Result CommandGetChecksum::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  ArgParser parser(args);
  int address;
  int length;
  int block_size;
  if (!parser.Parse(0, address)) {
    out << "Missing required Address argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  if (!parser.Parse(1, length)) {
    out << "Missing required length argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  if (!parser.Parse(2, block_size)) {
    out << "Missing required block_size argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  if (address & 0x07) {
    out << "Address must be evenly divisible by 8." << std::endl;
    return HANDLED;
  }
  if (length & 0x07) {
    out << "length must be evenly divisible by 8." << std::endl;
    return HANDLED;
  }
  if (block_size & 0x07) {
    out << "block_size must be evenly divisible by 8." << std::endl;
    return HANDLED;
  }

  auto request = std::make_shared<GetChecksum>(address, length, block_size);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    for (auto& checksum : request->checksums) {
      out << std::hex << std::setfill('0') << std::setw(8) << checksum
          << std::endl;
    }
  }

  return Command::HANDLED;
}

Command::Result CommandGetContext::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    out << "Missing required thread_id argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  bool enable_control = true;
  bool enable_integer = true;
  bool enable_floatingpoint = true;

  if (args.size() > 1) {
    enable_control = parser.ArgExists("control", "c");
    enable_integer = parser.ArgExists("integer", "int", "i");
    enable_floatingpoint = parser.ArgExists("float", "fp", "f");
  }

  auto request = std::make_shared<GetContext>(
      thread_id, enable_control, enable_integer, enable_floatingpoint);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    out << request->context << std::endl;
  }
  return HANDLED;
}

Command::Result CommandGetExtContext::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    out << "Missing required thread_id argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  auto request = std::make_shared<GetExtContext>(thread_id);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    out << request->context << std::endl;
  }
  return HANDLED;
}

Command::Result CommandGetFile::operator()(XBOXInterface& interface,
                                           const std::vector<std::string>& args,
                                           std::ostream& out) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    out << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  path = EnsureXFATStylePath(path);

  bool exists;
  bool is_directory;
  if (!CheckRemotePath(interface, path, exists, is_directory, out)) {
    return HANDLED;
  }

  if (!exists) {
    out << "No such file." << std::endl;
    return HANDLED;
  }

  std::string local_path_str;
  std::filesystem::path local_path;
  if (!parser.Parse(1, local_path_str)) {
    std::string portable_path = path;
    std::replace(portable_path.begin(), portable_path.end(), '\\', '/');
    local_path = std::filesystem::path(portable_path).filename();
  } else {
    local_path = local_path_str;
  }

  if (is_directory) {
    out << "Recursively fetching files from " << path << std::endl;
    std::error_code err;
    std::filesystem::create_directories(local_path, err);
    SaveDirectory(interface, path, local_path, out);
  } else {
    std::filesystem::path parent_dir = local_path.parent_path();
    if (!parent_dir.empty()) {
      std::error_code err;
      std::filesystem::create_directories(parent_dir, err);
    }
    if (SaveFile(interface, path, local_path, out)) {
      out << path << " -> " << local_path << std::endl;
    }
  }

  return HANDLED;
}

Command::Result CommandGetFileAttributes::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    out << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  path = EnsureXFATStylePath(path);

  auto request = std::make_shared<GetFileAttributes>(path);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    out << "Size: " << request->filesize << " ";
    for (auto& f : request->flags) {
      out << f << " ";
    }
    out << std::endl;
  }
  return HANDLED;
}

Command::Result CommandGetMem::operator()(XBOXInterface& interface,
                                          const std::vector<std::string>& args,
                                          std::ostream& out) {
  ArgParser parser(args);
  uint32_t address;
  uint32_t size;
  if (!parser.Parse(0, address)) {
    out << "Missing required Address argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  if (!parser.Parse(1, size)) {
    out << "Missing required size argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  auto request = std::make_shared<GetMemBinary>(address, size);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    int count = 0;
    for (auto& val : request->data) {
      out << std::hex << std::setw(2) << std::setfill('0')
          << static_cast<uint32_t>(val) << " ";
      if (count && (count % 32) == 0) {
        out << std::endl;
      }
      ++count;
    }
    if (((count - 1) % 32) != 0) {
      out << std::endl;
    }
  }

  return HANDLED;
}

Command::Result CommandGetProcessID::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  auto request = std::make_shared<GetProcessID>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    out << std::hex << std::setfill('0') << std::setw(8) << request->process_id
        << std::endl;
  }
  return HANDLED;
}

Command::Result CommandGetUtilityDriveInfo::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  auto request = std::make_shared<GetUtilityDriveInfo>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    for (auto& partition : request->partitions) {
      out << partition.first << ": 0x" << std::hex << std::setfill('0')
          << std::setw(8) << partition.second << std::endl;
    }
  }
  return HANDLED;
}

Command::Result CommandGo::operator()(XBOXInterface& interface,
                                      const std::vector<std::string>& args,
                                      std::ostream& out) {
  SendAndPrintMessage(interface, std::make_shared<Go>(), out);
  return HANDLED;
}

Command::Result CommandHalt::operator()(XBOXInterface& interface,
                                        const std::vector<std::string>& args,
                                        std::ostream& out) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    SendAndPrintMessage(interface, std::make_shared<Halt>(), out);
  } else {
    SendAndPrintMessage(interface, std::make_shared<Halt>(thread_id), out);
  }

  return HANDLED;
}

Command::Result CommandIsBreak::operator()(XBOXInterface& interface,
                                           const std::vector<std::string>& args,
                                           std::ostream& out) {
  ArgParser parser(args);
  int address;
  if (!parser.Parse(0, address)) {
    out << "Missing required Address argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  auto request = std::make_shared<IsBreak>(address);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    switch (request->type) {
      case IsBreak::Type::NONE:
        out << "No breakpoint" << std::endl;
        break;

      case IsBreak::Type::WRITE:
        out << "Write" << std::endl;
        break;

      case IsBreak::Type::READ_OR_WRITE:
        out << "Read/Write" << std::endl;
        break;

      case IsBreak::Type::EXECUTE:
        out << "Execute" << std::endl;
        break;

      case IsBreak::Type::ADDRESS:
        out << "Previously set breakpoint" << std::endl;
        break;
    }
  }
  return HANDLED;
}

Command::Result CommandIsDebugger::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  auto request = std::make_shared<IsDebugger>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    out << request->attached << std::endl;
  }
  return HANDLED;
}

Command::Result CommandIsStopped::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    out << "Missing required thread_id argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  auto request = std::make_shared<IsStopped>(thread_id);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    if (!request->stopped) {
      out << "Not stopped." << std::endl;
    } else {
      out << *request->stop_reason << std::endl;
    }
  }

  return HANDLED;
}

Command::Result CommandMagicBoot::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    out << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  path = EnsureXFATStylePath(path);
  bool nodebug = parser.ArgExists("nodebug");
  bool cold = parser.ArgExists("cold");
  SendAndPrintMessage(interface,
                      std::make_shared<MagicBoot>(path, !nodebug, cold), out);
  return HANDLED;
}

Command::Result CommandMemoryMap::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  auto request = std::make_shared<MemoryMapGlobal>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    out << std::setfill('0') << std::hex << std::setw(8)
        << "MmHighestPhysicalPage: 0x" << request->mm_highest_physical_page
        << " RetailPfnRegion: 0x" << request->retail_pfn_region
        << " SystemPteRange: 0x" << request->system_pte_range
        << " AvailablePages: " << std::dec << request->available_pages
        << " AllocatedPagesByUsage: " << request->allocated_pages_by_usage
        << " PfnDatabase: 0x" << std::hex << request->pfn_database
        << " AddressSpaceLock: " << std::dec << request->address_space_lock
        << " VadRoot: 0x" << std::hex << request->vad_root << " VadHint: 0x"
        << request->vad_hint << " VadFreeHint: 0x" << request->vad_free_hint
        << " MmNumberOfPhysicalPages: " << std::dec
        << request->mm_number_of_physical_pages
        << " MmAvailablePages: " << request->mm_available_pages << std::endl;
  }

  return HANDLED;
}

Command::Result CommandMakeDirectory::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    out << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  path = EnsureXFATStylePath(path);
  SendAndPrintMessage(interface, std::make_shared<Mkdir>(path), out);
  return HANDLED;
}

Command::Result CommandModuleSections::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    out << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  path = EnsureXFATStylePath(path);
  auto request = std::make_shared<ModSections>(path);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    for (auto& m : request->sections) {
      out << m << std::endl;
    }
  }
  return HANDLED;
}

Command::Result CommandModules::operator()(XBOXInterface& interface,
                                           const std::vector<std::string>& args,
                                           std::ostream& out) {
  auto request = std::make_shared<Modules>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    for (auto& m : request->modules) {
      out << m << std::endl;
    }
  }
  return HANDLED;
}

Command::Result CommandNoStopOn::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  uint32_t flags = 0;
  ArgParser parser(args);
  if (parser.ArgExists("fce", "exception")) {
    flags |= NoStopOn::kFirstChanceException;
  }
  if (parser.ArgExists("debugstr")) {
    flags |= NoStopOn::kDebugStr;
  }
  if (parser.ArgExists("createthread")) {
    flags |= NoStopOn::kCreateThread;
  }
  if (parser.ArgExists("stacktrace")) {
    flags |= NoStopOn::kStacktrace;
  }
  if (!flags) {
    flags = NoStopOn::kAll;
  }
  SendAndPrintMessage(interface, std::make_shared<NoStopOn>(flags), out);
  return HANDLED;
}

Command::Result CommandNotifyAt::operator()(
    XBOXInterface& base_interface, const std::vector<std::string>& args,
    std::ostream& out) {
  GET_DEBUGGERXBOXINTERFACE(base_interface, interface);
  ArgParser parser(args);
  uint32_t port;
  if (!parser.Parse(0, port)) {
    out << "Missing required Port argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  if (port < 1024 || port > 65535) {
    out << "Invalid Port argument, must be between 1024 and 65535."
        << std::endl;
    return HANDLED;
  }

  IPAddress address(port);
  if (!interface.StartNotificationListener(address)) {
    out << "Failed to start notification listener on Port " << port
        << std::endl;
    return HANDLED;
  }

  bool drop_flag = parser.ArgExists("drop");
  bool debug_flag = parser.ArgExists("debug");
  SendAndPrintMessage(
      interface, std::make_shared<NotifyAt>(port, drop_flag, debug_flag), out);

  if (!drop_flag) {
    interface.AttachDebugNotificationHandler();
  } else {
    interface.DetachDebugNotificationHandler();
  }
  return HANDLED;
}

Command::Result CommandPutFile::operator()(XBOXInterface& interface,
                                           const std::vector<std::string>& args,
                                           std::ostream& out) {
  ArgParser parser(args);
  std::string local_path;
  if (!parser.Parse(0, local_path)) {
    out << "Missing required local_path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  std::string remote_path;
  if (!parser.Parse(1, remote_path)) {
    out << "Missing required remote_path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  remote_path = EnsureXFATStylePath(remote_path);

  bool is_directory = std::filesystem::is_directory(local_path);
  if (!is_directory && !std::filesystem::is_regular_file(local_path)) {
    out << "Invalid local_path, must be a regular file or a directory."
        << std::endl;
    return HANDLED;
  }

  UploadFileOverwriteAction overwrite_action =
      parser.ArgExists("allow_overwrite", "overwrite", "-f")
          ? UploadFileOverwriteAction::OVERWRITE
          : (is_directory ? UploadFileOverwriteAction::SKIP
                          : UploadFileOverwriteAction::ABORT);

  if (is_directory) {
    UploadDirectory(interface, local_path, remote_path, overwrite_action, true,
                    out);
  } else {
    UploadFile(interface, local_path, remote_path, overwrite_action, out);
  }

  return HANDLED;
}

Command::Result CommandRename::operator()(XBOXInterface& interface,
                                          const std::vector<std::string>& args,
                                          std::ostream& out) {
  ArgParser parser(args);
  std::string path;
  std::string new_path;
  if (!parser.Parse(0, path)) {
    out << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  if (!parser.Parse(1, new_path)) {
    out << "Missing required new_path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  path = EnsureXFATStylePath(path);
  new_path = EnsureXFATStylePath(new_path);

  SendAndPrintMessage(interface, std::make_shared<Rename>(path, new_path), out);
  return HANDLED;
}

Command::Result CommandReboot::operator()(XBOXInterface& interface,
                                          const std::vector<std::string>& args,
                                          std::ostream& out) {
  uint32_t flags = 0;
  ArgParser parser(args);
  if (parser.ArgExists("wait")) {
    flags |= Reboot::kWait;
  }
  if (parser.ArgExists("warm")) {
    flags |= Reboot::kWarm;
  }
  if (parser.ArgExists("nodebug")) {
    flags |= Reboot::kNoDebug;
  }
  if (parser.ArgExists("stop")) {
    flags |= Reboot::kStop;
  }
  SendAndPrintMessage(interface, std::make_shared<Reboot>(flags), out);
  return HANDLED;
}

Command::Result CommandResume::operator()(XBOXInterface& interface,
                                          const std::vector<std::string>& args,
                                          std::ostream& out) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    out << "Missing required thread_id argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  SendAndPrintMessage(interface, std::make_shared<Resume>(thread_id), out);
  return HANDLED;
}

Command::Result CommandScreenshot::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  auto request = std::make_shared<Screenshot>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
    return HANDLED;
  }

  std::string target_file = "Screenshot_";

  std::time_t result = std::time(nullptr);
  char buf[32] = {0};
  strftime(buf, 31, "%FT%T", std::localtime(&result));
  target_file += buf;

  uint32_t width = request->width;
  uint32_t height = request->height;
  uint32_t bpp = request->pitch / request->width;
  uint32_t format = request->format;

  auto& info = GetTextureFormatInfo(format);
  if (!info.IsValid()) {
    printf(
        "Unsupported screenshot format 0x%X - saving as .raw w:%d h:%d "
        "bytes_per_pixel:%d\n",
        format, width, height, bpp);
    SaveRawFile(target_file, width, height, bpp, format, request->data, out);
    return HANDLED;
  }

  SDL_Surface* surface =
      info.Convert(request->data, request->width, request->height);
  if (!surface) {
    out << "Conversion to PNG failed." << std::endl;
    return HANDLED;
  }

  target_file += ".png";
  if (IMG_SavePNG(surface, target_file.c_str())) {
    out << "Failed to save PNG file " << target_file << std::endl;
  }
  SDL_FreeSurface(surface);

  return HANDLED;
}

Command::Result CommandSetMem::operator()(XBOXInterface& interface,
                                          const std::vector<std::string>& args,
                                          std::ostream& out) {
  ArgParser parser(args);
  uint32_t address;
  if (!parser.Parse(0, address)) {
    out << "Missing required Address argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  if (args.size() < 2) {
    out << "Missing required value string." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  std::string value;
  auto it = args.begin() + 1;
  for (; it != args.end(); ++it) {
    value += *it;
  }

  SendAndPrintMessage(interface, std::make_shared<SetMem>(address, value), out);
  return HANDLED;
}

Command::Result CommandStop::operator()(XBOXInterface& interface,
                                        const std::vector<std::string>& args,
                                        std::ostream& out) {
  SendAndPrintMessage(interface, std::make_shared<Stop>(), out);
  return HANDLED;
}

Command::Result CommandStopOn::operator()(XBOXInterface& interface,
                                          const std::vector<std::string>& args,
                                          std::ostream& out) {
  uint32_t flags = 0;
  ArgParser parser(args);
  if (parser.ArgExists("fce", "exception")) {
    flags |= StopOn::kFirstChanceException;
  }
  if (parser.ArgExists("debugstr")) {
    flags |= StopOn::kDebugStr;
  }
  if (parser.ArgExists("createthread")) {
    flags |= StopOn::kCreateThread;
  }
  if (parser.ArgExists("stacktrace")) {
    flags |= StopOn::kStacktrace;
  }
  if (!flags) {
    flags = StopOn::kAll;
  }
  SendAndPrintMessage(interface, std::make_shared<StopOn>(flags), out);
  return HANDLED;
}

Command::Result CommandSuspend::operator()(XBOXInterface& interface,
                                           const std::vector<std::string>& args,
                                           std::ostream& out) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    out << "Missing required thread_id argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  SendAndPrintMessage(interface, std::make_shared<Suspend>(thread_id), out);
  return HANDLED;
}

Command::Result CommandThreadInfo::operator()(
    XBOXInterface& interface, const std::vector<std::string>& args,
    std::ostream& out) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    out << "Missing required thread_id argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  auto request = std::make_shared<ThreadInfo>(thread_id);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    out << "Suspend count: " << request->suspend_count << std::endl;
    out << "Priority: " << request->priority << std::endl;
    out << "Thread local storage addr: 0x" << std::hex << std::setw(8)
        << std::setfill('0') << request->tls_base << std::endl;
    out << "Start addr: 0x" << std::hex << std::setw(8) << std::setfill('0')
        << request->start << std::endl;
    out << "Base addr: 0x" << std::hex << std::setw(8) << std::setfill('0')
        << request->base << std::endl;
    out << "limit: " << request->limit << std::endl;
  }
  return HANDLED;
}

Command::Result CommandThreads::operator()(XBOXInterface& interface,
                                           const std::vector<std::string>& args,
                                           std::ostream& out) {
  auto request = std::make_shared<Threads>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    for (auto tid : request->threads) {
      out << tid << std::endl;
    }
  }
  return HANDLED;
}

Command::Result CommandWalkMem::operator()(XBOXInterface& interface,
                                           const std::vector<std::string>& args,
                                           std::ostream& out) {
  auto request = std::make_shared<WalkMem>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    for (auto& region : request->regions) {
      out << "Base Address: 0x" << std::hex << std::setw(8) << std::setfill('0')
          << region.start << std::dec << " size: " << region.size
          << " protection: 0x" << std::hex << region.protect;
      for (auto& flag : region.flags) {
        out << " " << flag;
      }
      out << std::endl;
    }
  }
  return HANDLED;
}

Command::Result CommandXBEInfo::operator()(XBOXInterface& interface,
                                           const std::vector<std::string>& args,
                                           std::ostream& out) {
  ArgParser parser(args);
  std::string path;
  std::shared_ptr<XBEInfo> request;
  if (!parser.Parse(0, path)) {
    request = std::make_shared<XBEInfo>();
  } else {
    path = EnsureXFATStylePath(path);
    bool on_disk_only = parser.ArgExists("disk_only", "true");
    request = std::make_shared<XBEInfo>(path, on_disk_only);
  }

  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    out << "Name: " << request->name << " checksum " << std::hex
        << std::setfill('0') << std::setw(8) << request->checksum << std::endl;
  }

  return HANDLED;
}

Command::Result CommandXTLInfo::operator()(XBOXInterface& interface,
                                           const std::vector<std::string>& args,
                                           std::ostream& out) {
  auto request = std::make_shared<XTLInfo>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    out << *request << std::endl;
  } else {
    out << "Last error: " << std::hex << std::setfill('0') << std::setw(8)
        << request->last_err << std::endl;
  }
  return HANDLED;
}
