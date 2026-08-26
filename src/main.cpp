#include <boost/algorithm/string/join.hpp>
#include <boost/program_options.hpp>
#include <cstdlib>
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

namespace {

void PrintHelp(std::ostream& os, const po::options_description& visible_opts) {
  os << "Usage: xbdm_gdb_bridge [options] [<IP[:Port]>] [command...]\n\n"
     << "Positional arguments:\n"
     << "  <IP[:Port]>                     IP (and optionally Port) of the "
        "XBOX to\n"
     << "                                  connect to. May be omitted if the\n"
     << "                                  XBDM_XBOX_ADDRESS environment "
        "variable is set.\n"
     << "  [command...]                    Optional command to run instead of "
        "running\n"
     << "                                  the shell.\n\n"
     << visible_opts << "\n"
     << "Environment variables:\n"
     << "  XBDM_XBOX_ADDRESS               IP (and optionally Port) of the "
        "default XBOX\n"
     << "                                  to connect to if not specified on "
        "the\n"
     << "                                  command line.\n";
}

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

  po::options_description visible_opts("Options");
  // clang-format off
  visible_opts.add_options()
      ("help,?", po::bool_switch(), "Print this help message.")
      ("shell,s", po::bool_switch(&run_shell), "Run the shell even if an initial command is given.")
      ("verbosity,v", po::value<uint32_t>()->value_name("<level>")->default_value(0), "Sets logging verbosity.")
      ("no-debugger", po::bool_switch(&disable_debugger_logging), "Disable verbose logging for the debugger module.")
      ("no-gdb", po::bool_switch(&disable_gdb_logging), "Disable verbose logging for the GDB module.")
      ("no-xbdm", po::bool_switch(&disable_xbdm_logging), "Disable verbose logging for the XBDM module.")
      ;
  // clang-format on

  po::options_description hidden_opts;
  hidden_opts.add_options()("positional-args",
                            po::value<std::vector<std::string>>(),
                            "Positional arguments");

  po::positional_options_description positional;
  positional.add("positional-args", -1);

  po::options_description all_opts;
  all_opts.add(visible_opts).add(hidden_opts);

  auto parsed = po::command_line_parser(argc, argv)
                    .options(all_opts)
                    .positional(positional)
                    .run();

  po::variables_map vm;
  try {
    po::store(parsed, vm);

    if (vm["help"].as<bool>()) {
      PrintHelp(std::cout, visible_opts);
      return 0;
    }

    po::notify(vm);
  } catch (boost::program_options::error& e) {
    std::cout << "ERROR: " << e.what() << std::endl;
    PrintHelp(std::cout, visible_opts);
    return 1;
  }

  std::vector<std::string> positional_args;
  if (vm.count("positional-args")) {
    positional_args = vm["positional-args"].as<std::vector<std::string>>();
  }

  std::string xbox_address_str;
  std::vector<std::string> additional_commands;

  const char* env_xbox_address = std::getenv("XBDM_XBOX_ADDRESS");
  bool has_env_address =
      (env_xbox_address != nullptr && *env_xbox_address != '\0');

  if (!positional_args.empty()) {
    const std::string& first_arg = positional_args.front();
    if (IPAddress::IsIPv4Address(first_arg)) {
      xbox_address_str = first_arg;
      additional_commands.assign(positional_args.begin() + 1,
                                 positional_args.end());
    } else if (has_env_address) {
      xbox_address_str = env_xbox_address;
      additional_commands = positional_args;
    } else {
      std::cout
          << "ERROR: '" << first_arg
          << "' does not appear to be an IP address and XBDM_XBOX_ADDRESS "
             "is not set."
          << std::endl;
      PrintHelp(std::cout, visible_opts);
      return 1;
    }
  } else {
    if (has_env_address) {
      xbox_address_str = env_xbox_address;
    } else {
      std::cout << "Missing required <IP[:Port]> parameter and "
                   "XBDM_XBOX_ADDRESS is not set."
                << std::endl;
      PrintHelp(std::cout, visible_opts);
      return 1;
    }
  }

  if (!IPAddress::IsIPv4Address(xbox_address_str)) {
    std::cout << "ERROR: '" << xbox_address_str
              << "' is not a valid XBOX IP address." << std::endl;
    PrintHelp(std::cout, visible_opts);
    return 1;
  }

  IPAddress xbox_addr(xbox_address_str, DEFAULT_PORT);
  uint32_t verbosity = vm["verbosity"].as<uint32_t>();

  logging::InitializeLogging(verbosity);
  logging::SetGDBTraceEnabled(!disable_gdb_logging);
  logging::SetXBDMTraceEnabled(!disable_xbdm_logging);
  logging::SetDebuggerTraceEnabled(!disable_debugger_logging);

  std::vector<std::vector<std::string>> commands =
      command_line_command_tokenizer::SplitCommands(additional_commands);

  return main_(xbox_addr, commands, run_shell || commands.empty());
}
