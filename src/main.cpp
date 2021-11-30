#include <boost/program_options.hpp>
#include <iostream>
#include <vector>

#include "net/ip_address.h"
#include "shell/shell.h"
#include "util/logging.h"
#include "xbox/xbox_interface.h"

#define DEFAULT_PORT 731

namespace po = boost::program_options;

static const std::string kCommandDelimiter{"&&"};

void validate(boost::any &v, const std::vector<std::string> &values,
              IPAddress *, int) {
  po::validators::check_first_occurrence(v);
  const std::string &value = po::validators::get_single_string(values);

  IPAddress addr(value, DEFAULT_PORT);
  v = boost::any(addr);
}

static int main_(const IPAddress &xbox_addr,
                 const std::vector<std::vector<std::string>> &commands,
                 bool run_shell) {
  LOG(trace) << "Startup - XBDM @ " << xbox_addr;
  auto interface = std::make_shared<XBOXInterface>("XBOX", xbox_addr);
  interface->Start();

  auto shell = Shell(interface);
  for (auto &command : commands) {
    shell.ProcessCommand(command);
  }

  if (run_shell) {
    shell.Run();
  }

  interface->Stop();
  return 0;
}

// Splits the given vector of strings into sub-vectors delimited by
// kCommandDelimiter
static std::vector<std::vector<std::string>> split_commands(
    const std::vector<std::string> &additional_commands) {
  std::vector<std::vector<std::string>> ret;

  if (additional_commands.empty()) {
    return ret;
  }

  std::vector<std::string> cmd;
  for (auto &elem : additional_commands) {
    if (elem == "&&") {
      ret.push_back(cmd);
      cmd.clear();
      continue;
    }

    cmd.push_back(elem);
  }
  ret.push_back(cmd);

  return std::move(ret);
}

int main(int argc, char *argv[]) {
  bool run_shell{false};
  bool disable_xbdm_logging{false};
  bool disable_gdb_logging{false};
  bool disable_debugger_logging{false};

  po::options_description opts("Options:");
  // clang-format off
  opts.add_options()
      ("help,?", po::bool_switch(), "Print this help message.")
      ("xbox", po::value<IPAddress>()->value_name("<IP[:Port]>"), "IP (and optionally Port) of the XBOX to connect to.")
      ("shell,s", po::bool_switch(&run_shell), "Run the shell even if an initial command is given.")
      ("verbosity,v", po::value<uint32_t>()->value_name("<level>")->default_value(0), "Sets logging verbosity.")
      ("no-debugger", po::bool_switch(&disable_debugger_logging), "Disable verbose logging for the debugger module.")
      ("no-gdb", po::bool_switch(&disable_gdb_logging), "Disable verbose logging for the GDB module.")
      ("no-xbdm", po::bool_switch(&disable_xbdm_logging), "Disable verbose logging for the XBDM module.")
      ("command", po::value<std::vector<std::string>>()->multitoken(), "Optional command to run instead of running the shell.")
      ;
  // clang-format on

  po::positional_options_description positional;
  positional.add("xbox", 1);
  positional.add("command", -1);

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

  IPAddress xbox_addr = vm["xbox"].as<IPAddress>();
  uint32_t verbosity = vm["verbosity"].as<uint32_t>();
  std::vector<std::string> additional_commands;
  auto command_params = vm.find("command");
  if (command_params != vm.end()) {
    additional_commands = command_params->second.as<std::vector<std::string>>();
  }

  logging::InitializeLogging(verbosity);
  logging::SetGDBTraceEnabled(!disable_gdb_logging);
  logging::SetXBDMTraceEnabled(!disable_xbdm_logging);
  logging::SetDebuggerTraceEnabled(!disable_debugger_logging);

  std::vector<std::vector<std::string>> commands =
      split_commands(additional_commands);

  return main_(xbox_addr, commands, run_shell || commands.empty());
}
