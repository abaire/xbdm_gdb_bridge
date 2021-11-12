#include "commands.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "util/parsing.h"

static void SendAndPrintMessage(
    XBOXInterface &interface,
    const std::shared_ptr<RDCPProcessedRequest> &request) {
  interface.SendCommandSync(request);
  std::cout << *request << std::endl;
}

Command::Result CommandBreak::operator()(XBOXInterface &interface,
                                         const std::vector<std::string> &args) {
  ArgParser parser(args, true);
  if (!parser.HasCommand()) {
    PrintUsage();
    return HANDLED;
  }

  if (parser.IsCommand("start")) {
    SendAndPrintMessage(interface, std::make_shared<BreakAtStart>());
    return HANDLED;
  }

  if (parser.IsCommand("clearall")) {
    SendAndPrintMessage(interface, std::make_shared<BreakClearAll>());
    return HANDLED;
  }

  bool clear = parser.ShiftPrefixModifier('-');

  if (parser.IsCommand("a", "addr", "Address")) {
    uint32_t address;
    if (!parser.Parse(0, address)) {
      std::cout << "Missing required Address argument." << std::endl;
      PrintUsage();
      return HANDLED;
    }
    SendAndPrintMessage(interface,
                        std::make_shared<BreakAddress>(address, clear));
    return HANDLED;
  }

  if (parser.IsCommand("r", "read")) {
    uint32_t address;
    if (!parser.Parse(0, address)) {
      std::cout << "Missing required Address argument." << std::endl;
      PrintUsage();
      return HANDLED;
    }
    int32_t size = 1;
    parser.Parse(1, size);
    SendAndPrintMessage(interface,
                        std::make_shared<BreakOnRead>(address, size, clear));
    return HANDLED;
  }

  if (parser.IsCommand("w", "write")) {
    uint32_t address;
    if (!parser.Parse(0, address)) {
      std::cout << "Missing required Address argument." << std::endl;
      PrintUsage();
      return HANDLED;
    }
    int32_t size = 1;
    parser.Parse(1, size);
    SendAndPrintMessage(interface,
                        std::make_shared<BreakOnWrite>(address, size, clear));
    return HANDLED;
  }

  if (parser.IsCommand("e", "exec", "execute")) {
    uint32_t address;
    if (!parser.Parse(0, address)) {
      std::cout << "Missing required Address argument." << std::endl;
      PrintUsage();
      return HANDLED;
    }
    int32_t size = 1;
    parser.Parse(1, size);
    SendAndPrintMessage(interface,
                        std::make_shared<BreakOnExecute>(address, size, clear));
    return HANDLED;
  }

  std::cout << "Invalid mode " << args.front() << std::endl;
  PrintUsage();
  return HANDLED;
}

Command::Result CommandBye::operator()(XBOXInterface &interface,
                                       const std::vector<std::string> &) {
  SendAndPrintMessage(interface, std::make_shared<Bye>());
  return HANDLED;
}

Command::Result CommandContinue::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    std::cout << "Missing required thread_id argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  bool exception = false;
  parser.Parse(1, exception);

  SendAndPrintMessage(interface,
                      std::make_shared<Continue>(thread_id, exception));
  return HANDLED;
}

Command::Result CommandDebugOptions::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  if (args.empty()) {
    SendAndPrintMessage(interface, std::make_shared<GetDebugOptions>());
    return HANDLED;
  }

  ArgParser parser(args);
  bool enable_crashdump = parser.ArgExists("c", "crashdump");
  bool enable_dcptrace = parser.ArgExists("d", "dpctrace");
  SendAndPrintMessage(interface, std::make_shared<SetDebugOptions>(
                                     enable_crashdump, enable_dcptrace));
  return HANDLED;
}

Command::Result CommandDebugger::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  bool disable = parser.ArgExists("d", "disable", "off");
  SendAndPrintMessage(interface, std::make_shared<Debugger>(disable));
  return HANDLED;
}

Command::Result CommandDebugMode::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  SendAndPrintMessage(interface, std::make_shared<DebugMode>());
  return HANDLED;
}

Command::Result CommandDelete::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    std::cout << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  bool recursive = parser.ArgExists("-r");
  SendAndPrintMessage(interface, std::make_shared<Delete>(path, recursive));
  return HANDLED;
}

static bool FetchDirectoryEntires(XBOXInterface &interface,
                                  const std::string &path,
                                  std::list<DirList::Entry> &directories,
                                  std::list<DirList::Entry> &files) {
  directories.clear();
  files.clear();

  auto request = std::make_shared<DirList>(path);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
    return false;
  }

  for (const auto &entry : request->entries) {
    if (entry.is_directory) {
      directories.push_back(entry);
    } else {
      files.push_back(entry);
    }
  }

  return true;
}

Command::Result CommandDirList::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    std::cout << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  // Prevent access denied errors when trying to list the base of a drive path.
  if (path.back() == ':') {
    path.push_back('\\');
  }

  std::list<DirList::Entry> directories;
  std::list<DirList::Entry> files;
  if (!FetchDirectoryEntires(interface, path, directories, files)) {
    return HANDLED;
  }
  auto comparator = [](const DirList::Entry &a, const DirList::Entry &b) {
    return boost::algorithm::to_lower_copy(a.name) <
           boost::algorithm::to_lower_copy(b.name);
  };
  directories.sort(comparator);
  files.sort(comparator);

  for (auto &entry : directories) {
    std::cout << "           " << entry.name << "\\" << std::endl;
  }
  for (auto &entry : files) {
    std::cout << std::setw(10) << entry.filesize << " " << entry.name
              << std::endl;
  }

  return HANDLED;
}

Command::Result CommandDebugMonitorVersion::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  auto request = std::make_shared<DebugMonitorVersion>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    std::cout << request->version << std::endl;
  }
  return HANDLED;
}

Command::Result CommandDriveFreeSpace::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  std::string drive_letter;
  if (!parser.Parse(0, drive_letter)) {
    std::cout << "Missing required drive_letter argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  auto request = std::make_shared<DriveFreeSpace>(drive_letter);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    std::cout << "total: " << request->total_bytes
              << " total free: " << request->free_bytes
              << " free to caller: " << request->free_to_caller << std::endl;
  }
  return HANDLED;
}

Command::Result CommandDriveList::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  auto request = std::make_shared<DriveList>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    for (auto &letter : request->drives) {
      std::cout << letter << std::endl;
    }
  }

  return HANDLED;
}

Command::Result CommandGetChecksum::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  int address;
  int length;
  int block_size;
  if (!parser.Parse(0, address)) {
    std::cout << "Missing required Address argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  if (!parser.Parse(1, length)) {
    std::cout << "Missing required length argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  if (!parser.Parse(2, block_size)) {
    std::cout << "Missing required block_size argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  if (address & 0x07) {
    std::cout << "Address must be evenly divisible by 8." << std::endl;
    return HANDLED;
  }
  if (length & 0x07) {
    std::cout << "length must be evenly divisible by 8." << std::endl;
    return HANDLED;
  }
  if (block_size & 0x07) {
    std::cout << "block_size must be evenly divisible by 8." << std::endl;
    return HANDLED;
  }

  auto request = std::make_shared<GetChecksum>(address, length, block_size);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    for (auto &checksum : request->checksums) {
      std::cout << std::hex << std::setfill('0') << std::setw(8) << checksum
                << std::endl;
    }
  }

  return Command::HANDLED;
}

Command::Result CommandGetContext::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    std::cout << "Missing required thread_id argument." << std::endl;
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
    std::cout << *request << std::endl;
  } else {
    std::cout << request->context << std::endl;
  }
  return HANDLED;
}

Command::Result CommandGetExtContext::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    std::cout << "Missing required thread_id argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  auto request = std::make_shared<GetExtContext>(thread_id);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    std::cout << request->context << std::endl;
  }
  return HANDLED;
}

static bool SaveFile(XBOXInterface &interface, const std::string &remote,
                     const std::filesystem::path &local) {
  auto request = std::make_shared<GetFile>(remote);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
    return false;
  }

  std::ofstream of(local, std::ofstream::binary | std::ofstream::trunc);
  if (!of) {
    std::cout << "Failed to create local file " << local << std::endl;
    return false;
  }
  of.write(reinterpret_cast<char *>(request->data.data()),
           static_cast<std::streamsize>(request->data.size()));
  of.close();

  return of.good();
}

static bool SaveDirectory(XBOXInterface &interface, const std::string &remote,
                          const std::filesystem::path &local) {
  std::list<DirList::Entry> directories;
  std::list<DirList::Entry> files;
  if (!FetchDirectoryEntires(interface, remote, directories, files)) {
    return false;
  }

  std::string remote_dir = remote;
  if (remote_dir.back() != '\\') {
    remote_dir += "\\";
  }

  for (auto &dir : directories) {
    std::string remote_path = remote_dir + dir.name;
    std::filesystem::path local_path = local / dir.name;
    std::error_code err;
    std::filesystem::create_directories(local_path, err);

    if (!SaveDirectory(interface, remote_path, local_path)) {
      return false;
    }
  }

  for (auto &file : files) {
    std::string remote_path = remote_dir + file.name;
    std::filesystem::path local_path = local / file.name;
    if (!SaveFile(interface, remote_path, local_path)) {
      return false;
    }
    std::cout << remote_path << " -> " << local_path << std::endl;
  }

  return true;
}

bool CheckRemoteFile(XBOXInterface &interface, const std::string &path,
                     bool &exists, bool &is_dir) {
  auto request = std::make_shared<GetFileAttributes>(path);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
    return false;
  }

  exists = request->exists;
  is_dir = request->flags.find("directory") != request->flags.end();
  return true;
}

Command::Result CommandGetFile::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    std::cout << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  bool exists;
  bool is_directory;
  if (!CheckRemoteFile(interface, path, exists, is_directory)) {
    return HANDLED;
  }

  if (!exists) {
    std::cout << "No such file." << std::endl;
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
    std::cout << "Recursively fetching files from " << path << std::endl;
    std::error_code err;
    std::filesystem::create_directories(local_path, err);
    SaveDirectory(interface, path, local_path);
  } else {
    std::filesystem::path parent_dir = local_path.parent_path();
    if (!parent_dir.empty()) {
      std::error_code err;
      std::filesystem::create_directories(parent_dir, err);
    }
    if (SaveFile(interface, path, local_path)) {
      std::cout << path << " -> " << local_path << std::endl;
    }
  }

  return HANDLED;
}

Command::Result CommandGetFileAttributes::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    std::cout << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  auto request = std::make_shared<GetFileAttributes>(path);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    std::cout << "Size: " << request->filesize << " ";
    for (auto &f : request->flags) {
      std::cout << f << " ";
    }
    std::cout << std::endl;
  }
  return HANDLED;
}

Command::Result CommandGetMem::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  uint32_t address;
  uint32_t size;
  if (!parser.Parse(0, address)) {
    std::cout << "Missing required Address argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  if (!parser.Parse(1, size)) {
    std::cout << "Missing required size argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  auto request = std::make_shared<GetMemBinary>(address, size);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    int count = 0;
    for (auto &val : request->data) {
      std::cout << std::hex << val << " ";
      if (count && (count % 32) == 0) {
        std::cout << std::endl;
      }
      ++count;
    }
    if (((count - 1) % 32) != 0) {
      std::cout << std::endl;
    }
  }

  return HANDLED;
}

Command::Result CommandGetProcessID::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  auto request = std::make_shared<GetProcessID>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    std::cout << std::hex << std::setfill('0') << std::setw(8)
              << request->process_id << std::endl;
  }
  return HANDLED;
}

Command::Result CommandGetUtilityDriveInfo::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  auto request = std::make_shared<GetUtilityDriveInfo>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    for (auto &partition : request->partitions) {
      std::cout << partition.first << ": 0x" << std::hex << std::setfill('0')
                << std::setw(8) << partition.second << std::endl;
    }
  }
  return HANDLED;
}

Command::Result CommandGo::operator()(XBOXInterface &interface,
                                      const std::vector<std::string> &args) {
  SendAndPrintMessage(interface, std::make_shared<Go>());
  return HANDLED;
}

Command::Result CommandHalt::operator()(XBOXInterface &interface,
                                        const std::vector<std::string> &args) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    SendAndPrintMessage(interface, std::make_shared<Halt>());
  } else {
    SendAndPrintMessage(interface, std::make_shared<Halt>(thread_id));
  }

  return HANDLED;
}

Command::Result CommandIsBreak::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  int address;
  if (!parser.Parse(0, address)) {
    std::cout << "Missing required Address argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  auto request = std::make_shared<IsBreak>(address);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    switch (request->type) {
      case IsBreak::Type::NONE:
        std::cout << "No breakpoint" << std::endl;
        break;

      case IsBreak::Type::WRITE:
        std::cout << "Write" << std::endl;
        break;

      case IsBreak::Type::READ_OR_WRITE:
        std::cout << "Read/Write" << std::endl;
        break;

      case IsBreak::Type::EXECUTE:
        std::cout << "Execute" << std::endl;
        break;

      case IsBreak::Type::ADDRESS:
        std::cout << "Previously set breakpoint" << std::endl;
        break;
    }
  }
  return HANDLED;
}

Command::Result CommandIsDebugger::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  auto request = std::make_shared<IsDebugger>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    std::cout << request->attached << std::endl;
  }
  return HANDLED;
}

Command::Result CommandIsStopped::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    std::cout << "Missing required thread_id argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  auto request = std::make_shared<IsStopped>(thread_id);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    if (!request->stopped) {
      std::cout << "Not stopped." << std::endl;
    } else {
      std::cout << *request->stop_reason << std::endl;
    }
  }

  return HANDLED;
}

Command::Result CommandMagicBoot::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    std::cout << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  bool nodebug = parser.ArgExists("nodebug");
  bool cold = parser.ArgExists("cold");
  SendAndPrintMessage(interface,
                      std::make_shared<MagicBoot>(path, !nodebug, cold));
  return HANDLED;
}

Command::Result CommandMemoryMap::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  auto request = std::make_shared<MemoryMapGlobal>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    std::cout << std::setfill('0') << std::hex << std::setw(8)
              << "MmHighestPhysicalPage: 0x"
              << request->mm_highest_physical_page << " RetailPfnRegion: 0x"
              << request->retail_pfn_region << " SystemPteRange: 0x"
              << request->system_pte_range << " AvailablePages: " << std::dec
              << request->available_pages
              << " AllocatedPagesByUsage: " << request->allocated_pages_by_usage
              << " PfnDatabase: 0x" << std::hex << request->pfn_database
              << " AddressSpaceLock: " << std::dec
              << request->address_space_lock << " VadRoot: 0x" << std::hex
              << request->vad_root << " VadHint: 0x" << request->vad_hint
              << " VadFreeHint: 0x" << request->vad_free_hint
              << " MmNumberOfPhysicalPages: " << std::dec
              << request->mm_number_of_physical_pages
              << " MmAvailablePages: " << request->mm_available_pages
              << std::endl;
  }

  return HANDLED;
}

Command::Result CommandMakeDirectory::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    std::cout << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  SendAndPrintMessage(interface, std::make_shared<Mkdir>(path));
  return HANDLED;
}

Command::Result CommandModuleSections::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  std::string path;
  if (!parser.Parse(0, path)) {
    std::cout << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  auto request = std::make_shared<ModSections>(path);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    for (auto &m : request->sections) {
      std::cout << m << std::endl;
    }
  }
  return HANDLED;
}

Command::Result CommandModules::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  auto request = std::make_shared<Modules>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    for (auto &m : request->modules) {
      std::cout << m << std::endl;
    }
  }
  return HANDLED;
}

Command::Result CommandNoStopOn::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
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
  SendAndPrintMessage(interface, std::make_shared<NoStopOn>(flags));
  return HANDLED;
}

Command::Result CommandNotifyAt::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  uint32_t port;
  if (!parser.Parse(0, port)) {
    std::cout << "Missing required Port argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  if (port < 1024 || port > 65535) {
    std::cout << "Invalid Port argument, must be between 1024 and 65535."
              << std::endl;
    return HANDLED;
  }

  IPAddress address(port);
  if (!interface.StartNotificationListener(address)) {
    std::cout << "Failed to start notification listener on Port " << port
              << std::endl;
    return HANDLED;
  }

  bool drop_flag = parser.ArgExists("drop");
  bool debug_flag = parser.ArgExists("debug");
  SendAndPrintMessage(interface,
                      std::make_shared<NotifyAt>(port, drop_flag, debug_flag));

  if (!drop_flag) {
    interface.AttachDebugNotificationHandler();
  } else {
    interface.DetachDebugNotificationHandler();
  }
  return HANDLED;
}

static bool UploadFile(XBOXInterface &interface, const std::string &local_path,
                       const std::string &remote_path, bool overwrite) {
  bool exists;
  bool is_dir;
  if (!CheckRemoteFile(interface, remote_path, exists, is_dir)) {
    return false;
  }

  std::string full_remote_path = remote_path;
  if (is_dir) {
    if (full_remote_path.back() != '\\') {
      full_remote_path += "\\";
    }
    full_remote_path += std::filesystem::path(local_path).filename();

    if (!CheckRemoteFile(interface, full_remote_path, exists, is_dir)) {
      return false;
    }
  }

  if (exists && !is_dir && !overwrite) {
    std::cout << "Remote file '" << remote_path
              << "' already exists, skipping..." << std::endl;
    return true;
  }

  std::ifstream ifs(local_path, std::ifstream::binary);
  if (!ifs) {
    std::cout << "Failed to open '" << local_path << "' for reading."
              << std::endl;
    return false;
  }
  std::istreambuf_iterator<char> ifs_begin(ifs);
  std::istreambuf_iterator<char> ifs_end;
  std::vector<uint8_t> data(ifs_begin, ifs_end);
  ifs.close();

  std::cout << local_path << " => " << full_remote_path << " ... "
            << std::flush;

  auto request = std::make_shared<SendFile>(full_remote_path, data);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << "Failed" << std::endl;
    std::cout << *request << std::endl;
    return false;
  }
  std::cout << "OK" << std::endl;

  return true;
}

static bool UploadDirectory(XBOXInterface &interface,
                            const std::string &local_path,
                            const std::string &remote_path, bool overwrite) {
  bool exists;
  bool is_dir;
  if (!CheckRemoteFile(interface, remote_path, exists, is_dir)) {
    return false;
  }
  if (exists && !is_dir) {
    std::cout << "Remote path '" << remote_path
              << "' exists and is a file. Aborting." << std::endl;
    return false;
  }

  if (!exists) {
    auto request = std::make_shared<Mkdir>(remote_path);
    interface.SendCommandSync(request);
    if (!request->IsOK()) {
      std::cout << *request << std::endl;
      return false;
    }
  }

  std::string full_remote_path = remote_path;
  if (full_remote_path.back() != '\\') {
    full_remote_path += "\\";
  }
  full_remote_path += std::filesystem::path(local_path).filename();
  full_remote_path += "\\";

  for (auto const &dir_entry :
       std::filesystem::directory_iterator{local_path}) {
    if (dir_entry.is_regular_file()) {
      if (!UploadFile(interface, dir_entry.path(), full_remote_path,
                      overwrite)) {
        return false;
      }
    } else if (dir_entry.is_directory()) {
      if (!UploadDirectory(interface, dir_entry.path(), full_remote_path,
                           overwrite)) {
        return false;
      }
    }
  }

  return true;
}

Command::Result CommandPutFile::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  std::string local_path;
  if (!parser.Parse(0, local_path)) {
    std::cout << "Missing required local_path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  std::string remote_path;
  if (!parser.Parse(1, remote_path)) {
    std::cout << "Missing required remote_path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  bool is_directory = std::filesystem::is_directory(local_path);
  if (!is_directory && !std::filesystem::is_regular_file(local_path)) {
    std::cout << "Invalid local_path, must be a regular file or a directory."
              << std::endl;
    return HANDLED;
  }

  bool allow_overwrite = parser.ArgExists("allow_overwrite", "overwrite", "-f");

  if (is_directory) {
    UploadDirectory(interface, local_path, remote_path, allow_overwrite);
  } else {
    UploadFile(interface, local_path, remote_path, allow_overwrite);
  }

  return HANDLED;
}

Command::Result CommandRename::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  std::string path;
  std::string new_path;
  if (!parser.Parse(0, path)) {
    std::cout << "Missing required path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  if (!parser.Parse(1, path)) {
    std::cout << "Missing required new_path argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  SendAndPrintMessage(interface, std::make_shared<Rename>(path, new_path));
  return HANDLED;
}

Command::Result CommandReboot::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
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
  SendAndPrintMessage(interface, std::make_shared<Reboot>(flags));
  return HANDLED;
}

Command::Result CommandResume::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    std::cout << "Missing required thread_id argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  SendAndPrintMessage(interface, std::make_shared<Resume>(thread_id));
  return HANDLED;
}

Command::Result CommandSetMem::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  uint32_t address;
  if (!parser.Parse(0, address)) {
    std::cout << "Missing required Address argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  if (args.size() < 2) {
    std::cout << "Missing required value string." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  std::string value;
  auto it = args.begin() + 1;
  for (; it != args.end(); ++it) {
    value += *it;
  }

  SendAndPrintMessage(interface, std::make_shared<SetMem>(address, value));
  return HANDLED;
}

Command::Result CommandStop::operator()(XBOXInterface &interface,
                                        const std::vector<std::string> &args) {
  SendAndPrintMessage(interface, std::make_shared<Stop>());
  return HANDLED;
}

Command::Result CommandStopOn::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
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
  SendAndPrintMessage(interface, std::make_shared<StopOn>(flags));
  return HANDLED;
}

Command::Result CommandSuspend::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    std::cout << "Missing required thread_id argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  SendAndPrintMessage(interface, std::make_shared<Suspend>(thread_id));
  return HANDLED;
}

Command::Result CommandThreadInfo::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  int thread_id;
  if (!parser.Parse(0, thread_id)) {
    std::cout << "Missing required thread_id argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  auto request = std::make_shared<ThreadInfo>(thread_id);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    std::cout << "Suspend count: " << request->suspend_count << std::endl;
    std::cout << "Priority: " << request->priority << std::endl;
    std::cout << "Thread local storage addr: 0x" << std::hex << std::setw(8)
              << std::setfill('0') << request->tls_base << std::endl;
    std::cout << "Start addr: 0x" << std::hex << std::setw(8)
              << std::setfill('0') << request->start << std::endl;
    std::cout << "Base addr: 0x" << std::hex << std::setw(8)
              << std::setfill('0') << request->base << std::endl;
    std::cout << "limit: " << request->limit << std::endl;
  }
  return HANDLED;
}

Command::Result CommandThreads::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  auto request = std::make_shared<Threads>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    for (auto tid : request->threads) {
      std::cout << tid << std::endl;
    }
  }
  return HANDLED;
}

Command::Result CommandWalkMem::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  auto request = std::make_shared<WalkMem>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    for (auto &region : request->regions) {
      std::cout << "Base Address: 0x" << std::hex << std::setw(8)
                << std::setfill('0') << region.start << std::dec
                << " size: " << region.size << " protection: 0x" << std::hex
                << region.protect;
      for (auto &flag : region.flags) {
        std::cout << " " << flag;
      }
      std::cout << std::endl;
    }
  }
  return HANDLED;
}

Command::Result CommandXBEInfo::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  ArgParser parser(args);
  std::string path;
  std::shared_ptr<XBEInfo> request;
  if (!parser.Parse(0, path)) {
    request = std::make_shared<XBEInfo>();
  } else {
    bool on_disk_only = parser.ArgExists("disk_only", "true");
    request = std::make_shared<XBEInfo>(path, on_disk_only);
  }

  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    std::cout << "Name: " << request->name << " checksum " << std::hex
              << std::setfill('0') << std::setw(8) << request->checksum
              << std::endl;
  }

  return HANDLED;
}

Command::Result CommandXTLInfo::operator()(
    XBOXInterface &interface, const std::vector<std::string> &args) {
  auto request = std::make_shared<XTLInfo>();
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    std::cout << "Last error: " << std::hex << std::setfill('0') << std::setw(8)
              << request->last_err << std::endl;
  }
  return HANDLED;
}
