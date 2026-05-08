#include <boost/test/unit_test.hpp>
#include <filesystem>
#include <fstream>
#include <set>

#include "configure_test.h"
#include "shell/file_util.h"
#include "test_util/mock_xbdm_server/mock_server_debugger_interface_fixture.h"
#include "test_util/mock_xbdm_server/mock_xbdm_server.h"

using namespace xbdm_gdb_bridge;
using namespace xbdm_gdb_bridge::testing;

#define FILE_UTIL_TEST_CASE(__name) \
  BOOST_AUTO_TEST_CASE(__name, *boost::unit_test::timeout(TEST_TIMEOUT_SECONDS))

namespace {

struct UploadDirectoryFixture : public XBDMDebuggerInterfaceFixture {
  UploadDirectoryFixture() {
    temp_dir = std::filesystem::temp_directory_path() / "xbdm_test_upload";
    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    // Mock server handlers
    server->SetCommandHandler(
        "getfileattributes",
        [this](ClientTransport& client, const std::string& params) {
          RDCPMapResponse map(params);
          std::string name = map.GetString("name");

          if (remote_files.count(name)) {
            server->SendResponse(client, StatusCode::OK_MULTILINE_RESPONSE);
            server->SendStringWithTerminator(client, "sizehi=0 sizelo=10");
            server->SendMultilineTerminator(client);
            return true;
          }

          if (name.back() != '\\') {
            name += "\\";
          }
          if (remote_dirs.count(name)) {
            server->SendResponse(client, StatusCode::OK_MULTILINE_RESPONSE);
            server->SendStringWithTerminator(client, "directory");
            server->SendMultilineTerminator(client);
            return true;
          }

          server->SendResponse(client, StatusCode::ERR_FILE_NOT_FOUND);
          return true;
        });

    server->SetCommandHandler(
        "mkdir", [this](ClientTransport& client, const std::string& params) {
          RDCPMapResponse map(params);
          std::string name = map.GetString("name");
          if (name.back() != '\\') {
            name += "\\";
          }

          if (remote_dirs.count(name)) {
            server->SendResponse(client, StatusCode::ERR_EXISTS);
            return true;
          }

          remote_dirs.insert(name);
          created_dirs.insert(name);

          server->SendResponse(client, StatusCode::OK);
          return true;
        });

    server->SetCommandHandler(
        "sendfile", [this](ClientTransport& client, const std::string& params) {
          RDCPMapResponse map(params);
          std::string name = map.GetString("name");
          uploaded_files.insert(name);
          server->SendResponse(client, StatusCode::OK);
          return true;
        });

    server->SetCommandHandler(
        "setfileattributes",
        [this](ClientTransport& client, const std::string& params) {
          server->SendResponse(client, StatusCode::OK);
          return true;
        });
  }

  ~UploadDirectoryFixture() override { std::filesystem::remove_all(temp_dir); }

  std::filesystem::path CreateLocalFile(
      const std::string& relative_path,
      const std::string& content = "test") const {
    auto full_path = temp_dir / relative_path;
    std::filesystem::create_directories(full_path.parent_path());
    std::ofstream ofs(full_path);
    ofs << content;
    return full_path;
  }

  void AddRemoteDirectory(const std::string& path) {
    if (path.back() != '\\') {
      remote_dirs.insert(path + "\\");
    } else {
      remote_dirs.insert(path);
    }
  }

  std::filesystem::path temp_dir;
  std::set<std::string> remote_dirs;
  std::set<std::string> remote_files;
  std::set<std::string> created_dirs;
  std::set<std::string> uploaded_files;
};

}  // namespace

BOOST_FIXTURE_TEST_SUITE(UploadDirectoryTests, UploadDirectoryFixture)

FILE_UTIL_TEST_CASE(UploadDirectorySimple) {
  CreateLocalFile("a.txt");
  CreateLocalFile("sub/b.txt");
  CreateLocalFile("sub/deeper/c.txt");
  CreateLocalFile("sub/deeper/d.txt");

  AddRemoteDirectory("e:\\target");

  std::stringstream out;
  bool result =
      UploadDirectory(*interface, temp_dir.string(), "e:\\target",
                      UploadFileOverwriteAction::OVERWRITE, false, false, out);

  BOOST_CHECK(result);
  std::string expected_root =
      "e:\\target\\" + temp_dir.filename().string() + "\\";

  BOOST_CHECK(created_dirs.count(expected_root));
  BOOST_CHECK(created_dirs.count(expected_root + "sub\\"));
  BOOST_CHECK(created_dirs.count(expected_root + "sub\\deeper\\"));
  BOOST_CHECK_EQUAL(created_dirs.size(), 3);

  BOOST_CHECK(uploaded_files.count(expected_root + "a.txt"));
  BOOST_CHECK(uploaded_files.count(expected_root + "sub\\b.txt"));
  BOOST_CHECK(uploaded_files.count(expected_root + "sub\\deeper\\c.txt"));
  BOOST_CHECK(uploaded_files.count(expected_root + "sub\\deeper\\d.txt"));
  BOOST_CHECK_EQUAL(uploaded_files.size(), 4);
}

FILE_UTIL_TEST_CASE(UploadDirectoryContentsOnly) {
  CreateLocalFile("a.txt");

  AddRemoteDirectory("e:\\target");

  std::stringstream out;
  bool result =
      UploadDirectory(*interface, temp_dir.string(), "e:\\target",
                      UploadFileOverwriteAction::OVERWRITE, true, false, out);

  BOOST_CHECK(result);
  BOOST_CHECK(uploaded_files.count("e:\\target\\a.txt"));
  BOOST_CHECK(
      !created_dirs.count("e:\\target\\" + temp_dir.filename().string()));
}

FILE_UTIL_TEST_CASE(UploadDirectoryFlatten) {
  CreateLocalFile("a.txt");
  CreateLocalFile("sub/b.txt");
  CreateLocalFile("sub/deeper/c.txt");

  AddRemoteDirectory("e:\\target");

  std::stringstream out;
  bool result =
      UploadDirectory(*interface, temp_dir.string(), "e:\\target",
                      UploadFileOverwriteAction::OVERWRITE, true, true, out);

  BOOST_CHECK(result);
  BOOST_CHECK(uploaded_files.count("e:\\target\\a.txt"));
  BOOST_CHECK(uploaded_files.count("e:\\target\\b.txt"));
  BOOST_CHECK(uploaded_files.count("e:\\target\\c.txt"));
  BOOST_CHECK_EQUAL(uploaded_files.size(), 3);
  BOOST_CHECK(!created_dirs.count("e:\\target\\sub"));
  BOOST_CHECK_EQUAL(created_dirs.size(), 0);
}

FILE_UTIL_TEST_CASE(UploadDirectorySkipsExisting) {
  CreateLocalFile("a.txt");

  AddRemoteDirectory("e:\\target");
  remote_files.insert("e:\\target\\a.txt");

  std::stringstream out;
  bool result =
      UploadDirectory(*interface, temp_dir.string(), "e:\\target",
                      UploadFileOverwriteAction::SKIP, true, false, out);

  BOOST_CHECK(result);
  BOOST_CHECK(uploaded_files.empty());
}

FILE_UTIL_TEST_CASE(UploadDirectoryAbortsOnExisting) {
  CreateLocalFile("a.txt");

  AddRemoteDirectory("e:\\target");
  remote_files.insert("e:\\target\\a.txt");

  std::stringstream out;
  bool result =
      UploadDirectory(*interface, temp_dir.string(), "e:\\target",
                      UploadFileOverwriteAction::ABORT, true, false, out);

  BOOST_CHECK(!result);
  BOOST_CHECK(uploaded_files.empty());
}

BOOST_AUTO_TEST_SUITE_END()
