#ifndef XBDM_GDB_BRIDGE_FILE_UTIL_H
#define XBDM_GDB_BRIDGE_FILE_UTIL_H

#include <filesystem>

#include "rdcp/xbdm_requests.h"
#include "shell/command.h"

bool CheckRemotePath(XBOXInterface &interface, const std::string &path,
                     bool &exists, bool &is_dir);

bool CheckRemotePath(XBOXInterface &interface, const std::string &path,
                     bool &exists, bool &is_dir, uint64_t &size,
                     uint64_t &create_timestamp, uint64_t &change_timestamp);

bool FetchDirectoryEntries(XBOXInterface &interface, const std::string &path,
                           std::list<DirList::Entry> &directories,
                           std::list<DirList::Entry> &files);

bool DeleteRecursively(XBOXInterface &interface, const std::string &path);

bool SaveFile(XBOXInterface &interface, const std::string &remote,
              const std::filesystem::path &local);

bool SaveDirectory(XBOXInterface &interface, const std::string &remote,
                   const std::filesystem::path &local);

bool SaveRawFile(const std::string &filename_root, uint32_t width,
                 uint32_t height, uint32_t bpp, uint32_t format,
                 const std::vector<uint8_t> &data);

//! Uploads the file at `local_path` to `full_remote_path`, overwriting the file
//! if it exists. Performs no checking to verify that the remote directory is
//! valid.
//!
//! If `set_timestamp` is `true`, also updates the change_time on the created
//! file to (more or less) match the `local_path`'s last_write_time.
bool UploadFileWithoutChecking(XBOXInterface &interface,
                               const std::string &local_path,
                               const std::string &full_remote_path,
                               bool set_timestamp = true);

enum class UploadFileOverwriteAction {
  //! Overwrites any existing file.
  OVERWRITE,
  //! Aborts the action if the file exists.
  ABORT,
  //! Skips the file and indicates success if the file exists.
  SKIP,
};

//! Uploads the file at `local_path` to `remote_path`. If `remote_path` is an
//! existing directory or ends with "\", it is created as necessary and the file
//! is placed within the directory. If `remote_path` exists, `ovewrite_action`
//! determines the behavior.
bool UploadFile(XBOXInterface &interface, const std::string &local_path,
                const std::string &remote_path,
                UploadFileOverwriteAction overwrite_action =
                    UploadFileOverwriteAction::SKIP);

bool UploadDirectory(XBOXInterface &interface, const std::string &local_path,
                     const std::string &remote_path,
                     UploadFileOverwriteAction overwrite_action =
                         UploadFileOverwriteAction::SKIP,
                     bool contents_only = false);

//! Uploads the file at `local_path` to `remote_path` if it does not exist or
//! has a stale change timestamp.
bool SyncFile(XBOXInterface &interface, const std::string &local_path,
              const std::string &remote_path);

enum class SyncFileMissingAction {
  //! Deletes any files or directories that exist on the remote but not local.
  DELETE,
  //! Leaves any files that exist on the remote but not local.
  LEAVE,
};

//! Recursively syncs the given `remote_directory` with `local_directory`.
bool SyncDirectory(XBOXInterface &interface, const std::string &local_directory,
                   const std::string &remote_directory,
                   SyncFileMissingAction missing_action);

#endif  // XBDM_GDB_BRIDGE_FILE_UTIL_H
