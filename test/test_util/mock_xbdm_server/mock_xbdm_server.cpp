#include "mock_xbdm_server.h"

#include <sys/fcntl.h>

#include <algorithm>
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

using namespace std::chrono_literals;

namespace xbdm_gdb_bridge::testing {

static constexpr uint8_t kTerminator[] = {'\r', '\n'};
static constexpr long kTerminatorLen =
    sizeof(kTerminator) / sizeof(kTerminator[0]);

constexpr const char kTagMockServer[] = "MockXBDM";
#define LOG_SERVER(lvl) \
  LOG_TAGGED(lvl, xbdm_gdb_bridge::testing::kTagMockServer)

static constexpr auto kDefaultNotificationDelay = 5ms;

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

template <typename Rep, typename Period>
bool TimedConnect(int sock, const struct sockaddr_in& addr,
                  const std::chrono::duration<Rep, Period>& timeout) {
  int flags = fcntl(sock, F_GETFL, 0);
  if (flags < 0) {
    LOG_SERVER(error) << "TimedConnect: fcntl(F_GETFL) failed: "
                      << strerror(errno);
    close(sock);
    return false;
  }

  if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
    LOG_SERVER(error) << "TimedConnect: fcntl(F_SETFL) failed: "
                      << strerror(errno);
    close(sock);
    return false;
  }

  int ret = connect(sock, reinterpret_cast<struct sockaddr const*>(&addr),
                    sizeof(addr));
  if (!ret) {
    return true;
  }

  if (ret < 0 && errno != EINPROGRESS) {
    LOG_SERVER(error) << "TimedConnect: connect failed " << errno;
    close(sock);
    return false;
  }

  fd_set write_fds;
  FD_ZERO(&write_fds);
  FD_SET(sock, &write_fds);

  struct timeval tv;
  auto secs = std::chrono::duration_cast<std::chrono::seconds>(timeout);
  auto usecs =
      std::chrono::duration_cast<std::chrono::microseconds>(timeout - secs);
  tv.tv_sec = secs.count();
  tv.tv_usec = usecs.count();

  ret = select(sock + 1, nullptr, &write_fds, nullptr, &tv);
  if (ret < 0) {
    LOG_SERVER(error) << "TimedConnect: select failed " << errno;
    close(sock);
    return false;
  }

  if (!ret) {
    LOG_SERVER(error) << "TimedConnect: connection attempt timed out";
    close(sock);
    return false;
  }

  if (fcntl(sock, F_SETFL, flags) < 0) {
    LOG_SERVER(error) << "TimedConnect: reset blocking failed: "
                      << strerror(errno);
    close(sock);
    return false;
  }

  return true;
}

std::shared_ptr<TCPConnection> CreateNotificationConnection(
    const std::string& name, const IPAddress& address) {
  int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0) {
    return nullptr;
  }

  if (!TimedConnect(sock, address.Address(), 500ms)) {
    LOG_SERVER(error) << "notification channel connect failed " << errno;
    return nullptr;
  }

  return std::make_shared<TCPConnection>(name + "_Notif", sock, address);
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

  select_thread_ = std::make_shared<SelectThread>("ST_MockXBDMSrv");
  task_queue_ = std::make_shared<TaskConnection>("ServerTaskQueue");
  select_thread_->AddConnection(task_queue_);
  server_ = std::make_shared<DelegatingServer>(
      "MockXBDMServerDS", [this](int sock, IPAddress& address) {
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

  ForEachClient([](ClientTransport& client) {
    client.Close();
    return true;
  });

  const std::lock_guard lock(clients_mutex_);
  clients_.clear();
}

void MockXBDMServer::AwaitQuiescence() {
  if (!running_) {
    return;
  }

  assert(select_thread_ && "May not be called before Start");
  select_thread_->AwaitQuiescence();
}

void MockXBDMServer::ForEachClient(std::function<bool(ClientTransport&)> f) {
  const std::lock_guard lock(clients_mutex_);
  std::erase_if(clients_, [&f](std::shared_ptr<ClientTransport> client) {
    return !client || !f(*client);
  });
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

void MockXBDMServer::SendString(ClientTransport& transport,
                                const std::string& str) const {
  transport.Send(str);
}

void MockXBDMServer::SendBinaryResponse(ClientTransport& transport,
                                        const std::vector<uint8_t>& binary) {
  SendResponse(transport, OK_BINARY_RESPONSE);
  transport.Send(binary);
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

  const std::lock_guard lock(transport.ReadLock());
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
    std::string trimmed = Trim(command);
    if (trimmed.empty()) {
      return;
    }

    std::string rdcp_command;
    std::string params_str;

    size_t space_pos = trimmed.find(' ');
    if (space_pos != std::string::npos) {
      rdcp_command = boost::to_lower_copy(trimmed.substr(0, space_pos));
      params_str = trimmed.substr(space_pos + 1);
    } else {
      rdcp_command = boost::to_lower_copy(trimmed);
    }

    if (!ProcessCommand(transport, rdcp_command, params_str)) {
      transport.DropReceiveBuffer();
      transport.Close();
      return;
    }

    {
      std::lock_guard handled_lock(after_handlers_mutex_);
      auto post_handler = after_handlers_.find(rdcp_command);
      if (post_handler != after_handlers_.end()) {
        post_handler->second(params_str);
      }
    }

    buffer += packet_len;
    message_end = ParseMessage(buffer, buffer_end);
  }

  transport.ShiftReadBuffer(bytes_processed);
}

bool MockXBDMServer::ProcessCommand(ClientTransport& client,
                                    const std::string& command,
                                    const std::string& params_str) {
  auto it = custom_handlers_.find(command);
  if (it != custom_handlers_.end()) {
    return it->second(client, params_str);
  }

  // Built-in command handlers
#define HANDLE(cmd, handler)                    \
  if (command == cmd) {                         \
    return Handle##handler(client, params_str); \
  }

  HANDLE("break", Break)
  HANDLE("bye", Bye)
  HANDLE("continue", Continue)
  HANDLE("debugger", Debugger)
  HANDLE("getcontext", GetContext)
  HANDLE("getmem2", GetMem2)
  HANDLE("setmem", SetMem)
  HANDLE("go", Go)
  HANDLE("isstopped", IsStopped)
  HANDLE("modules", Modules)
  HANDLE("nostopon", NoStopOn)
  HANDLE("notifyat", NotifyAt)
  HANDLE("reboot", Reboot)
  HANDLE("resume", Resume)
  HANDLE("stopon", StopOn)
  HANDLE("suspend", Suspend)
  HANDLE("threadinfo", ThreadInfo)
  HANDLE("threads", Threads)
  HANDLE("title", Title)
  HANDLE("walkmem", WalkMemory)

#undef HANDLE

  std::stringstream error_message_builder;
  error_message_builder << "Command '" << command << "' unimplemented";
  auto err = error_message_builder.str();
  LOG_SERVER(warning) << err;
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

  auto notification_address = client.Address().WithPort(port.value());

  if (drop) {
    std::lock_guard lock(notification_mutex_);
    std::erase_if(notification_connections_,
                  [&notification_address](
                      const std::pair<std::pair<std::string, IPAddress>,
                                      std::shared_ptr<TCPConnection>>& entry) {
                    auto& addr = entry.first.second;
                    return addr == notification_address;
                  });
  } else {
    auto key = std::make_pair(client.Name(), notification_address);
    task_queue_->Post([this, key]() {
      {
        auto connection = CreateNotificationConnection(key.first, key.second);
        std::lock_guard lock(notification_mutex_);
        notification_connections_[key] = connection;
        select_thread_->AddConnection(connection);

        connection->Send(GetExecutionStateNotificationMessage());
      }
    });
  }

  SendResponse(client, OK);

  return true;
}

bool MockXBDMServer::HandleDebugger(ClientTransport& client,
                                    const std::string& parameters) {
  RDCPMapResponse params(parameters);

  if (params.HasKey("connect")) {
    if (state_.is_debugable_) {
      SendResponse(client, OK);
    } else {
      SendResponse(client, ERR_NOT_DEBUGGABLE);
    }

    if (state_.awaiting_debugger_) {
      state_.awaiting_debugger_ = false;
    }

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
                                   const std::string&) {
  SendResponse(client, OK_MULTILINE_RESPONSE, "thread list follows");
  for (auto& thread : state_.threads) {
    SendStringWithTerminator(client, std::to_string(thread.first));
  }
  SendMultilineTerminator(client);

  return true;
}

bool MockXBDMServer::HandleThreadInfo(ClientTransport& client,
                                      const std::string& parameters) {
  RDCPMapResponse params(parameters);

  auto thread_id = params.GetOptionalDWORD("thread");
  if (!thread_id.has_value()) {
    SendResponse(client, ERR_UNEXPECTED, "Missing thread");
    return true;
  }

  std::lock_guard lock(state_mutex_);
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
                                   const std::string&) {
  SendResponse(client, OK_MULTILINE_RESPONSE);
  std::lock_guard lock(state_mutex_);

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
                                      const std::string&) {
  SendResponse(client, OK_MULTILINE_RESPONSE, "Valid virtual addresses follow");
  std::lock_guard lock(state_mutex_);

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

bool MockXBDMServer::HandleReboot(ClientTransport& client,
                                  const std::string& parameters) {
  RDCPMapResponse params(parameters);

  bool warm = params.HasKey("warm");
  bool nodebug = params.HasKey("nodebug");

  // TODO: Emulate behavior
  if (warm || nodebug) {
    LOG_SERVER(debug) << "Reboot param not implemented, ignoring";
  }

  SendResponse(client, OK);

  {
    std::lock_guard lock(state_mutex_);
    state_.boot_actions.wait_for_debugger = params.HasKey("wait");
    state_.boot_actions.halt = params.HasKey("stop");
  }

  task_queue_->Post([this]() { PerformReboot(); });

  return true;
}

bool MockXBDMServer::HandleBye(ClientTransport& client, const std::string&) {
  LOG_SERVER(trace) << "Received bye message from " << client.Address();
  return false;
}

bool MockXBDMServer::HandleContinue(ClientTransport& client,
                                    const std::string& parameters) {
  RDCPMapResponse params(parameters);

  auto thread_id = params.GetOptionalDWORD("thread");
  if (!thread_id.has_value()) {
    SendResponse(client, ERR_UNEXPECTED, "Missing required thread ID");
    return true;
  }

  std::lock_guard lock(state_mutex_);
  auto entry = state_.threads.find(thread_id.value());
  if (entry == state_.threads.end()) {
    SendResponse(client, ERR_NO_SUCH_THREAD);
    return true;
  }

  auto& thread = entry->second;
  if (!thread.stopped) {
    SendResponse(client, ERR_NOT_STOPPED);
    return true;
  }

  thread.stopped = false;
  SendResponse(client, OK);
  return true;
}

bool MockXBDMServer::HandleTitle(ClientTransport& client,
                                 const std::string& parameters) {
  RDCPMapResponse params(parameters);

  auto path = params.GetString("dir");
  if (path.empty()) {
    SendResponse(client, ERR_ACCESS_DENIED, "Missing required dir param");
    return true;
  }

  std::lock_guard lock(state_mutex_);
  auto& info = state_.load_on_boot_info;

  info.name = params.GetString("name", "default.xbe");
  info.path = path;
  info.command_line = params.GetString("cmdline");
  info.persistent = params.HasKey("persist");

  SendResponse(client, OK);
  return true;
}

bool MockXBDMServer::HandleGetContext(ClientTransport& client,
                                      const std::string& parameters) {
  RDCPMapResponse params(parameters);
  auto thread_id = params.GetOptionalDWORD("thread");
  if (!thread_id.has_value()) {
    SendResponse(client, ERR_UNEXPECTED, "Missing thread");
    return true;
  }

  std::lock_guard lock(state_mutex_);
  auto entry = state_.threads.find(thread_id.value());
  if (entry == state_.threads.end()) {
    SendResponse(client, ERR_NO_SUCH_THREAD);
    return true;
  }

  SendResponse(client, OK_MULTILINE_RESPONSE, "context follows");

  auto& thread = entry->second;

  std::string response;
  auto append = [&](const char* key, const std::optional<uint32_t>& val) {
    if (val.has_value()) {
      char buf[32];
      snprintf(buf, sizeof(buf), "0x%x", val.value());
      if (!response.empty()) {
        response += " ";
      }
      response += key;
      response += "=";
      response += buf;
    }
  };

  append("Eax", thread.eax);
  append("Ebx", thread.ebx);
  append("Ecx", thread.ecx);
  append("Edx", thread.edx);
  append("Esi", thread.esi);
  append("Edi", thread.edi);
  append("Ebp", thread.ebp);
  append("Esp", thread.esp);
  append("Eip", thread.eip);
  append("EFlags", thread.eflags);
  append("Cr0NpxState", thread.cr0_npx_state);

  SendStringWithTerminator(client, response);
  SendMultilineTerminator(client);

  return true;
}

bool MockXBDMServer::HandleGetMem2(ClientTransport& client,
                                   const std::string& parameters) {
  RDCPMapResponse params(parameters);

  auto address = params.GetOptionalDWORD("addr");
  if (!address.has_value()) {
    SendResponse(client, ERR_UNEXPECTED, "Missing addr");
    return true;
  }

  auto length = params.GetOptionalDWORD("length");
  if (!length.has_value()) {
    SendResponse(client, ERR_UNEXPECTED, "Missing length");
    return true;
  }

  // TODO: Investigate behavior in actual XBDM when requesting regions with
  //       gaps.

  std::vector<uint8_t> data;

  std::lock_guard lock(state_mutex_);
  state_.ReadVirtualMemory(data, address.value(), length.value());

  SendBinaryResponse(client, data);
  return true;
}

bool MockXBDMServer::HandleSetMem(ClientTransport& client,
                                  const std::string& parameters) {
  LOG_SERVER(trace) << "SetMem with parameters: " << parameters;
  RDCPMapResponse params(parameters);

  auto address = params.GetOptionalDWORD("addr");
  if (!address.has_value()) {
    SendResponse(client, ERR_UNEXPECTED, "Missing addr");
    return true;
  }

  auto value = params.GetString("data");
  if (value.empty()) {
    SendResponse(client, ERR_UNEXPECTED, "Missing value");
    return true;
  }

  auto data = HexToBytes(value);
  state_.WriteVirtualMemory(address.value(), data);

  SendResponse(client, OK);
  return true;
}

bool MockXBDMServer::HandleGo(ClientTransport& client, const std::string&) {
  auto previous_state = SetExecutionState(S_STARTED);
  if (previous_state == S_STARTED) {
    SendResponse(client, ERR_UNEXPECTED, "Not stopped");
  } else {
    SendResponse(client, OK);

    std::lock_guard lock(state_mutex_);
    if (state_.IsStartingUp()) {
      task_queue_->Post([this]() { AdvancePhase(); });
    }
  }

  return true;
}

bool MockXBDMServer::HandleBreak(ClientTransport& client,
                                 const std::string& parameters) {
  RDCPMapResponse params(parameters);

  if (params.HasKey("clearall")) {
    std::lock_guard lock(state_mutex_);
    state_.breakpoints.clear();
    SendResponse(client, OK);
    return true;
  }

  if (params.HasKey("start")) {
    std::lock_guard lock(state_mutex_);
    state_.boot_actions.break_at_first_thread = true;
    SendResponse(client, OK);
    return true;
  }

  auto read_address = params.GetOptionalDWORD("read");
  auto write_address = params.GetOptionalDWORD("write");
  auto execute_address = params.GetOptionalDWORD("execute");
  auto addr_address = params.GetOptionalDWORD("addr");

  uint32_t address;
  Breakpoint::Type type;

  if (read_address.has_value()) {
    address = read_address.value();
    type = Breakpoint::Type::READ;
  } else if (write_address.has_value()) {
    address = write_address.value();
    type = Breakpoint::Type::WRITE;
  } else if (execute_address.has_value()) {
    address = execute_address.value();
    type = Breakpoint::Type::EXECUTE;
  } else if (addr_address.has_value()) {
    address = addr_address.value();
    type = Breakpoint::Type::EXECUTE;
  } else {
    SendResponse(client, ERR_UNEXPECTED, "Missing breakpoint type");
    return true;
  }

  if (params.HasKey("clear")) {
    // TODO: Check that the breakpoint types match.
    RemoveBreakpoint(address);
  } else {
    AddBreakpoint(address, type);
  }

  SendResponse(client, OK);
  return true;
}

bool MockXBDMServer::HandleIsStopped(ClientTransport& client,
                                     const std::string& parameters) {
  RDCPMapResponse params(parameters);

  auto thread_id = params.GetOptionalDWORD("thread");
  if (!thread_id.has_value()) {
    SendResponse(client, ERR_UNEXPECTED, "Missing required thread ID");
    return true;
  }

  std::lock_guard lock(state_mutex_);
  auto entry = state_.threads.find(thread_id.value());
  if (entry == state_.threads.end()) {
    SendResponse(client, ERR_NO_SUCH_THREAD);
    return true;
  }

  const auto& thread = entry->second;
  if (!thread.stopped) {
    SendResponse(client, ERR_NOT_STOPPED);
    return true;
  }

  std::stringstream response;
  response << thread.stop_reason << " addr=" << std::hex
           << thread.eip.value_or(0) << " thread=" << std::dec << thread.id;
  SendResponse(client, OK, response.str());
  return true;
}

bool MockXBDMServer::HandleStopOn(ClientTransport& client,
                                  const std::string& parameters) {
  RDCPMapResponse params(parameters);

  // TODO: See if it is an error to pass no options at all.

  std::lock_guard lock(state_mutex_);
  if (params.HasKey("all")) {
    state_.stop_events.SetAll();
  } else {
    if (params.HasKey("createthread")) {
      state_.stop_events.create_thread = true;
    }
    if (params.HasKey("fce")) {
      state_.stop_events.first_chance_exception = true;
    }
    if (params.HasKey("debugstr")) {
      state_.stop_events.debug_str = true;
    }
    if (params.HasKey("stacktrace")) {
      state_.stop_events.stack_trace = true;
    }
  }

  SendResponse(client, OK);
  return true;
}

bool MockXBDMServer::HandleNoStopOn(ClientTransport& client,
                                    const std::string& parameters) {
  RDCPMapResponse params(parameters);

  // TODO: See if it is an error to pass no options at all.

  std::lock_guard lock(state_mutex_);
  if (params.HasKey("all")) {
    state_.stop_events.ClearAll();
  } else {
    if (params.HasKey("createthread")) {
      state_.stop_events.create_thread = false;
    }
    if (params.HasKey("fce")) {
      state_.stop_events.first_chance_exception = false;
    }
    if (params.HasKey("debugstr")) {
      state_.stop_events.debug_str = false;
    }
    if (params.HasKey("stacktrace")) {
      state_.stop_events.stack_trace = false;
    }
  }

  SendResponse(client, OK);
  return true;
}

bool MockXBDMServer::HandleSuspend(ClientTransport& client,
                                   const std::string& parameters) {
  RDCPMapResponse params(parameters);
  auto thread_id = params.GetOptionalDWORD("thread");
  if (!thread_id.has_value()) {
    SendResponse(client, ERR_UNEXPECTED, "Missing thread");
    return true;
  }

  std::lock_guard lock(state_mutex_);
  auto entry = state_.threads.find(thread_id.value());
  if (entry == state_.threads.end()) {
    SendResponse(client, ERR_NO_SUCH_THREAD);
    return true;
  }

  auto& thread = entry->second;

  thread.suspended = true;
  SendResponse(client, OK);

  return true;
}

bool MockXBDMServer::HandleResume(ClientTransport& client,
                                  const std::string& parameters) {
  RDCPMapResponse params(parameters);
  auto thread_id = params.GetOptionalDWORD("thread");
  if (!thread_id.has_value()) {
    SendResponse(client, ERR_UNEXPECTED, "Missing thread");
    return true;
  }

  std::lock_guard lock(state_mutex_);
  auto entry = state_.threads.find(thread_id.value());
  if (entry == state_.threads.end()) {
    SendResponse(client, ERR_NO_SUCH_THREAD);
    return true;
  }

  auto& thread = entry->second;

  thread.suspended = false;
  SendResponse(client, OK);

  return true;
}

void MockXBDMServer::SetMemoryRegion(uint32_t address,
                                     const std::vector<uint8_t>& data) {
  std::lock_guard lock(state_mutex_);
  MemoryRegion region{
      .base_address = address,
      .size = static_cast<uint32_t>(data.size()),
      .data = data,
  };

  state_.memory_regions[address] = region;
}

void MockXBDMServer::ClearMemoryRegion(uint32_t address) {
  std::lock_guard lock(state_mutex_);
  state_.memory_regions.erase(address);
}

std::vector<uint8_t> MockXBDMServer::GetMemoryRegion(uint32_t address,
                                                     size_t length) {
  // Find the memory region containing this address
  // TODO: Support access bridging regions.
  for (const auto& [base, region] : state_.memory_regions) {
    if (address >= base && address + length <= base + region.data.size()) {
      size_t offset = address - base;
      return {region.data.begin() + offset,
              region.data.begin() + offset + length};
    }
  }
  return {};
}

uint32_t MockXBDMServer::AddThread(const std::string& name, uint32_t eip,
                                   uint32_t base, uint32_t start) {
  std::lock_guard lock(state_mutex_);
  uint32_t thread_id = state_.next_thread_id++;

  SimulatedThread thread;
  thread.id = thread_id;
  thread.eip = eip;
  thread.eflags = 0x202;
  thread.base = base;
  thread.start = start;

  state_.threads[thread_id] = thread;

  if (state_.current_thread_id == 0) {
    state_.current_thread_id = thread_id;
  }

  return thread_id;
}

void MockXBDMServer::RemoveThread(uint32_t thread_id) {
  std::lock_guard lock(state_mutex_);
  state_.threads.erase(thread_id);

  if (state_.current_thread_id == thread_id && !state_.threads.empty()) {
    state_.current_thread_id = state_.threads.begin()->first;
  }
}

void MockXBDMServer::SetThreadRegister(uint32_t thread_id,
                                       const std::string& reg_name,
                                       uint32_t value) {
  std::lock_guard lock(state_mutex_);
  auto it = state_.threads.find(thread_id);
  if (it != state_.threads.end()) {
    it->second.SetRegister(reg_name, value);
  }
}

void MockXBDMServer::SuspendThread(uint32_t thread_id) {
  std::lock_guard lock(state_mutex_);
  auto it = state_.threads.find(thread_id);
  if (it != state_.threads.end()) {
    it->second.suspended = true;
  }
}

void MockXBDMServer::ResumeThread(uint32_t thread_id) {
  std::lock_guard lock(state_mutex_);
  auto it = state_.threads.find(thread_id);
  if (it != state_.threads.end()) {
    it->second.suspended = false;
  }
}

void MockXBDMServer::AddBreakpoint(uint32_t address, Breakpoint::Type type) {
  std::lock_guard lock(state_mutex_);
  state_.breakpoints.emplace(std::piecewise_construct,
                             std::forward_as_tuple(address),
                             std::forward_as_tuple(address, type));
}

void MockXBDMServer::RemoveBreakpoint(uint32_t address) {
  std::lock_guard lock(state_mutex_);
  state_.breakpoints.erase(address);
}

bool MockXBDMServer::HasBreakpoint(uint32_t address) const {
  std::lock_guard lock(state_mutex_);
  return state_.breakpoints.find(address) != state_.breakpoints.end();
}

void MockXBDMServer::AddModule(const std::string& name, uint32_t base_address,
                               uint32_t size) {
  Module module{
      .name = name,
      .base_address = base_address,
      .size = size,
      .timestamp = 0x12345678,
      .checksum = 0x0abcdef9,
  };

  std::lock_guard lock(state_mutex_);
  state_.modules[name] = module;
}

void MockXBDMServer::RemoveModule(const std::string& name) {
  std::lock_guard lock(state_mutex_);
  state_.modules.erase(name);
}

void MockXBDMServer::AddXbeSection(const std::string& name,
                                   uint32_t base_address, uint32_t size,
                                   uint32_t index, uint32_t flags) {
  XbeSection section{.name = name,
                     .base_address = base_address,
                     .size = size,
                     .index = index,
                     .flags = flags};

  std::lock_guard lock(state_mutex_);
  state_.xbe_sections[name] = section;
}

void MockXBDMServer::RemoveXbeSection(const std::string& name) {
  std::lock_guard lock(state_mutex_);
  state_.xbe_sections.erase(name);
}

void MockXBDMServer::AddRegion(uint32_t base_address, uint32_t size,
                               uint32_t protect) {
  MemoryRegion region{
      .base_address = base_address, .size = size, .protect = protect};

  std::lock_guard lock(state_mutex_);
  state_.memory_regions[base_address] = region;
}

void MockXBDMServer::AddRegion(uint32_t base_address,
                               const std::vector<uint8_t>& data,
                               uint32_t protect) {
  std::lock_guard lock(state_mutex_);
  AddRegion(base_address, data.size(), protect);
  state_.memory_regions[base_address].data = data;
}

void MockXBDMServer::RemoveRegion(uint32_t base_address) {
  std::lock_guard lock(state_mutex_);
  state_.memory_regions.erase(base_address);
}

ExecutionState MockXBDMServer::SetExecutionState(ExecutionState state) {
  auto ret = state_.execution_state.exchange(state);
  if (ret != state) {
    std::string notification = GetExecutionStateNotificationMessage();
    task_queue_->Post([this, state, notification]() {
      SendNotification(notification);

      std::lock_guard lock(execution_state_handlers_mutex_);
      for (const auto& entry : execution_state_handlers_) {
        const auto& state_handler_pair = entry.second;
        if (state == state_handler_pair.first) {
          state_handler_pair.second();
        }
      }
    });
  }
  return ret;
}

int MockXBDMServer::AddExecutionStateCallback(ExecutionState state,
                                              ExecutionStateHandler handler) {
  std::lock_guard lock(execution_state_handlers_mutex_);
  execution_state_handlers_.emplace(next_execution_state_handler_id_,
                                    std::make_pair(state, handler));
  return next_execution_state_handler_id_++;
}

void MockXBDMServer::RemoveExecutionStateCallback(int token) {
  std::lock_guard lock(execution_state_handlers_mutex_);
  execution_state_handlers_.erase(token);
}

void MockXBDMServer::SetAfterCommandHandler(const std::string& command,
                                            AfterCommandHandler handler) {
  std::lock_guard lock(after_handlers_mutex_);
  after_handlers_.emplace(command, handler);
}

void MockXBDMServer::RemoveAfterCommandHandler(const std::string& command) {
  std::lock_guard lock(after_handlers_mutex_);
  after_handlers_.erase(command);
}

std::string MockXBDMServer::GetExecutionStateNotificationMessage() const {
  ExecutionState current_state = state_.execution_state;
  switch (current_state) {
    default:
    case S_INVALID:
      assert(!"Invalid execution state");

    case S_STARTED:
      return "execution started\r\n";

    case S_STOPPED:
      return "execution stopped\r\n";

    case S_PENDING:
      return "execution pending\r\n";

    case S_REBOOTING:
      return "execution rebooting\r\n";
  }
}

void MockXBDMServer::SetCommandHandler(const std::string& command,
                                       CommandHandler handler) {
  custom_handlers_[boost::to_lower_copy(command)] = std::move(handler);
}

void MockXBDMServer::RemoveCommandHandler(const std::string& command) {
  custom_handlers_.erase(boost::to_lower_copy(command));
}

void MockXBDMServer::PerformReboot() {
  {
    std::lock_guard lock(state_mutex_);
    state_.execution_state = S_REBOOTING;
    state_.execution_phase = MockXboxState::TitleExecutionPhase::BOOTING;
  }

  SendNotificationAndClose("execution rebooting\r\n");

  task_queue_->PostDelayed(1ms, [this]() {
    ReconnectNotificationChannels();

    task_queue_->PostDelayed(kDefaultNotificationDelay, [this]() {
      std::lock_guard lock(state_mutex_);
      state_.ResetThreadStates();
      AdvancePhase();
    });
  });
}

void MockXBDMServer::SimulateBootToDashboard() {
  std::lock_guard lock(state_mutex_);
  auto& info = state_.load_on_boot_info;
  info.name = "default.xbe";
  info.path = "//DashboardDrive/";
  info.command_line = "";
  info.persistent = false;

  PerformReboot();
}

void MockXBDMServer::AdvancePhase() {
  std::lock_guard lock(state_mutex_);
  auto current_phase = state_.execution_phase;
  switch (current_phase) {
    case MockXboxState::TitleExecutionPhase::BOOTING:
      AdvancePhaseStart();
      break;
    case MockXboxState::TitleExecutionPhase::START:
      AdvancePhaseLoadModules();
      break;
    case MockXboxState::TitleExecutionPhase::LOAD_MODULES:
      AdvancePhaseLoadSections();
      break;
    case MockXboxState::TitleExecutionPhase::LOAD_SECTIONS:
      AdvancePhaseStartFirstThread();
      break;
    case MockXboxState::TitleExecutionPhase::START_FIRST_THREAD:
      AdvancePhaseStartThreads();
      break;
    case MockXboxState::TitleExecutionPhase::START_THREADS:
      AdvancePhaseRunning();
      break;
    case MockXboxState::TitleExecutionPhase::RUNNING:
      LOG_SERVER(debug) << "MockXboxState RUNNING";
      break;
  }
}

void MockXBDMServer::AdvancePhaseStart() {
  std::lock_guard lock(state_mutex_);
  state_.execution_phase = MockXboxState::TitleExecutionPhase::START;

  if (state_.boot_actions.wait_for_debugger) {
    state_.awaiting_debugger_ = true;
    SetExecutionState(S_PENDING);
  } else {
    SetExecutionState(state_.boot_actions.halt ? S_STOPPED : S_STARTED);
  }

  if (state_.execution_state == S_STARTED) {
    task_queue_->Post([this]() { AdvancePhase(); });
  }
}

void MockXBDMServer::AdvancePhaseLoadModules() {
  std::lock_guard lock(state_mutex_);
  state_.execution_phase = MockXboxState::TitleExecutionPhase::LOAD_SECTIONS;

  for (const auto& module : state_.modules) {
    PostModuleLoadNotification(module.second);
  }

  if (state_.execution_state == S_STARTED) {
    task_queue_->Post([this]() { AdvancePhase(); });
  }
}

void MockXBDMServer::AdvancePhaseLoadSections() {
  std::lock_guard lock(state_mutex_);
  state_.execution_phase = MockXboxState::TitleExecutionPhase::LOAD_SECTIONS;

  for (const auto& section : state_.xbe_sections) {
    PostSectionLoadNotification(section.second);
  }

  if (state_.execution_state == S_STARTED) {
    task_queue_->Post([this]() { AdvancePhase(); });
  }
}

void MockXBDMServer::AdvancePhaseStartFirstThread() {
  std::lock_guard lock(state_mutex_);
  state_.execution_phase =
      MockXboxState::TitleExecutionPhase::START_FIRST_THREAD;

  const auto& entry = state_.threads.begin();
  assert(entry != state_.threads.end() && "No initial thread defined");

  auto& thread = entry->second;
  PostThreadCreateNotification(thread);
  thread.created = true;

  if (state_.boot_actions.break_at_first_thread ||
      state_.stop_events.create_thread) {
    SimulateExecutionBreakpoint(thread.start, thread.id);
  }

  if (state_.execution_state == S_STARTED) {
    task_queue_->Post([this]() { AdvancePhase(); });
  }
}

void MockXBDMServer::AdvancePhaseStartThreads() {
  std::lock_guard lock(state_mutex_);
  state_.execution_phase = MockXboxState::TitleExecutionPhase::START_THREADS;

  for (auto& entry : state_.threads) {
    auto& thread = entry.second;
    if (thread.created) {
      continue;
    }
    PostThreadCreateNotification(thread);
    thread.created = true;

    if (state_.stop_events.create_thread) {
      SimulateExecutionBreakpoint(thread.start, thread.id);
      return;
    }
  }

  if (state_.execution_state == S_STARTED) {
    task_queue_->Post([this]() { AdvancePhase(); });
  }
}

void MockXBDMServer::AdvancePhaseRunning() {
  std::lock_guard lock(state_mutex_);
  state_.execution_phase = MockXboxState::TitleExecutionPhase::RUNNING;

  if (state_.execution_state == S_STARTED) {
    task_queue_->Post([this]() { AdvancePhase(); });
  }
}

bool MockXBDMServer::SimulateExecutionBreakpoint(uint32_t address,
                                                 uint32_t thread_id) {
  std::lock_guard lock(state_mutex_);
  SimulatedThread* thread = nullptr;
  if (thread_id) {
    auto entry = state_.threads.find(thread_id);
    if (entry == state_.threads.end()) {
      return false;
    }

    thread = &entry->second;
  } else {
    for (auto& entry : state_.threads) {
      if (entry.second.ContainsAddress(address)) {
        thread = &entry.second;
        break;
      }
    }
  }

  assert(thread && "Failed to identify appropriate thread");
  if (!thread) {
    return false;
  }

  thread->stopped = true;
  thread->stop_reason = "break";
  SetExecutionState(S_STOPPED);

  std::stringstream notification;
  notification << "break addr=0x" << std::hex << address
               << " thread=" << std::dec << thread->id << " stop\r\n";
  PostNotification(notification.str());
  return true;
}

bool MockXBDMServer::SimulateReadWatchpoint(uint32_t address,
                                            uint32_t thread_id, bool stop) {
  return PostWatchpointNotification("read", address, thread_id, stop);
}

bool MockXBDMServer::SimulateWriteWatchpoint(uint32_t address,
                                             uint32_t thread_id, bool stop) {
  return PostWatchpointNotification("write", address, thread_id, stop);
}

bool MockXBDMServer::SimulateExecuteWatchpoint(uint32_t address,
                                               uint32_t thread_id, bool stop) {
  return PostWatchpointNotification("execute", address, thread_id, stop);
}

bool MockXBDMServer::PostWatchpointNotification(const std::string& type,
                                                uint32_t address,
                                                uint32_t thread_id, bool stop) {
  std::lock_guard lock(state_mutex_);
  SimulatedThread* thread = nullptr;
  if (thread_id) {
    auto entry = state_.threads.find(thread_id);
    if (entry == state_.threads.end()) {
      return false;
    }
    thread = &entry->second;
  } else {
    for (auto& entry : state_.threads) {
      if (entry.second.ContainsAddress(address)) {
        thread = &entry.second;
        break;
      }
    }
  }

  assert(thread && "Failed to identify appropriate thread");
  if (!thread) {
    return false;
  }

  std::stringstream notification;
  notification << "data thread=" << std::dec << thread->id << " addr=0x"
               << std::hex << thread->eip.value_or(0) << " " << type << "=0x"
               << address;

  if (stop) {
    notification << " stop";
    thread->stopped = true;
    thread->stop_reason = "data";
    SetExecutionState(S_STOPPED);
  }

  notification << "\r\n";
  PostNotification(notification.str());

  return true;
}

void MockXBDMServer::PostModuleLoadNotification(const Module& module) {
  std::stringstream notification;

  // TODO: Model handling of thread local storage and xbe flags.
  notification << "modload name=\"" << module.name << "\" base=" << std::hex
               << module.base_address << " size=" << module.size
               << " check=" << module.checksum
               << " timestamp=" << module.timestamp << " tls xbe" << "\r\n";

  PostNotification(notification.str());
}

void MockXBDMServer::PostSectionLoadNotification(const XbeSection& section) {
  std::stringstream notification;

  notification << "sectload name=\"" << section.name << "\" base=" << std::hex
               << section.base_address << " size=" << section.size << std::dec
               << " index=" << section.index << " flags=" << section.flags
               << "\r\n";

  PostNotification(notification.str());
}

void MockXBDMServer::PostThreadCreateNotification(
    const SimulatedThread& thread) {
  std::stringstream notification;

  notification << "create thread=" << thread.id << " start=" << std::hex
               << thread.start << "\r\n";

  PostNotification(notification.str());
}

void MockXBDMServer::ReconnectNotificationChannels() {
  std::lock_guard lock(notification_mutex_);
  std::map<std::pair<std::string, IPAddress>, std::shared_ptr<TCPConnection>>
      new_connections;

  for (const auto& entry : notification_connections_) {
    auto connection =
        CreateNotificationConnection(entry.first.first, entry.first.second);
    if (!connection) {
      LOG_SERVER(error) << "Failed to reconnect notification channel to "
                        << entry.first.first << " : " << entry.first.second;
      continue;
    }

    new_connections[entry.first] = connection;
    select_thread_->AddConnection(connection);
    connection->Send("hello\r\n");
  }

  new_connections.swap(notification_connections_);
}

void MockXBDMServer::PostNotification(const std::string& message) {
  task_queue_->Post([this, message]() { SendNotification(message); });
}

void MockXBDMServer::SendNotification(const std::string& message) const {
  std::lock_guard lock(notification_mutex_);
  for (auto& entry : notification_connections_) {
    auto& connection = entry.second;

    connection->Send(message);
  }
}

void MockXBDMServer::SendNotificationAndClose(const std::string& message) {
  std::lock_guard lock(notification_mutex_);
  for (auto& entry : notification_connections_) {
    auto& connection = entry.second;

    connection->Send(message);
    connection->FlushAndClose();
    connection.reset();
  }
}

}  // namespace xbdm_gdb_bridge::testing
