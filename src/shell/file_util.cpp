#include "file_util.h"

#include <fstream>

//! Value below which setfileattributes timestamp modifications will always
//! fail. Sunday, August 6, 2000 11:42:36 PM
static constexpr uint64_t kMinTimestamp = 0x01c0000000000000ULL;

//! Value above which setfileattributes timestamp modifications will always
//! fail. Monday, October 8, 2114 11:37:38 PM
static constexpr uint64_t kMaxTimestamp = 0x023fffffffffffffULL;

static constexpr uint64_t kTimeRange = kMaxTimestamp - kMinTimestamp;

static constexpr uint64_t kUsableTimestampRange = 0xFFFFFFFFF0000000ULL;

static std::string EnsureTrailingBackslash(const std::string& dir_path) {
  if (dir_path.back() != '\\') {
    return dir_path + '\\';
  }

  return dir_path;
}

std::string EnsureXFATStylePath(const std::string& path) {
  auto safe_path = path;
  std::replace(safe_path.begin(), safe_path.end(), '/', '\\');
  return std::move(safe_path);
}

static std::string XFATPathToLocalPath(const std::string& relative_path) {
  std::string ret = relative_path;
  if (std::filesystem::path::preferred_separator != '\\') {
    std::replace(ret.begin(), ret.end(), '\\',
                 std::filesystem::path::preferred_separator);
  }
  return std::move(ret);
}

static uint64_t SafeXFATTimestampForFile(const std::string& local_path) {
  auto last_write_time =
      std::filesystem::last_write_time(local_path).time_since_epoch();

  // NT timestamps are in 100-nanosecond units since 1601, but the Xbox has
  // several additional limitations: 1) it performs a range check on the
  // timestamp 2) it never sets the timestamp to exactly the requested value,
  // the low dword is always slightly higher than requested
  //    (within about 2 seconds of the request).
  // To work around this, seconds since the epoch are used and then shifted into
  // the valid range. The mod times won't be correct, but they'll be
  // deterministic and comparable to future modifications from the same source
  // machine.

  // XFAT never writes the low byte and adds around 0x1050000 to the requested
  // value. The actual value is shifted out of this range so the
  // nondeterministic bits can be ignored when comparing.
  uint64_t change_timestamp =
      std::chrono::duration_cast<std::chrono::seconds>(last_write_time).count()
      << 28;
  if (change_timestamp < kMinTimestamp) {
    change_timestamp += kMinTimestamp;
  }
  while (change_timestamp > kMaxTimestamp) {
    change_timestamp -= kTimeRange;
  }
  change_timestamp &= kUsableTimestampRange;

  return change_timestamp;
}

bool CheckRemotePath(XBOXInterface& interface, const std::string& path,
                     bool& exists, bool& is_dir) {
  uint64_t size;
  uint64_t create;
  uint64_t change;

  return CheckRemotePath(interface, path, exists, is_dir, size, create, change);
}

bool CheckRemotePath(XBOXInterface& interface, const std::string& path,
                     bool& exists, bool& is_dir, uint64_t& size,
                     uint64_t& create_timestamp, uint64_t& change_timestamp) {
  auto request = std::make_shared<GetFileAttributes>(path);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
    return false;
  }

  exists = request->exists;
  is_dir = request->flags.find("directory") != request->flags.end();
  size = request->filesize;
  create_timestamp = request->create_timestamp;
  change_timestamp = request->change_timestamp;
  return true;
}

bool FetchDirectoryEntries(XBOXInterface& interface, const std::string& path,
                           std::list<DirList::Entry>& directories,
                           std::list<DirList::Entry>& files) {
  directories.clear();
  files.clear();

  auto request = std::make_shared<DirList>(EnsureXFATStylePath(path));
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << *request << std::endl;
    return false;
  }

  for (const auto& entry : request->entries) {
    if (entry.is_directory) {
      directories.push_back(entry);
    } else {
      files.push_back(entry);
    }
  }

  return true;
}

//! Walks a remote directory, invoking the given callbacks on each file and
//! directory discovered.
//!
//! The ProcessFileCallback accepts a std::string indicating the relative path
//! from the remote root to the directory containing the file and a
//! DirList::Entry describing the file. It may return `false` to abort the walk.
//!
//! The ProcessDirectoryCallback accepts a std::string filename and a reference
//! to a bool. The bool may be set to `true` to skip further processing of the
//! directory. The callback may return `false` to abort the walk.
template <typename ProcessFileCallback, typename ProcessDirectoryCallback>
static bool WalkRemoteDir(XBOXInterface& interface,
                          const std::string& remote_directory,
                          ProcessFileCallback&& process_file,
                          ProcessDirectoryCallback&& process_dir) {
  std::string full_remote_directory = EnsureTrailingBackslash(remote_directory);
  bool remote_exists;
  bool remote_is_dir;
  if (!CheckRemotePath(interface, full_remote_directory, remote_exists,
                       remote_is_dir)) {
    return false;
  }
  if (remote_exists && !remote_is_dir) {
    std::cout << "Remote path '" << remote_directory << "' is a file."
              << std::endl;
    return false;
  }

  if (!remote_exists) {
    return true;
  }

  std::deque directories = {full_remote_directory};

  while (!directories.empty()) {
    const std::string& dir = directories.front();

    std::list<DirList::Entry> subdirectories;
    std::list<DirList::Entry> files;
    if (!FetchDirectoryEntries(interface, dir, subdirectories, files)) {
      return false;
    }

    bool should_skip = false;
    if (!process_dir(dir, should_skip)) {
      return false;
    }
    if (!should_skip) {
      for (auto& subdir : subdirectories) {
        directories.emplace_back(EnsureTrailingBackslash(dir) + subdir.name);
      }

      auto relative_path = dir.substr(full_remote_directory.size());
      for (auto& file : files) {
        if (!process_file(relative_path, file)) {
          return false;
        }
      }
    }

    directories.pop_front();
  }

  return true;
}

bool DeleteRecursively(XBOXInterface& interface, const std::string& path) {
  std::list<DirList::Entry> directories;
  std::list<DirList::Entry> files;
  if (!FetchDirectoryEntries(interface, path, directories, files)) {
    return false;
  }

  std::string root_path = EnsureTrailingBackslash(path);

  for (auto& file : files) {
    std::string full_path = root_path + file.name;
    auto request = std::make_shared<Delete>(full_path, false);
    interface.SendCommandSync(request);
    if (!request->IsOK()) {
      std::cout << *request << std::endl;
      return false;
    }

    std::cout << "rm " << full_path << std::endl;
  }

  for (auto& dir : directories) {
    std::string full_path = root_path + dir.name;
    if (!DeleteRecursively(interface, full_path)) {
      return false;
    }
  }

  {
    auto request = std::make_shared<Delete>(path, true);
    interface.SendCommandSync(request);
    if (!request->IsOK()) {
      std::cout << *request << std::endl;
      return false;
    }

    std::cout << "rm " << path << std::endl;
  }

  return true;
}

bool SaveFile(XBOXInterface& interface, const std::string& remote,
              const std::filesystem::path& local) {
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
  of.write(reinterpret_cast<char*>(request->data.data()),
           static_cast<std::streamsize>(request->data.size()));
  of.close();

  return of.good();
}

bool SaveDirectory(XBOXInterface& interface, const std::string& remote,
                   const std::filesystem::path& local) {
  std::list<DirList::Entry> directories;
  std::list<DirList::Entry> files;
  if (!FetchDirectoryEntries(interface, remote, directories, files)) {
    return false;
  }

  std::string remote_dir = EnsureTrailingBackslash(remote);

  for (auto& dir : directories) {
    std::string remote_path = remote_dir + dir.name;
    std::filesystem::path local_path = local / dir.name;
    std::error_code err;
    std::filesystem::create_directories(local_path, err);

    if (!SaveDirectory(interface, remote_path, local_path)) {
      return false;
    }
  }

  for (auto& file : files) {
    std::string remote_path = remote_dir + file.name;
    std::filesystem::path local_path = local / file.name;
    if (!SaveFile(interface, remote_path, local_path)) {
      return false;
    }
    std::cout << remote_path << " -> " << local_path << std::endl;
  }

  return true;
}

bool SaveRawFile(const std::string& filename_root, uint32_t width,
                 uint32_t height, uint32_t bpp, uint32_t format,
                 const std::vector<uint8_t>& data) {
  char buf[256] = {0};
  snprintf(buf, 255, "%s-w%d_h%d_bpp%d_fmt%d.bin", filename_root.c_str(), width,
           height, bpp, format);

  // TODO: Check to see if the file already exists and dedup.
  std::ofstream of(buf, std::ofstream::binary | std::ofstream::trunc);
  if (!of) {
    std::cout << "Failed to create local file " << buf << std::endl;
    return false;
  }

  of.write(reinterpret_cast<const char*>(data.data()),
           static_cast<std::streamsize>(data.size()));
  of.close();

  if (!of.good()) {
    std::cout << "Failed to save local file " << buf << std::endl;
  }
  return of.good();
}

bool UploadFileWithoutChecking(XBOXInterface& interface,
                               const std::string& local_path,
                               const std::string& full_remote_path,
                               bool set_timestamp) {
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

  auto safe_full_remote_path = EnsureXFATStylePath(full_remote_path);
  std::cout << local_path << " => " << safe_full_remote_path << " ... "
            << std::flush;

  auto request = std::make_shared<SendFile>(safe_full_remote_path, data);
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    std::cout << "Failed" << std::endl;
    std::cout << *request << std::endl;
    return false;
  }

  std::cout << "OK" << std::endl;

  if (set_timestamp) {
    auto change_timestamp = SafeXFATTimestampForFile(local_path);
    auto update_request = std::make_shared<SetFileAttributes>(
        safe_full_remote_path, std::nullopt, std::nullopt, change_timestamp,
        change_timestamp);
    interface.SendCommandSync(update_request);
    if (!update_request->IsOK()) {
      std::cout << "Failed to update timestamp after uploading file '"
                << safe_full_remote_path << "': " << *update_request
                << std::endl;
      return false;
    }
  }

  return true;
}

bool UploadFile(XBOXInterface& interface, const std::string& local_path,
                const std::string& remote_path,
                UploadFileOverwriteAction overwrite_action) {
  auto safe_remote_path = EnsureXFATStylePath(remote_path);

  bool exists;
  bool is_dir;
  if (!CheckRemotePath(interface, safe_remote_path, exists, is_dir)) {
    return false;
  }

  if (!exists && safe_remote_path.back() == '\\') {
    auto request = std::make_shared<Mkdir>(safe_remote_path);
    interface.SendCommandSync(request);
    if (!request->IsOK()) {
      std::cout << *request << std::endl;
      return false;
    }
    is_dir = true;
  }

  std::string full_remote_path = safe_remote_path;
  if (is_dir) {
    if (full_remote_path.back() != '\\') {
      full_remote_path += "\\";
    }
    full_remote_path += std::filesystem::path(local_path).filename();

    if (!CheckRemotePath(interface, full_remote_path, exists, is_dir)) {
      return false;
    }
  }

  if (exists && !is_dir) {
    switch (overwrite_action) {
      case UploadFileOverwriteAction::OVERWRITE:
        break;

      case UploadFileOverwriteAction::SKIP:
        std::cout << "Remote file '" << remote_path
                  << "' already exists, skipping..." << std::endl;
        return true;

      case UploadFileOverwriteAction::ABORT:
        std::cout << "Remote file '" << remote_path
                  << "' already exists, aborting..." << std::endl;
        return false;
    }
  }

  return UploadFileWithoutChecking(interface, local_path, full_remote_path);
}

template <typename ProcessFileCallback>
static bool WalkDirectory(const std::string& root_path,
                          ProcessFileCallback&& cb) {
  std::deque directories{root_path};

  while (!directories.empty()) {
    const std::string& local_path = directories.front();

    for (auto const& dir_entry :
         std::filesystem::directory_iterator{local_path}) {
      if (dir_entry.is_regular_file()) {
        if (!cb(dir_entry.path())) {
          return false;
        }
      } else if (dir_entry.is_directory()) {
        directories.push_back(dir_entry.path());
      }
    }
    directories.pop_front();
  }
  return true;
}

bool UploadDirectory(XBOXInterface& interface, const std::string& local_path,
                     const std::string& remote_path,
                     UploadFileOverwriteAction overwrite_action,
                     bool contents_only) {
  auto safe_remote_path = EnsureXFATStylePath(remote_path);

  bool exists;
  bool is_dir;
  if (!CheckRemotePath(interface, safe_remote_path, exists, is_dir)) {
    return false;
  }
  if (exists && !is_dir) {
    std::cout << "Remote path '" << safe_remote_path
              << "' exists and is a file. Aborting." << std::endl;
    return false;
  }

  if (!exists) {
    auto request = std::make_shared<Mkdir>(safe_remote_path);
    interface.SendCommandSync(request);
    if (!request->IsOK()) {
      std::cout << *request << std::endl;
      return false;
    }
  }

  std::string full_remote_path = EnsureTrailingBackslash(safe_remote_path);
  if (!contents_only) {
    full_remote_path += std::filesystem::path(local_path).filename();
    full_remote_path = EnsureTrailingBackslash(full_remote_path);
  }

  auto process_file = [&interface, &full_remote_path,
                       overwrite_action](const std::string& local_file) {
    return UploadFile(interface, local_file, full_remote_path,
                      overwrite_action);
  };

  return WalkDirectory(local_path, process_file);
}

bool SyncFile(XBOXInterface& interface, const std::string& local_path,
              const std::string& remote_path) {
  std::string full_remote_path = remote_path;

  bool remote_exists;
  bool remote_is_dir;
  uint64_t remote_filesize;
  uint64_t remote_create_timestamp;
  uint64_t remote_change_timestamp;
  if (!CheckRemotePath(interface, full_remote_path, remote_exists,
                       remote_is_dir, remote_filesize, remote_create_timestamp,
                       remote_change_timestamp)) {
    return false;
  }

  if (remote_is_dir) {
    if (full_remote_path.back() != '\\') {
      full_remote_path += "\\";
    }

    full_remote_path += std::filesystem::path(local_path).filename();
    if (!CheckRemotePath(interface, full_remote_path, remote_exists,
                         remote_is_dir, remote_filesize,
                         remote_create_timestamp, remote_change_timestamp)) {
      return false;
    }
  }

  auto upload_file = [&interface, &local_path, &full_remote_path]() {
    if (!UploadFileWithoutChecking(interface, local_path, full_remote_path,
                                   true)) {
      std::cout << "Failed to upload file." << std::endl;
      return false;
    }
    return true;
  };

  if (!remote_exists) {
    return upload_file();
  }

  auto change_timestamp = SafeXFATTimestampForFile(local_path);
  remote_change_timestamp &= kUsableTimestampRange;
  if (change_timestamp == remote_change_timestamp &&
      remote_filesize == std::filesystem::file_size(local_path)) {
    std::cout << "Skipping '" << local_path << "' with same modification time."
              << std::endl;
    return true;
  }

  return upload_file();
}

static bool MakeDirs(XBOXInterface& interface,
                     const std::string& remote_directory) {
  auto safe_remote_path = EnsureXFATStylePath(remote_directory);
  bool remote_exists;
  bool remote_is_dir;
  if (!CheckRemotePath(interface, safe_remote_path, remote_exists,
                       remote_is_dir)) {
    return false;
  }

  if (remote_exists && !remote_is_dir) {
    std::cout << "Remote path '" << safe_remote_path << "' is a file."
              << std::endl;
    return false;
  }

  if (remote_exists) {
    return true;
  }

  size_t end = safe_remote_path.find('\\');
  while (end != std::string::npos) {
    auto subpath = safe_remote_path.substr(0, end);
    if (subpath.back() != ':') {
      auto request = std::make_shared<Mkdir>(subpath);
      interface.SendCommandSync(request);
      if (!request->IsOK() && request->status != ERR_EXISTS) {
        std::cout << *request << std::endl;
        return false;
      }
    }

    end = safe_remote_path.find('\\', end + 1);
  }
  return true;
}

static bool CreateRemoteDirectoryHierarchy(
    XBOXInterface& interface, const std::string& remote_directory,
    const std::set<std::string>& hierarchy) {
  std::string full_remote_dir =
      EnsureTrailingBackslash(EnsureXFATStylePath(remote_directory));

  if (!MakeDirs(interface, full_remote_dir)) {
    return false;
  }

  for (auto& populated_dir : hierarchy) {
    if (populated_dir.empty() || populated_dir == ".") {
      continue;
    }

    std::string full_remote_subdir =
        EnsureXFATStylePath(full_remote_dir + populated_dir);
    auto request = std::make_shared<Mkdir>(full_remote_subdir);
    interface.SendCommandSync(request);
    if (!request->IsOK() && request->status != ERR_EXISTS) {
      std::cout << *request << std::endl;
      return false;
    }
  }

  return true;
}

bool SyncDirectory(XBOXInterface& interface, const std::string& local_directory,
                   const std::string& remote_directory,
                   SyncFileMissingAction missing_action) {
  bool remote_exists;
  bool remote_is_dir;
  if (!CheckRemotePath(interface, remote_directory, remote_exists,
                       remote_is_dir)) {
    return false;
  }

  if (remote_exists && !remote_is_dir) {
    std::cout << "Remote path '" << remote_directory << "' is a file."
              << std::endl;
    return false;
  }

  std::set<std::string> local_files;
  std::set<std::string> relative_local_files;
  std::set<std::string> local_populated_dirs;
  {
    auto process_file = [&local_files, &relative_local_files, &local_directory,
                         &local_populated_dirs](const std::string& local_file) {
      auto path = std::filesystem::path(local_file);
      if (path.filename() == ".DS_Store") {
        return true;
      }

      auto relative_file = std::filesystem::relative(path, local_directory);
      local_populated_dirs.insert(relative_file.parent_path());
      local_files.insert(local_file);
      relative_local_files.insert(relative_file);
      return true;
    };

    if (!WalkDirectory(local_directory, process_file)) {
      std::cout << "Failed to process local directory '" << local_directory
                << "'" << std::endl;
      return false;
    }
  }

  std::string remote_root = EnsureTrailingBackslash(remote_directory);
  auto local_root = std::filesystem::path(local_directory);
  auto process_remote_file = [&interface, &local_files, &relative_local_files,
                              &local_root, &missing_action,
                              &remote_root](const std::string& subdir,
                                            const DirList::Entry& remote_file) {
    std::string relative_path = remote_file.name;
    if (!subdir.empty()) {
      relative_path = XFATPathToLocalPath(
          std::filesystem::path(subdir + "\\" + remote_file.name));
    }

    auto full_remote_path = EnsureXFATStylePath(remote_root + relative_path);

    if (relative_local_files.find(relative_path) !=
        relative_local_files.end()) {
      auto local_file = local_root / relative_path;
      local_files.erase(local_file);
      relative_local_files.erase(relative_path);

      auto change_timestamp = SafeXFATTimestampForFile(local_file);
      auto remote_change_timestamp =
          remote_file.change_timestamp & kUsableTimestampRange;
      if (change_timestamp == remote_change_timestamp &&
          remote_file.filesize == std::filesystem::file_size(local_file)) {
        std::cout << "Skipping '" << local_file
                  << "' with same modification time." << std::endl;
      } else {
        std::cout << "Uploading '" << local_file << "'" << std::endl;
        if (!UploadFileWithoutChecking(interface, local_file, full_remote_path,
                                       true)) {
          return false;
        }
      }
    } else if (missing_action == SyncFileMissingAction::DELETE) {
      auto request = std::make_shared<Delete>(full_remote_path, false);
      interface.SendCommandSync(request);
      if (!request->IsOK()) {
        std::cout << *request << std::endl;
        return false;
      }
    }
    return true;
  };

  auto process_remote_dir = [&interface, &missing_action, &local_populated_dirs,
                             &remote_root](const std::string& remote_dir,
                                           bool& should_skip) {
    std::string full_remote_dir = EnsureTrailingBackslash(remote_dir);
    if (full_remote_dir.find(remote_root) != 0) {
      std::cout << "Error: Remote directory '" << full_remote_dir
                << "' is not relative to '" << remote_root << "'" << std::endl;
      return false;
    }
    std::string subdir = full_remote_dir.substr(remote_root.size());
    if (subdir.back() == '\\') {
      subdir.pop_back();
    }
    subdir = XFATPathToLocalPath(subdir);
    should_skip = (!subdir.empty() && local_populated_dirs.find(subdir) ==
                                          local_populated_dirs.end());
    if (should_skip && missing_action == SyncFileMissingAction::DELETE) {
      if (!DeleteRecursively(interface, full_remote_dir)) {
        return false;
      }
    }
    return true;
  };

  // Remove or update everything that exists on the remote.
  if (!WalkRemoteDir(interface, remote_directory, process_remote_file,
                     process_remote_dir)) {
    return false;
  }

  // Add anything that only exists locally.
  if (!CreateRemoteDirectoryHierarchy(interface, remote_directory,
                                      local_populated_dirs)) {
    return false;
  }

  for (auto& file : local_files) {
    auto remote_relative_path = remote_root;
    remote_relative_path += std::filesystem::relative(file, local_directory);
    if (!UploadFileWithoutChecking(interface, file, remote_relative_path,
                                   true)) {
      return false;
    }
  }

  return true;
}
