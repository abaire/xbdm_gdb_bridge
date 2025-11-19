#define BOOST_TEST_MODULE XboxMemoryTests
#include <algorithm>
#include <boost/test/included/unit_test.hpp>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "test_util/mock_xbdm_server/mock_xbox_state.h"

using namespace xbdm_gdb_bridge::testing;

struct TestFixture {
  MockXboxState state;

  void AddRegion(uint32_t address, const std::vector<uint8_t>& content) {
    state.memory_regions[address] =
        MemoryRegion{address, static_cast<uint32_t>(content.size()), content};
  }
};

BOOST_FIXTURE_TEST_SUITE(MemoryReadTests, TestFixture)

BOOST_AUTO_TEST_CASE(ReadEmptyStateReturnsFalse) {
  std::vector<uint8_t> result;
  bool success = state.ReadVirtualMemory(result, 0x1000, 10, 0);

  BOOST_CHECK_EQUAL(success, false);
  std::vector<uint8_t> expected(10, 0);
  BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(),
                                expected.end());
}

BOOST_AUTO_TEST_CASE(ReadExactRegion) {
  std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
  AddRegion(0x1000, data);

  std::vector<uint8_t> result;
  bool success = state.ReadVirtualMemory(result, 0x1000, 4);

  BOOST_CHECK_EQUAL(success, true);
  BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), data.begin(),
                                data.end());
}

BOOST_AUTO_TEST_CASE(ReadSubsetOfRegion) {
  // Region: 00 11 22 33 44 55
  std::vector<uint8_t> data = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  AddRegion(0x2000, data);

  // Read offset 2, length 3 -> Expect 22 33 44
  std::vector<uint8_t> result;
  bool success = state.ReadVirtualMemory(result, 0x2002, 3);
  std::vector<uint8_t> expected = {0x22, 0x33, 0x44};

  BOOST_CHECK_EQUAL(success, true);
  BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(),
                                expected.end());
}

BOOST_AUTO_TEST_CASE(ReadWithStartUnmapped) {
  // Region starts at 0x1005. Request starts at 0x1000.
  // Gap 0x1000-0x1005 -> Should return false
  std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC};
  AddRegion(0x1005, data);

  std::vector<uint8_t> result;
  bool success = state.ReadVirtualMemory(result, 0x1000, 8);

  BOOST_CHECK_EQUAL(success, false);
}

BOOST_AUTO_TEST_CASE(ReadWithEndUnmapped) {
  // Region ends before request ends.
  std::vector<uint8_t> data = {0xFF, 0xEE};
  AddRegion(0x1000, data);  // Ends at 0x1002

  std::vector<uint8_t> result;
  bool success = state.ReadVirtualMemory(result, 0x1000, 4);

  BOOST_CHECK_EQUAL(success, false);
}

BOOST_AUTO_TEST_CASE(ReadSpanningMultipleRegionsWithHole) {
  // Complex case:
  // 0x1000: [0xAA, 0xAA] (Ends 0x1002)
  // ... HOLE of 2 bytes ...
  // 0x1004: [0xBB, 0xBB] (Ends 0x1006)

  AddRegion(0x1000, {0xAA, 0xAA});
  AddRegion(0x1004, {0xBB, 0xBB});

  // Read from 0x1000 to 0x1006 (6 bytes total)
  std::vector<uint8_t> result;
  bool success = state.ReadVirtualMemory(result, 0x1000, 6);

  // Should fail because of the hole at 0x1002-0x1004
  BOOST_CHECK_EQUAL(success, false);
}

BOOST_AUTO_TEST_CASE(ReadSpanningContiguousRegions) {
  // 0x1000: [0xAA, 0xBB] (Ends 0x1002)
  // 0x1002: [0xCC, 0xDD] (Starts 0x1002)

  AddRegion(0x1000, {0xAA, 0xBB});
  AddRegion(0x1002, {0xCC, 0xDD});

  std::vector<uint8_t> result;
  bool success = state.ReadVirtualMemory(result, 0x1000, 4);

  std::vector<uint8_t> expected = {0xAA, 0xBB, 0xCC, 0xDD};

  BOOST_CHECK_EQUAL(success, true);
  BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(),
                                expected.end());
}

BOOST_AUTO_TEST_CASE(ReadZeroLength) {
  // Specific logic in the function returns false for length 0
  AddRegion(0x1000, {0x01});
  std::vector<uint8_t> result;
  bool success = state.ReadVirtualMemory(result, 0x1000, 0);
  BOOST_CHECK_EQUAL(success, false);
}

BOOST_AUTO_TEST_CASE(SafetyCheckDataSmallerThanSizeAttribute) {
  // Scenario: The memory map says region is 100 bytes, but the simulation only
  // has 2 bytes of data This is considered "valid memory" (it is mapped), but
  // the data is missing (maybe zero-init). Function should return TRUE because
  // the region exists in the map.

  MemoryRegion region;
  region.base_address = 0x5000;
  region.size = 100;           // Claims to be huge
  region.data = {0x11, 0x22};  // Actually tiny
  state.memory_regions[0x5000] = region;

  std::vector<uint8_t> result;
  bool success = state.ReadVirtualMemory(result, 0x5000, 4, 0xCC);

  // Expectation:
  // 1. Returns true (memory is mapped).
  // 2. Bytes 0-1 are 0x11, 0x22.
  // 3. Bytes 2-3 are 0xCC (the fill value), because the data vector ran out but
  // the mapping exists.

  std::vector<uint8_t> expected = {0x11, 0x22, 0xCC, 0xCC};

  BOOST_CHECK_EQUAL(success, true);
  BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(),
                                expected.end());
}

BOOST_AUTO_TEST_SUITE_END()
