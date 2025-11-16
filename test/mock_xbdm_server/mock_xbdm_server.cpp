#include "mock_xbdm_server.h"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <utility>

#include "mock_xbdm_client_transport.h"
#include "net/delegating_server.h"
#include "net/select_thread.h"
#include "net/tcp_connection.h"
#include "rdcp/rdcp_response_processors.h"
#include "util/logging.h"

namespace xbdm_gdb_bridge::testing {

static constexpr uint8_t kTerminator[] = {'\r', '\n'};
static constexpr long kTerminatorLen =
    sizeof(kTerminator) / sizeof(kTerminator[0]);

constexpr const char kTagMockServer[] = "MockXBDM";
#define LOG_SERVER(lvl) \
  LOG_TAGGED(lvl, xbdm_gdb_bridge::testing::kTagMockServer)

namespace {

std::string Trim(const std::string& str) {
  size_t start = str.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = str.find_last_not_of(" \t\r\n");
  return str.substr(start, end - start + 1);
}

std::string BytesToHex(const std::vector<uint8_t>& bytes) {
  std::stringstream ss;
  ss << std::hex << std::setfill('0');
  for (uint8_t byte : bytes) {
    ss << std::setw(2) << static_cast<int>(byte);
  }
  return ss.str();
}

std::vector<uint8_t> HexToBytes(const std::string& hex) {
  std::vector<uint8_t> bytes;
  for (size_t i = 0; i < hex.length(); i += 2) {
    std::string byte_str = hex.substr(i, 2);
    bytes.push_back(static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16)));
  }
  return bytes;
}

const char* ParseMessage(const char* buffer, const char* buffer_end) {
  auto terminator = std::search(buffer, buffer_end, kTerminator,
                                kTerminator + kTerminatorLen);
  if (terminator == buffer_end) {
    return nullptr;
  }

  return terminator;
}

}  // namespace

MockXBDMServer::MockXBDMServer(uint16_t port) : port_(port) {
  AddThread("MockXBDMServerXBE_Main", 0xDEADBEEF);
}

MockXBDMServer::~MockXBDMServer() { Stop(); }

bool MockXBDMServer::Start() {
  if (running_) {
    return true;
  }

  select_thread_ = std::make_shared<SelectThread>();
  server_ = std::make_shared<DelegatingServer>(
      "MockXBDMServer", [this](int sock, IPAddress& address) {
        this->OnClientConnected(sock, address);
      });
  select_thread_->AddConnection(server_);

  IPAddress address(port_);
  server_->Listen(address);
  port_ = server_->Address().Port();

  select_thread_->Start();
  running_ = true;
  return true;
}

void MockXBDMServer::Stop() {
  if (!running_) {
    return;
  }

  running_ = false;

  server_->Close();
  select_thread_->Stop();

  {
    const std::lock_guard lock(clients_mutex_);
    for (auto& client : clients_) {
      client->Close();
    }
    clients_.clear();
  }
}

const IPAddress& MockXBDMServer::GetAddress() const {
  return server_->Address();
}

void MockXBDMServer::OnClientConnected(int sock, IPAddress& address) {
  LOG_SERVER(trace) << "XBDM client connected from " << address;

  auto transport = std::make_shared<ClientTransport>(
      sock, address,
      [this](ClientTransport& transport) { OnClientBytesReceived(transport); });
  if (!running_) {
    LOG_SERVER(warning) << "Discarding late connection";
    transport->Close();
    return;
  }

  clients_.push_back(transport);
  select_thread_->AddConnection(transport, [this, transport]() {
    const std::lock_guard lock(clients_mutex_);
    auto new_end = std::remove(clients_.begin(), clients_.end(), transport);
    clients_.erase(new_end, clients_.end());
  });

  if (accept_client_connections_) {
    SendResponse(transport, OK_CONNECTED);
  }
}

void MockXBDMServer::SendResponse(ClientTransport& transport,
                                  StatusCode code) const {
  switch (code) {
    case OK:
      SendResponse(transport, code, "OK");
      return;
    case OK_CONNECTED:
      SendResponse(transport, code, "connected");
      return;
    case OK_MULTILINE_RESPONSE:
      SendResponse(transport, code, "multiline response follows");
      return;
    case OK_BINARY_RESPONSE:
      SendResponse(transport, code, "binary response follows");
      return;
    case OK_SEND_BINARY_DATA:
      SendResponse(transport, code, "ready to receive binary");
      return;
    case OK_CONNECTION_DEDICATED:
      SendResponse(transport, code, "connection dedicated");
      return;
    default:
      break;
  }

  std::string default_message = "Code ";
  default_message += std::to_string(code);
  SendResponse(transport, code, default_message);
}

void MockXBDMServer::SendResponse(ClientTransport& transport, StatusCode code,
                                  const std::string& message) const {
  transport.Send(std::to_string(code));
  transport.Send("- ");
  transport.Send(message);
  transport.Send("\r\n");
}

void MockXBDMServer::SendString(
    xbdm_gdb_bridge::testing::ClientTransport& transport,
    const std::string& str) const {
  transport.Send(str);
}

void MockXBDMServer::SendKeyValue(ClientTransport& transport,
                                  const std::string& key, uint32_t value,
                                  bool leading_space) const {
  SendKeyRawValue(transport, key, std::to_string(value), leading_space);
}

void MockXBDMServer::SendKeyHexValue(ClientTransport& transport,
                                     const std::string& key, uint32_t value,
                                     bool leading_space) const {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "0x%x", value);
  SendKeyRawValue(transport, key, buffer, leading_space);
}

void MockXBDMServer::OnClientBytesReceived(
    xbdm_gdb_bridge::testing::ClientTransport& transport) {
  if (transport.BytesAvailable() < 4) {
    return;
  }

  const std::lock_guard<std::recursive_mutex> lock(transport.ReadLock());
  auto& read_buffer = transport.ReadBuffer();
  char const* buffer = reinterpret_cast<char*>(read_buffer.data());

  auto buffer_end = buffer + read_buffer.size();
  auto message_end = ParseMessage(buffer, buffer_end);
  long bytes_processed = 0;

  while (message_end) {
    long message_len = message_end - buffer;
    long packet_len = message_len + kTerminatorLen;
    bytes_processed += packet_len;

    std::string command(buffer, message_len);
    if (!ProcessCommand(transport, command)) {
      transport.DropReceiveBuffer();
      transport.Close();
      return;
    }

    buffer += packet_len;
    message_end = ParseMessage(buffer, buffer_end);
  }

  transport.ShiftReadBuffer(bytes_processed);
}

bool MockXBDMServer::ProcessCommand(ClientTransport& client,
                                    const std::string& command_line) {
  std::string trimmed = Trim(command_line);
  if (trimmed.empty()) {
    return true;
  }

  // Parse command and parameters
  std::string command;
  std::string params_str;

  size_t space_pos = trimmed.find(' ');
  if (space_pos != std::string::npos) {
    command = boost::to_lower_copy(trimmed.substr(0, space_pos));
    params_str = trimmed.substr(space_pos + 1);
  } else {
    command = boost::to_lower_copy(trimmed);
  }

  auto it = custom_handlers_.find(command);
  if (it != custom_handlers_.end()) {
    return it->second(client, command_line);
  }

  // Built-in command handlers
#define HANDLE(cmd, handler)                    \
  if (command == cmd) {                         \
    return Handle##handler(client, params_str); \
  }

  HANDLE("notifyat", NotifyAt)
  HANDLE("debugger", Debugger)
  HANDLE("threads", Threads)
  HANDLE("threadinfo", ThreadInfo)
  HANDLE("modules", Modules)
  HANDLE("walkmem", WalkMemory)

#undef HANDLE

  std::stringstream error_message_builder;
  error_message_builder << "Command '" << command << "' unimplemented";
  auto err = error_message_builder.str();
  LOG_SERVER(trace) << err;
  SendResponse(client, ERR_UNKNOWN_COMMAND, err);
  return true;
}

bool MockXBDMServer::HandleNotifyAt(ClientTransport& client,
                                    const std::string& command_line) {
  RDCPMapResponse params(command_line);

  auto port = params.GetOptionalDWORD("port");
  if (!port.has_value()) {
    SendResponse(client, ERR_UNEXPECTED, "Missing port param");
    return true;
  }

  bool drop = params.HasKey("drop");
  //  bool debug = params.HasKey("debug");

  if (drop) {
    client.CloseNotificationConnection();
  } else {
    // TODO: Move this to a worker thread.
    auto notification_address = client.Address().WithPort(port.value());
    auto connection = client.CreateNotificationConnection(notification_address);
    select_thread_->AddConnection(connection);
  }

  SendResponse(client, OK);

  return true;
}

bool MockXBDMServer::HandleDebugger(ClientTransport& client,
                                    const std::string& command_line) {
  RDCPMapResponse params(command_line);

  if (params.HasKey("connect")) {
    if (state_.is_debugable_) {
      SendResponse(client, OK);
      return true;
    }

    SendResponse(client, ERR_NOT_DEBUGGABLE);
    return true;
  }

  if (params.HasKey("disconnect")) {
    // TODO: Implementme
    SendResponse(client, OK);
    return true;
  }

  SendResponse(client, ERR_UNEXPECTED, "Missing connect/disconnect");
  return true;
}

bool MockXBDMServer::HandleThreads(ClientTransport& client,
                                   const std::string& command_line) {
  SendResponse(client, OK_MULTILINE_RESPONSE, "thread list follows");
  for (auto& thread : state_.threads) {
    SendStringWithTerminator(client, std::to_string(thread.first));
  }
  SendMultilineTerminator(client);

  return true;
}

bool MockXBDMServer::HandleThreadInfo(ClientTransport& client,
                                      const std::string& command_line) {
  RDCPMapResponse params(command_line);

  auto thread_id = params.GetOptionalDWORD("thread");
  if (!thread_id.has_value()) {
    SendResponse(client, ERR_UNEXPECTED, "Missing thread");
    return true;
  }

  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  auto entry = state_.threads.find(thread_id.value());
  if (entry == state_.threads.end()) {
    SendResponse(client, ERR_NO_SUCH_THREAD);
    return true;
  }

  SendResponse(client, OK_MULTILINE_RESPONSE, "thread info follows");
  auto& thread_state = entry->second;

  bool prefix = false;
#define SEND(key, param)                                   \
  do {                                                     \
    SendKeyValue(client, key, thread_state.param, prefix); \
    prefix = true;                                         \
  } while (0)

#define SENDHEX(key, param)                                   \
  do {                                                        \
    SendKeyHexValue(client, key, thread_state.param, prefix); \
    prefix = true;                                            \
  } while (0)

  SEND("suspend", suspended);
  SEND("priority", priority);
  SENDHEX("tlsbase", tls_base);
  SENDHEX("start", start);
  SENDHEX("base", base);
  SENDHEX("limit", limit);
  SENDHEX("createhi", create.hi);
  SENDHEX("createlo", create.low);

#undef SENDHEX
#undef SEND

  SendTerminator(client);

  SendMultilineTerminator(client);

  return true;
}

bool MockXBDMServer::HandleModules(ClientTransport& client,
                                   const std::string& params) {
  SendResponse(client, OK_MULTILINE_RESPONSE);
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);

#define SENDHEX(key, param) SendKeyHexValue(client, key, module.param, true)

  for (auto& entry : state_.modules) {
    auto& module = entry.second;
    SendKeyValue(client, "name", module.name);

    SENDHEX("base", base_address);
    SENDHEX("size", size);
    SENDHEX("check", checksum);
    SENDHEX("timestamp", timestamp);

    SendTerminator(client);
  }
#undef SENDHEX

  SendMultilineTerminator(client);
  return true;
}

bool MockXBDMServer::HandleWalkMemory(ClientTransport& client,
                                      const std::string& params) {
  SendResponse(client, OK_MULTILINE_RESPONSE, "Valid virtual addresses follow");
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);

  bool prefix = false;
#define SENDHEX(key, param)                             \
  do {                                                  \
    SendKeyHexValue(client, key, region.param, prefix); \
    prefix = true;                                      \
  } while (0)

  for (auto& entry : state_.memory_regions) {
    auto& region = entry.second;

    SENDHEX("base", base_address);
    SENDHEX("size", data.size());
    SENDHEX("protect", protect);

    SendTerminator(client);
  }
#undef SENDHEX

  SendMultilineTerminator(client);
  return true;
}

void MockXBDMServer::SetMemoryRegion(uint32_t address,
                                     const std::vector<uint8_t>& data) {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  MemoryRegion region;
  region.base_address = address;
  region.data = data;
  state_.memory_regions[address] = region;
}

void MockXBDMServer::ClearMemoryRegion(uint32_t address) {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  state_.memory_regions.erase(address);
}

std::vector<uint8_t> MockXBDMServer::GetMemoryRegion(uint32_t address,
                                                     size_t length) {
  // Find the memory region containing this address
  for (const auto& [base, region] : state_.memory_regions) {
    if (address >= base && address + length <= base + region.data.size()) {
      size_t offset = address - base;
      return {region.data.begin() + offset,
              region.data.begin() + offset + length};
    }
  }
  return {};
}

uint32_t MockXBDMServer::AddThread(const std::string& name, uint32_t eip) {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  uint32_t thread_id = state_.next_thread_id++;

  SimulatedThread thread;
  thread.id = thread_id;
  thread.eip = eip;
  thread.eflags = 0x202;

  state_.threads[thread_id] = thread;

  if (state_.current_thread_id == 0) {
    state_.current_thread_id = thread_id;
  }

  return thread_id;
}

void MockXBDMServer::RemoveThread(uint32_t thread_id) {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  state_.threads.erase(thread_id);

  if (state_.current_thread_id == thread_id && !state_.threads.empty()) {
    state_.current_thread_id = state_.threads.begin()->first;
  }
}

void MockXBDMServer::SetThreadRegister(uint32_t thread_id,
                                       const std::string& reg_name,
                                       uint32_t value) {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  auto it = state_.threads.find(thread_id);
  if (it != state_.threads.end()) {
    it->second.SetRegister(reg_name, value);
  }
}

void MockXBDMServer::SuspendThread(uint32_t thread_id) {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  auto it = state_.threads.find(thread_id);
  if (it != state_.threads.end()) {
    it->second.suspended = true;
  }
}

void MockXBDMServer::ResumeThread(uint32_t thread_id) {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  auto it = state_.threads.find(thread_id);
  if (it != state_.threads.end()) {
    it->second.suspended = false;
  }
}

void MockXBDMServer::AddBreakpoint(uint32_t address, bool hardware) {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  Breakpoint bp;
  bp.address = address;
  bp.hardware = hardware;
  state_.breakpoints[address] = bp;
}

void MockXBDMServer::RemoveBreakpoint(uint32_t address) {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  state_.breakpoints.erase(address);
}

bool MockXBDMServer::HasBreakpoint(uint32_t address) const {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  return state_.breakpoints.find(address) != state_.breakpoints.end();
}

void MockXBDMServer::AddModule(const std::string& name, uint32_t base_address,
                               uint32_t size) {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  Module module;
  module.name = name;
  module.base_address = base_address;
  module.size = size;
  module.timestamp = 0x12345678;
  module.checksum = 0x0abcdef9;
  state_.modules[name] = module;
}

void MockXBDMServer::RemoveModule(const std::string& name) {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  state_.modules.erase(name);
}

void MockXBDMServer::SetCommandHandler(const std::string& command,
                                       CommandHandler handler) {
  custom_handlers_[boost::to_lower_copy(command)] = std::move(handler);
}

void MockXBDMServer::RemoveCommandHandler(const std::string& command) {
  custom_handlers_.erase(boost::to_lower_copy(command));
}

}  // namespace xbdm_gdb_bridge::testing
