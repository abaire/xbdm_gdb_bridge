#define BOOST_TEST_MODULE XboxDebuggerTests
#include <boost/test/unit_test.hpp>

#include "util/logging.h"

struct GlobalTestFixture {
  GlobalTestFixture() {
    logging::InitializeLogging(boost::log::trivial::severity_level::info);
  }

  ~GlobalTestFixture() {}
};

BOOST_GLOBAL_FIXTURE(GlobalTestFixture);
