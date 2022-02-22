#include <boost/test/unit_test.hpp>

#include "handler_loader/dxt_library.h"

BOOST_AUTO_TEST_SUITE(dxt_library_suite)

BOOST_AUTO_TEST_CASE(parse_test) {
  std::vector<char> data;
  // TODO: Make this configurable, e.g., framework::master_test_suite().argc.
  DXTLibrary lib("./ddxt-bin/libdynamic_dxt_loader.dll");

  BOOST_TEST(lib.Parse());
}

BOOST_AUTO_TEST_CASE(relocate_test) {
  std::vector<char> data;
  // TODO: Make this configurable.
  DXTLibrary lib("./ddxt-bin/libdynamic_dxt_loader.dll");
  lib.Parse();

  // Relocation should fail with unresolved imports.
  BOOST_TEST(!lib.Relocate(0xF00D));

  for (auto &dll : lib.GetImports()) {
    for (auto &import : dll.second) {
      import.real_address = 0x0BADF00D;
    }
  }
  BOOST_TEST(lib.Relocate(0xF00D));
}

BOOST_AUTO_TEST_CASE(entrypoint_test) {
  std::vector<char> data;
  // TODO: Make this configurable.
  DXTLibrary lib("./ddxt-bin/libdynamic_dxt_loader.dll");
  lib.Parse();

  BOOST_TEST(lib.GetEntrypoint() == 0x11000);

  for (auto &dll : lib.GetImports()) {
    for (auto &import : dll.second) {
      import.real_address = 0x0BADF00D;
    }
  }
  BOOST_TEST(lib.Relocate(0xF00D));

  uint32_t ep = lib.GetEntrypoint();
  BOOST_TEST(lib.GetEntrypoint() == 0x1000D);
}

BOOST_AUTO_TEST_SUITE_END()
