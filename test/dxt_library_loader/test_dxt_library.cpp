#include <boost/test/unit_test.hpp>

#include "configure_test.h"
#include "dyndxt_loader/dxt_library.h"

BOOST_AUTO_TEST_SUITE(dxt_library_suite)

BOOST_AUTO_TEST_CASE(parsing_valid_dll_succeeds) {
  std::vector<char> data;
  DXTLibrary lib(DYNDXT_LIB_DIR "/libdynamic_dxt_loader.dll");

  BOOST_TEST(lib.Parse());
}

BOOST_AUTO_TEST_CASE(relocate_with_unresolved_imports_fails) {
  std::vector<char> data;
  DXTLibrary lib(DYNDXT_LIB_DIR "/libdynamic_dxt_loader.dll");
  lib.Parse();

  // Relocation should fail with unresolved imports.
  BOOST_TEST(!lib.Relocate(0xF00D));
}

BOOST_AUTO_TEST_CASE(relocate_with_resolved_imports_succeeds) {
  std::vector<char> data;
  DXTLibrary lib(DYNDXT_LIB_DIR "/libdynamic_dxt_loader.dll");
  lib.Parse();

  for (auto &dll : lib.GetImports()) {
    for (auto &import : dll.second) {
      import.real_address = 0x0BADF00D;
    }
  }
  BOOST_TEST(lib.Relocate(0xF00D));
}

BOOST_AUTO_TEST_CASE(relocating_lower_moves_entrypoint) {
  DXTLibrary lib(DYNDXT_LIB_DIR "/libdynamic_dxt_loader.dll");
  lib.Parse();
  auto file_entrypoint = lib.GetEntrypoint();
  for (auto &dll : lib.GetImports()) {
    for (auto &import : dll.second) {
      import.real_address = 0x0BADF00D;
    }
  }
  auto image_base = lib.GetImageBase();
  auto new_base = image_base / 2;
  auto expected_address = file_entrypoint - new_base;

  BOOST_REQUIRE(lib.Relocate(new_base));

  auto relocated_entrypoint = lib.GetEntrypoint();
  BOOST_TEST(relocated_entrypoint == expected_address);
}

BOOST_AUTO_TEST_CASE(relocating_higher_moves_entrypoint) {
  DXTLibrary lib(DYNDXT_LIB_DIR "/libdynamic_dxt_loader.dll");
  lib.Parse();
  auto file_entrypoint = lib.GetEntrypoint();
  for (auto &dll : lib.GetImports()) {
    for (auto &import : dll.second) {
      import.real_address = 0x0BADF00D;
    }
  }
  auto image_base = lib.GetImageBase();
  auto new_base = image_base + 0xF00D;
  auto expected_address = file_entrypoint + 0xF00D;

  BOOST_REQUIRE(lib.Relocate(new_base));

  auto relocated_entrypoint = lib.GetEntrypoint();
  BOOST_TEST(relocated_entrypoint == expected_address);
}

BOOST_AUTO_TEST_CASE(relocating_to_same_address_retains_entrypoint) {
  DXTLibrary lib(DYNDXT_LIB_DIR "/libdynamic_dxt_loader.dll");
  lib.Parse();
  auto file_entrypoint = lib.GetEntrypoint();
  for (auto &dll : lib.GetImports()) {
    for (auto &import : dll.second) {
      import.real_address = 0x0BADF00D;
    }
  }
  auto image_base = lib.GetImageBase();

  BOOST_REQUIRE(lib.Relocate(image_base));

  auto relocated_entrypoint = lib.GetEntrypoint();
  BOOST_TEST(relocated_entrypoint == file_entrypoint);
}

BOOST_AUTO_TEST_SUITE_END()
