#include "commands.h"

#include <boost/algorithm/string.hpp>
#include <string>
#include <vector>

#include "util/parsing.h"

static void SendAndPrintMessage(
    XBOXInterface &interface,
    const std::shared_ptr<RDCPProcessedRequest> &request) {
  interface.SendCommandSync(request);
  std::cout << *request << std::endl;
}

struct ArgParser {
  explicit ArgParser(const std::vector<std::string> &args,
                     bool extract_command = false)
      : arguments(args) {
    if (args.empty()) {
      return;
    }

    if (extract_command) {
      command = args.front();
      boost::algorithm::to_lower(command);
    }
  }

  [[nodiscard]] bool HasCommand() const { return !command.empty(); }
  [[nodiscard]] bool ShiftPrefixModifier(char modifier) {
    if (command.empty() || command[0] != modifier) {
      return false;
    }

    command.erase(0);
    return true;
  }

  template <typename T, typename... Ts>
  using are_same_type = std::conjunction<std::is_same<T, Ts>...>;

  template <typename T>
  [[nodiscard]] bool IsCommand(const T &cmd) const {
    return (command == cmd);
  }

  template <typename T, typename... Ts, typename = are_same_type<T, Ts...>>
  [[nodiscard]] bool IsCommand(const T &cmd, const Ts &...rest) const {
    if (command == cmd) {
      return true;
    }

    return IsCommand(rest...);
  }

  template <typename T>
  [[nodiscard]] bool ArgExists(const T &arg) const {
    std::string test(arg);
    return std::find_if(arguments.begin(), arguments.end(),
                        [&test](const auto &v) {
                          return test == boost::algorithm::to_lower_copy(v);
                        }) != arguments.end();
  }

  template <typename T, typename... Ts, typename = are_same_type<T, Ts...>>
  [[nodiscard]] bool ArgExists(const T &arg, const Ts &...rest) const {
    if (ArgExists(arg)) {
      return true;
    }

    return ArgExists(rest...);
  }

  template <
      typename T,
      std::enable_if_t<std::is_integral<T>::value && sizeof(T) == 4, int> = 0>
  bool Parse(int arg_index, T &ret) const {
    if (HasCommand()) {
      ++arg_index;
    }
    if (arg_index >= arguments.size()) {
      return false;
    }
    ret = ParseInt32(arguments[arg_index]);
    return true;
  }

  bool Parse(int arg_index, bool &ret) const {
    if (HasCommand()) {
      ++arg_index;
    }
    if (arg_index >= arguments.size()) {
      return false;
    }

    std::string param = arguments[arg_index];
    boost::algorithm::to_lower(param);

    ret = param == "t" || param == "true" || param == "y" || param == "yes" ||
          param == "on" || param == "1";
    return true;
  }

  bool Parse(int arg_index, std::string &ret) const {
    if (HasCommand()) {
      ++arg_index;
    }
    if (arg_index >= arguments.size()) {
      return false;
    }
    ret = arguments[arg_index];
    return true;
  }

  std::string command;
  const std::vector<std::string> &arguments;
};

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

  if (parser.IsCommand("a", "addr", "address")) {
    uint32_t address;
    if (!parser.Parse(0, address)) {
      std::cout << "Missing required address argument." << std::endl;
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
      std::cout << "Missing required address argument." << std::endl;
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
      std::cout << "Missing required address argument." << std::endl;
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
      std::cout << "Missing required address argument." << std::endl;
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

  auto request = std::make_shared<DirList>(path);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
  } else {
    std::list<const DirList::Entry *> directories;
    std::list<const DirList::Entry *> files;
    for (const auto &entry : request->entries) {
      if (entry.is_directory) {
        directories.push_back(&entry);
      } else {
        files.push_back(&entry);
      }
    }

    auto comparator = [](const DirList::Entry *a, const DirList::Entry *b) {
      return boost::algorithm::to_lower_copy(a->name) <
             boost::algorithm::to_lower_copy(b->name);
    };
    directories.sort(comparator);
    files.sort(comparator);

    for (auto entry : directories) {
      std::cout << "           " << entry->name << std::endl;
    }
    for (auto entry : files) {
      std::cout << std::setw(10) << entry->filesize << " " << entry->name
                << std::endl;
    }
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
    std::cout << "Missing required address argument." << std::endl;
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
    std::cout << "address must be evenly divisible by 8." << std::endl;
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
    std::cout << "Missing required address argument." << std::endl;
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
    std::cout << "Missing required address argument." << std::endl;
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
    std::cout << "Missing required port argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }
  if (port < 1024 || port > 65535) {
    std::cout << "Invalid port argument, must be between 1024 and 65535."
              << std::endl;
    return HANDLED;
  }

  IPAddress address(port);
  if (!interface.StartNotificationListener(address)) {
    std::cout << "Failed to start notification listener on port " << port
              << std::endl;
    return HANDLED;
  }

  bool drop_flag = parser.ArgExists("drop");
  bool debug_flag = parser.ArgExists("debug");
  SendAndPrintMessage(interface,
                      std::make_shared<NotifyAt>(port, drop_flag, debug_flag));
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
    std::cout << "Missing required address argument." << std::endl;
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
      std::cout << "Base address: 0x" << std::hex << std::setw(8)
                << std::setfill('0') << region.base << std::dec
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
