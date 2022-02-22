#include <boost/test/unit_test.hpp>

#include "handler_loader/coff_loader.h"
#include "handler_loader/dxt_library.h"

BOOST_AUTO_TEST_SUITE(dxt_library_suite)

BOOST_AUTO_TEST_CASE(large_link_driver) {
  std::vector<char> data;
  DXTLibrary lib(
      "/mnt/linux_data/development/xbox/nxdk_dyndxt/cmake-build-release/"
      "dynamic_dxt_loader.ar");

  BOOST_TEST(lib.Parse());
}

BOOST_AUTO_TEST_SUITE_END()
