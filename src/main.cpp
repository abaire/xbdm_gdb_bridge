#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <vector>

#include "net/address.h"

#define DEFAULT_PORT 731

namespace logging = boost::log;
namespace po = boost::program_options;

void validate(boost::any &v, const std::vector<std::string> &values, Address *,
              int) {
  po::validators::check_first_occurrence(v);
  const std::string &value = po::validators::get_single_string(values);

  Address addr(value, DEFAULT_PORT);
  v = boost::any(addr);
}

static int _main(const Address &xbox_addr, bool colorize_output) {

  return 0;
}

int main(int argc, char *argv[]) {
  BOOST_LOG_TRIVIAL(trace) << "Startup.";

  bool colorize;

  po::options_description opts("Options:");
  // clang-format off
  opts.add_options()
      ("help,?", po::bool_switch(), "Print this help message.")
      ("xbox", po::value<Address>()->value_name("<ip[:port]>"), "IP (and optionally port) of the XBOX to connect to.")
      ("color,c", po::bool_switch(&colorize), "Colorize output.")
      ("verbosity,v", po::value<uint32_t>()->value_name("<level>")->default_value(0), "Sets logging verbosity.")
      ;
  // clang-format on

  po::positional_options_description positional;
  positional.add("xbox", 1);

  auto parsed = po::command_line_parser(argc, argv)
                    .allow_unregistered()
                    .options(opts)
                    .positional(positional)
                    .run();

  po::variables_map vm;
  try {
    po::store(parsed, vm);

    if (vm["help"].as<bool>()) {
      std::cout << opts << std::endl;
      return 1;
    }

    po::notify(vm);
  } catch (boost::program_options::error &e) {
    std::cout << "ERROR: " << e.what() << std::endl;
    std::cout << opts << std::endl;
    return 1;
  }

  if (!vm.count("xbox")) {
    std::cout << "Missing required 'xbox' parameter." << std::endl;
    std::cout << opts << std::endl;
    return 1;
  }

  Address xbox_addr = vm["xbox"].as<Address>();
  uint32_t verbosity = vm["verbosity"].as<uint32_t>();
  logging::core::get()->set_filter(logging::trivial::severity >=
                                   logging::trivial::info - verbosity);

  return _main(xbox_addr, colorize);
}
