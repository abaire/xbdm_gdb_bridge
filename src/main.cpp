#include <boost/program_options.hpp>
#include <iostream>
#include <vector>

#include "configure.h"
#include "net/ip_address.h"
#include "shell/gdb/gdb_commands.h"
#include "shell/shell.h"
#include "util/logging.h"
#include "util/parsing.h"
#include "xbox/bridge/gdb_xbox_interface.h"
#include "xbox/debugger/debugger_expression_parser.h"
#include "xbox/xbox_interface.h"

#define DEFAULT_PORT 731

namespace po = boost::program_options;

void validate(boost::any& v, const std::vector<std::string>& values, IPAddress*,
              int) {
  po::validators::check_first_occurrence(v);
  const std::string& value = po::validators::get_single_string(values);

  IPAddress addr(value, DEFAULT_PORT);
  v = boost::any(addr);
}

namespace {

int main_(const IPAddress& xbox_addr,
          const std::vector<std::vector<std::string>>& commands,
          bool run_shell) {
  LOG(trace) << "Startup - XBDM @ " << xbox_addr;
  std::shared_ptr<XBOXInterface> interface =
      std::make_shared<GDBXBOXInterface>("XBOX", xbox_addr);

  interface->SetExpressionParser(std::make_shared<DebuggerExpressionParser>());

  interface->Start();

  auto shell = Shell(interface);
  RegisterGDBCommands(shell);

  for (auto& command : commands) {
#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
    LOG(trace) << "Processing startup command '" << command.front() << "'";
#endif

    std::string flat_command = boost::algorithm::join(command, " ");
    ;
    ArgParser parser(flat_command);
    shell.ProcessCommand(parser);
  }

  if (run_shell) {
#ifdef ENABLE_HIGH_VERBOSITY_LOGGING
    LOG(trace) << "Running shell";
#endif
    shell.Run();
  }

  interface->Stop();
  return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
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
  } catch (boost::program_options::error& e) {
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
      command_line_command_tokenizer::SplitCommands(additional_commands);

  return main_(xbox_addr, commands, run_shell || commands.empty());
}
