#include <boost/test/unit_test.hpp>
#include <string>

#include "util/path.h"

BOOST_AUTO_TEST_SUITE(path_suite)

BOOST_AUTO_TEST_CASE(split_xbe_path_empty) {
  std::string xbe_file;
  std::string xbe_dir;
  bool result = SplitXBEPath("", xbe_dir, xbe_file);
  BOOST_TEST(!result);
}

BOOST_AUTO_TEST_CASE(split_xbe_path_root_no_slash) {
  std::string xbe_file;
  std::string xbe_dir;
  bool result = SplitXBEPath("e:", xbe_dir, xbe_file);
  BOOST_TEST(result);
  BOOST_TEST(xbe_dir == "e:\\");
  BOOST_TEST(xbe_file == "default.xbe");
}

BOOST_AUTO_TEST_CASE(split_xbe_path_root) {
  std::string xbe_file;
  std::string xbe_dir;
  bool result = SplitXBEPath("e:\\", xbe_dir, xbe_file);
  BOOST_TEST(result);
  BOOST_TEST(xbe_dir == "e:\\");
  BOOST_TEST(xbe_file == "default.xbe");
}

BOOST_AUTO_TEST_CASE(split_xbe_path_subdir) {
  std::string xbe_file;
  std::string xbe_dir;
  bool result = SplitXBEPath("e:\\subdir", xbe_dir, xbe_file);
  BOOST_TEST(result);
  BOOST_TEST(xbe_dir == "e:\\subdir\\");
  BOOST_TEST(xbe_file == "default.xbe");
}

BOOST_AUTO_TEST_CASE(split_xbe_path_in_root_dir) {
  std::string xbe_file;
  std::string xbe_dir;
  bool result = SplitXBEPath("default.xbe", xbe_dir, xbe_file);
  BOOST_TEST(result);
  BOOST_TEST(xbe_dir == "\\");
  BOOST_TEST(xbe_file == "default.xbe");
}

BOOST_AUTO_TEST_CASE(split_xbe_path_with_xbe_no_drive_letter) {
  std::string xbe_file;
  std::string xbe_dir;
  bool result = SplitXBEPath("dir\\test.xbe", xbe_dir, xbe_file);
  BOOST_TEST(result);
  BOOST_TEST(xbe_dir == "dir\\");
  BOOST_TEST(xbe_file == "test.xbe");
}

BOOST_AUTO_TEST_CASE(split_xbe_path_with_xbe) {
  std::string xbe_file;
  std::string xbe_dir;
  bool result = SplitXBEPath("c:\\dir\\test.xbe", xbe_dir, xbe_file);
  BOOST_TEST(result);
  BOOST_TEST(xbe_dir == "c:\\dir\\");
  BOOST_TEST(xbe_file == "test.xbe");
}

BOOST_AUTO_TEST_CASE(with_embedded_xbe_dir) {
  std::string xbe_file;
  std::string xbe_dir;
  bool result = SplitXBEPath("c:\\default.xbe\\", xbe_dir, xbe_file);
  BOOST_TEST(result);
  BOOST_TEST(xbe_dir == "c:\\default.xbe\\");
  BOOST_TEST(xbe_file == "default.xbe");
}

BOOST_AUTO_TEST_CASE(with_embedded_xbe_dir_and_explicit_xbe) {
  std::string xbe_file;
  std::string xbe_dir;
  bool result = SplitXBEPath("c:\\default.xbe\\test.xbe", xbe_dir, xbe_file);
  BOOST_TEST(result);
  BOOST_TEST(xbe_dir == "c:\\default.xbe\\");
  BOOST_TEST(xbe_file == "test.xbe");
}

BOOST_AUTO_TEST_SUITE_END()
