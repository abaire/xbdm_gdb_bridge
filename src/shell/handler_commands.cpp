#include "handler_commands.h"

#include "handler_loader/handler_loader.h"
#include "xbox/debugger/xbdm_debugger.h"

Command::Result HandlerCommandLoadBootstrap::operator()(
    XBOXInterface &interface, const std::vector<std::string> &) {
  auto debugger = interface.Debugger();
  if (!debugger) {
    std::cout << "Debugger not attached." << std::endl;
    return HANDLED;
  }

  if (!debugger->HaltAll()) {
    std::cout << "Failed to halt target." << std::endl;
  }

  bool successful = HandlerLoader::Bootstrap(interface);
  if (!successful) {
    std::cout << "Failed to inject handler loader. XBDM handlers will not work."
              << std::endl;
  }

cleanup:
  if (!debugger->ContinueAll()) {
    std::cout << "Failed to resume target." << std::endl;
  }

  if (!debugger->Go()) {
    std::cout << "Failed to go." << std::endl;
  }

  return HANDLED;
}
