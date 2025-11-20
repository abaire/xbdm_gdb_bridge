#include "macro_commands.h"

#include <filesystem>

#include "file_util.h"
#include "util/parsing.h"

Command::Result MacroCommandSyncFile::operator()(XBOXInterface& interface,
                                                 const ArgParser& args,
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

  if (!std::filesystem::is_regular_file(local_path)) {
    out << "Invalid local_path, must be a regular file." << std::endl;
    return HANDLED;
  }

  remote_path = EnsureXFATStylePath(remote_path);

  bool remote_exists;
  bool remote_is_dir;
  uint64_t remote_filesize;
  uint64_t remote_create_timestamp;
  uint64_t remote_change_timestamp;
  if (!CheckRemotePath(interface, remote_path, remote_exists, remote_is_dir,
                       remote_filesize, remote_create_timestamp,
                       remote_change_timestamp, out)) {
    return HANDLED;
  }

  if (remote_is_dir) {
    if (remote_path.back() != '\\') {
      remote_path += "\\";
    }

    remote_path += std::filesystem::path(local_path).filename();
    if (!CheckRemotePath(interface, remote_path, remote_exists, remote_is_dir,
                         remote_filesize, remote_create_timestamp,
                         remote_change_timestamp, out)) {
      return HANDLED;
    }
  }

  SyncFile(interface, local_path, remote_path, out);
  return HANDLED;
}

Command::Result MacroCommandSyncDirectory::operator()(XBOXInterface& interface,
                                                      const ArgParser& args,
                                                      std::ostream& out) {
  ArgParser parser(args);
  std::string local_path;
  if (!parser.Parse(0, local_path)) {
    out << "Missing required local_directory argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  std::string remote_path;
  if (!parser.Parse(1, remote_path)) {
    out << "Missing required remote_directory argument." << std::endl;
    PrintUsage();
    return HANDLED;
  }

  if (!std::filesystem::is_directory(local_path)) {
    out << "Invalid local_directory '" << local_path
        << "', must be a directory." << std::endl;
    return HANDLED;
  }
  remote_path = EnsureXFATStylePath(remote_path);

  SyncFileMissingAction missing_action =
      parser.ArgExists("allow_delete", "delete", "-d")
          ? SyncFileMissingAction::DELETE
          : SyncFileMissingAction::LEAVE;

  SyncDirectory(interface, local_path, remote_path, missing_action, out);

  return HANDLED;
}
