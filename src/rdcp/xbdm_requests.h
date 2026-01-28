#ifndef XBDM_GDB_BRIDGE_XBDM_REQUESTS_H
#define XBDM_GDB_BRIDGE_XBDM_REQUESTS_H

#include <arpa/inet.h>

#include <boost/optional/optional_io.hpp>
#include <optional>
#include <ostream>
#include <set>
#include <string>

#include "net/ip_address.h"
#include "rdcp/rdcp_processed_request.h"
#include "rdcp/rdcp_request.h"
#include "rdcp/rdcp_response.h"
#include "rdcp/rdcp_status_code.h"
#include "rdcp/types/memory_region.h"
#include "rdcp/types/module.h"
#include "rdcp/types/thread_context.h"
#include "rdcp/xbdm_stop_reasons.h"
#include "util/parsing.h"

// The maximum size of an RDCP command string.
#define MAXIMUM_SEND_LENGTH 512

/**
 * Command: adminpw
 *
 * Sets the administrator password for the Xbox.
 *
 * Usage: adminpw passwd=0q<high_part><low_part>
 */
struct AdminPW : public RDCPProcessedRequest {
  AdminPW(uint32_t high_part, uint32_t low_part)
      : RDCPProcessedRequest("adminpw") {
    SetData(" passwd=");
    AppendHexString((static_cast<uint64_t>(high_part) << 32) | low_part);
  }
};

/**
 * Command: authuser [name="..." | admin] {passwd=0q... | resp=0q...}
 *
 * Authenticates a user.
 */
struct AuthUser : public RDCPProcessedRequest {
  // Named user authentication (supports both passwd and resp via high/low
  // parts)
  AuthUser(const std::string& username, uint32_t high, uint32_t low,
           bool is_passwd = true)
      : RDCPProcessedRequest("authuser") {
    SetData(" name=\"");
    AppendData(username);
    AppendData("\" ");
    AppendData(is_passwd ? "passwd=" : "resp=");
    AppendHexString((static_cast<uint64_t>(high) << 32) | low);
  }

  // Admin authentication (uses admin parameter instead of name, only supports
  // resp)
  AuthUser(uint32_t high, uint32_t low) : RDCPProcessedRequest("authuser") {
    SetData(" admin resp=");
    AppendHexString((static_cast<uint64_t>(high) << 32) | low);
  }
};

/**
 * Command: altaddr
 *
 * Retrieves the alternative IP address of the console. This is often used
 * for debug communication on a secondary network interface.
 */
struct AltAddr : public RDCPProcessedRequest {
  AltAddr() : RDCPProcessedRequest("altaddr") {}

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    auto parsed = RDCPMapResponse(response->Data());
    address = htonl(parsed.GetDWORD("addr"));

    char buf[64] = {0};
    if (inet_ntop(AF_INET, &address, buf, 64)) {
      address_string = buf;
    } else {
      address_string.clear();
    }
  }

  uint32_t address{0};
  std::string address_string;
};

/**
 * Command: boxid
 *
 * Retrieves the unique identifier (Box ID) of the console, if locked.
 */
struct BoxID : public RDCPProcessedRequest {
  BoxID() : RDCPProcessedRequest("boxid") {}

  // TODO: Parse response.
};

/**
 * Command: break
 *
 * Base class for breakpoint operations. The 'break' command is used to set
 * and clear breakpoints (execution, read, write).
 */
struct BreakBase_ : public RDCPProcessedRequest {
  BreakBase_() : RDCPProcessedRequest("break") {}
};

/**
 * Command: break now
 *
 * Forces an immediate breakpoint.
 */
struct BreakNow : public BreakBase_ {
  BreakNow() : BreakBase_() { SetData(" now"); }
};

/**
 * Command: break start
 *
 * Sets a breakpoint at the entry point of the title (On Start).
 */
struct BreakAtStart : public BreakBase_ {
  BreakAtStart() : BreakBase_() { SetData(" start"); }
};

/**
 * Command: break clearall
 *
 * Clears all currently set breakpoints.
 */
struct BreakClearAll : public BreakBase_ {
  BreakClearAll() : BreakBase_() { SetData(" clearall"); }
};

/**
 * Command: break addr=... [clear]
 *
 * Sets or clears a breakpoint at a specific memory address.
 */
struct BreakAddress : public BreakBase_ {
  explicit BreakAddress(uint32_t address, bool clear = false) : BreakBase_() {
    if (clear) {
      SetData(" clear");
    }
    AppendData(" addr=");
    AppendHexString(address);
  }
};

/**
 * Command: break <type>=... [size=...] [clear]
 *
 * Base class for data breakpoints (read, write, execute) on a range of memory.
 */
struct BreakRange_ : public BreakBase_ {
  BreakRange_(const std::string& type, uint32_t address, uint32_t size = 0,
              bool clear = false)
      : BreakBase_() {
    if (clear) {
      SetData(" clear ");
    } else {
      SetData(" ");
    }

    AppendData(type);
    AppendData("=");
    AppendHexString(address);
    if (!clear) {
      AppendData(" size=");
      AppendHexString(size);
    }
  }
};

/**
 * Command: break read=... [size=...] [clear]
 *
 * Sets or clears a read data breakpoint (watchpoint) on a memory range.
 * Triggered when the CPU reads from the specified range.
 */
struct BreakOnRead : public BreakRange_ {
  explicit BreakOnRead(uint32_t address, int size = 0, bool clear = false)
      : BreakRange_("read", address, size, clear) {}
};

/**
 * Command: break write=... [size=...] [clear]
 *
 * Sets or clears a write data breakpoint (watchpoint) on a memory range.
 * Triggered when the CPU writes to the specified range.
 */
struct BreakOnWrite : public BreakRange_ {
  explicit BreakOnWrite(uint32_t address, int size = 0, bool clear = false)
      : BreakRange_("write", address, size, clear) {}
};

/**
 * Command: break execute=... [size=...] [clear]
 *
 * Sets or clears an execution breakpoint on a memory range.
 * Triggered when the CPU executes instructions in the specified range.
 */
struct BreakOnExecute : public BreakRange_ {
  explicit BreakOnExecute(uint32_t address, int size = 0, bool clear = false)
      : BreakRange_("execute", address, size, clear) {}
};

/**
 * Command: bye
 *
 * Gracefully ends the current debugging session.
 */
struct Bye : public RDCPProcessedRequest {
  Bye() : RDCPProcessedRequest("bye") {}
};

/**
 * Command: capctrl [start]
 *
 * Controls the call attribute profiler (CAP).
 */
struct ProfilerCaptureControl : public RDCPProcessedRequest {
  enum class Command { Start, Stop, IsFastCAPEnabled };

  explicit ProfilerCaptureControl(Command command, const std::string& name = "",
                                  uint32_t buffer_size_mb = 0)
      : RDCPProcessedRequest("capctrl") {
    switch (command) {
      case Command::Start:
        SetData(" start name=\"");
        AppendData(name);
        AppendData("\"");
        if (buffer_size_mb > 0) {
          AppendData(" buffersize=");
          AppendDecimalString(buffer_size_mb);
        }
        break;
      case Command::Stop:
        SetData(" stop");
        break;
      case Command::IsFastCAPEnabled:
        SetData(" fastcapenabled");
        break;
    }
  }
};

/**
 * Command: continue thread=... [exception]
 *
 * Resumes execution of a stopped thread.
 * 'exception' flag indicates if the exception should be passed to the thread.
 */
struct Continue : public RDCPProcessedRequest {
  explicit Continue(int thread_id, bool exception = false)
      : RDCPProcessedRequest("continue") {
    SetData(" thread=");
    AppendHexString(thread_id);
    if (exception) {
      AppendData(" exception");
    }
  }

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK || status == StatusCode::ERR_NOT_STOPPED;
  }
};

/**
 * Command: crashdump
 *
 * Forces the creation of a crash dump.
 */
struct Crashdump : public RDCPProcessedRequest {
  Crashdump() : RDCPProcessedRequest("crashdump") {}
};

/**
 * Command: dbgname
 *
 * Retrieves the friendly name of the debug console.
 */
struct GetDevkitName : public RDCPProcessedRequest {
  GetDevkitName() : RDCPProcessedRequest("dbgname") {}

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      name.clear();
      return;
    }

    name.assign(response->Data().begin(), response->Data().end());
    if (name.size() > 2 && name[0] == '"') {
      name = name.substr(1, name.size() - 1);
    }
  }

  std::string name;
};

/**
 * Command: dbgname name=...
 *
 * Sets the friendly name of the debug console.
 */
struct SetDevkitName : public RDCPProcessedRequest {
  explicit SetDevkitName(const std::string& new_name)
      : RDCPProcessedRequest("dbgname") {
    SetData(" name=");
    AppendData(new_name);
  }
};

/**
 * Command: dbgoptions
 *
 * Retrieves the current debug options (crashdump, dpctrace).
 */
struct GetDebugOptions : public RDCPProcessedRequest {
  GetDebugOptions() : RDCPProcessedRequest("dbgoptions") {}

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    auto parsed = RDCPMapResponse(response->Data());
    enable_crashdump = parsed.HasKey("crashdump");
    enable_dpctrace = parsed.HasKey("dpctrace");
  }

  bool enable_crashdump{false};
  bool enable_dpctrace{false};
};

/**
 * Command: dbgoptions crashdump=... dpctrace=...
 *
 * Sets the debug options (enabling/disabling crash dumps and DPC tracing).
 */
struct SetDebugOptions : public RDCPProcessedRequest {
  SetDebugOptions(bool enable_crashdump, bool enable_dcptrace)
      : RDCPProcessedRequest("dbgoptions") {
    SetData(" crashdump=");
    if (enable_crashdump) {
      AppendData("1");
    } else {
      AppendData("0");
    }

    AppendData(" dpctrace=");
    if (enable_dcptrace) {
      AppendData("1");
    } else {
      AppendData("0");
    }
  }
};

/**
 * Command: dbgoptions crashdump=...
 *
 * Enables or disables automatic crash dumps.
 */
struct SetEnableCrashdump : public RDCPProcessedRequest {
  explicit SetEnableCrashdump(bool enable)
      : RDCPProcessedRequest("dbgoptions") {
    SetData(" crashdump=");
    if (enable) {
      AppendData("1");
    } else {
      AppendData("0");
    }
  }
};

/**
 * Command: dbgoptions dpctrace=...
 *
 * Enables or disables DPC (Deferred Procedure Call) tracing.
 */
struct SetEnableDPCTrace : public RDCPProcessedRequest {
  explicit SetEnableDPCTrace(bool enable) : RDCPProcessedRequest("dbgoptions") {
    SetData(" dpctrace=");
    if (enable) {
      AppendData("1");
    } else {
      AppendData("0");
    }
  }
};

/**
 * Command: d3dopcode
 *
 * Related to Direct3D opcode processing. Specific usage details TBD.
 */
struct D3DOpcode : public RDCPProcessedRequest {
  D3DOpcode(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4,
            uint32_t p5)
      : RDCPProcessedRequest("d3dopcode") {
    SetData(" 0=");
    AppendHexString(p0);
    AppendData(" 1=");
    AppendHexString(p1);
    AppendData(" 2=");
    AppendHexString(p2);
    AppendData(" 3=");
    AppendHexString(p3);
    AppendData(" 4=");
    AppendHexString(p4);
    AppendData(" 5=");
    AppendHexString(p5);
  }
};

/**
 * Command: debugger [connect|disconnect]
 *
 * Connects or disconnects the debugger from the debug monitor.
 */
struct Debugger : public RDCPProcessedRequest {
  explicit Debugger(bool connect = true) : RDCPProcessedRequest("debugger") {
    if (connect) {
      SetData(" connect");
    } else {
      SetData(" disconnect");
    }
  }

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK || status == StatusCode::ERR_NOT_DEBUGGABLE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    debuggable = status != ERR_NOT_DEBUGGABLE;
  }

  bool debuggable;
};

/**
 * Command: debugmode
 *
 * Sets the debug security mode.
 */
struct DebugMode : public RDCPProcessedRequest {
  explicit DebugMode(bool enable) : RDCPProcessedRequest("debugmode") {
    if (enable) {
      SetData(" enabled");
    } else {
      SetData(" disabled");
    }
  }
};

/**
 * Command: dedicate [handler="..."] [global]
 *
 * Dedicates the current connection to a specific global handler, preventing
 * other connections from using it.
 */
struct Dedicate : public RDCPProcessedRequest {
  explicit Dedicate(const char* handler_name = nullptr)
      : RDCPProcessedRequest("dedicate") {
    if (handler_name) {
      SetData(" handler=\"");
      AppendData(handler_name);
      AppendData("\"");
    } else {
      SetData(" global");
    }
  }

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_CONNECTION_DEDICATED;
  }
};

/**
 * Command: deftitle [launcher|none|name="..." dir="..."]
 *
 * Sets the default title to run on boot. Can be the dashboard (launcher),
 * no title (none), or a specific title by name and directory.
 */
struct DefTitle : public RDCPProcessedRequest {
  DefTitle() : RDCPProcessedRequest("deftitle") { SetData(" none"); }

  explicit DefTitle(bool launcher) : RDCPProcessedRequest("deftitle") {
    if (launcher) {
      SetData(" launcher");
    } else {
      SetData(" none");
    }
  }

  DefTitle(const std::string& name, const std::string& directory)
      : RDCPProcessedRequest("deftitle") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\" dir=\"");
    AppendData(directory);
    AppendData("\"");
  }
};

/**
 * Command: delete name="..." [dir]
 *
 * Deletes a file or directory.
 */
struct Delete : public RDCPProcessedRequest {
  explicit Delete(const std::string& path, bool is_directory = false)
      : RDCPProcessedRequest("delete") {
    SetData(" name=\"");
    AppendData(path);
    AppendData("\"");
    if (is_directory) {
      AppendData(" dir");
    }
  }
};

/**
 * Command: dirlist name="..."
 *
 * Lists the contents of a directory.
 */
struct DirList : public RDCPProcessedRequest {
  struct Entry {
    std::string name;
    int64_t filesize{0};
    int64_t create_timestamp{0};
    int64_t change_timestamp{0};
    bool is_directory{false};
  };

  explicit DirList(const std::string& path) : RDCPProcessedRequest("dirlist") {
    SetData(" name=\"");
    AppendData(path);
    AppendData("\"");
  }

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_MULTILINE_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    auto parsed = RDCPMultiMapResponse(response->Data());
    for (auto& it : parsed.maps) {
      Entry e;
      e.name = it.GetString("name");
      e.filesize = it.GetQWORD("sizelo", "sizehi");
      e.create_timestamp = it.GetQWORD("createlo", "createhi");
      e.change_timestamp = it.GetQWORD("changelo", "changehi");
      e.is_directory = it.HasKey("directory");
      entries.push_back(e);
    }
  }

  std::vector<Entry> entries;
};

/**
 * Command: dmversion
 *
 * Retrieves the version of the debug monitor.
 */
struct DebugMonitorVersion : public RDCPProcessedRequest {
  DebugMonitorVersion() : RDCPProcessedRequest("dmversion") {}
  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    version.assign(response->Data().begin(), response->Data().end());
  }

  std::string version;
};

/**
 * Command: drivefreespace name=...
 *
 * Retrieves the free space on a specific drive.
 */
struct DriveFreeSpace : public RDCPProcessedRequest {
  explicit DriveFreeSpace(const std::string& name)
      : RDCPProcessedRequest("drivefreespace") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\"");
  }

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_MULTILINE_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    auto parsed = RDCPMapResponse(response->Data());
    free_to_caller = parsed.GetQWORD("freetocallerlo", "freetocallerhi");
    total_bytes = parsed.GetQWORD("totalbyteslo", "totalbyteshi");
    free_bytes = parsed.GetQWORD("totalfreebyteslo", "totalfreebyteshi");
  }

  int64_t free_to_caller{0};
  int64_t total_bytes{0};
  int64_t free_bytes{0};
};

/**
 * Command: drivelist
 *
 * Lists all available drives on the console.
 */
struct DriveList : public RDCPProcessedRequest {
  DriveList() : RDCPProcessedRequest("drivelist") {}

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    char buf[2] = {0};
    for (auto letter : response->Data()) {
      buf[0] = letter;
      drives.emplace_back(buf);
    }

    std::sort(drives.begin(), drives.end());
  }

  std::vector<std::string> drives;
};

/**
 * Command: dvdblk name="..." addr=... size=...
 *
 * Reads raw blocks from a named DVD stream/file.
 * 'name' is resolved to a DVD sample/stream.
 * 'addr' is the offset.
 * 'size' is the amount of data to read.
 */
struct DvdBlk : public RDCPProcessedRequest {
  DvdBlk(const std::string& name, uint32_t addr, uint32_t size)
      : RDCPProcessedRequest("dvdblk") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\" addr=");
    AppendHexString(addr);
    AppendData(" size=");
    AppendHexString(size);
  }

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_BINARY_RESPONSE;
  }
};

/**
 * Command: dvdperf [start | stop [report]]
 *
 * Controls DVD performance data collection.
 */
struct DvdPerf : public RDCPProcessedRequest {
  enum Action { START, STOP, STOP_REPORT };

  explicit DvdPerf(Action action) : RDCPProcessedRequest("dvdperf") {
    switch (action) {
      case START:
        SetData(" start");
        break;
      case STOP:
        SetData(" stop");
        break;
      case STOP_REPORT:
        SetData(" stop report");
        break;
    }
  }
};

/**
 * Command: fileeof
 *
 * Sets the end of file (truncates or extends) for a file.
 * Details on parameters TBD.
 */
struct FileEOF : public RDCPProcessedRequest {
  FileEOF(const std::string& path, uint32_t size)
      : RDCPProcessedRequest("fileeof") {
    SetData(" name=\"");
    AppendData(path);
    AppendData("\" size=");
    AppendHexString(size);
  }
};

/**
 * Command: flash
 *
 * Flashes a new kernel image / BIOS to the console.
 */
struct Flash : public RDCPProcessedRequest {
  explicit Flash(std::vector<uint8_t> buffer, uint32_t crc,
                 bool ignore_version_checking = false)
      : RDCPProcessedRequest("flash"), binary_payload(std::move(buffer)) {
    SetData(" length=");
    AppendHexString(static_cast<uint32_t>(binary_payload.size()));
    AppendData(" crc=");
    AppendHexString(crc);
    if (ignore_version_checking) {
      AppendData(" ignoreversionchecking=1");
    }
  }

  [[nodiscard]] const std::vector<uint8_t>* BinaryPayload() override {
    return &binary_payload;
  }

  std::vector<uint8_t> binary_payload;
};

/**
 * Command: fmtfat
 *
 * Formats a drive partition with the FAT file system.
 */
struct FmtFat : public RDCPProcessedRequest {
  explicit FmtFat(uint32_t partition) : RDCPProcessedRequest("fmtfat") {
    SetData(" partition=");
    AppendHexString(partition);
  }
};

/**
 * Command: funccall thread=...
 *
 * Calls a function in the context of a stopped thread.
 */
struct FuncCall : public RDCPProcessedRequest {
  explicit FuncCall(int thread_id) : RDCPProcessedRequest("funccall") {
    SetData(" thread=");
    AppendHexString(thread_id);
  }
};

/**
 * Command: getcontext thread=... [control] [int] [fp]
 *
 * Retrieves the CPU context (registers) for a stopped thread.
 * Can request control (EIP, EFLAGS, ESP), integer (EAX, etc.), and floating
 * point registers.
 */
struct GetContext : public RDCPProcessedRequest {
  explicit GetContext(int thread_id, bool enable_control = false,
                      bool enable_integer = false, bool enable_float = false)
      : RDCPProcessedRequest("getcontext") {
    SetData(" thread=");
    AppendHexString(thread_id);

    assert(enable_control || enable_integer || enable_float);

    if (enable_control) {
      AppendData(" control");
    }
    if (enable_integer) {
      AppendData(" int");
    }
    if (enable_float) {
      AppendData(" fp");
    }
  }

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_MULTILINE_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    context.Parse(RDCPMapResponse(response->Data()));
  }

  ThreadContext context;
};

/**
 * Command: getd3dstate
 *
 * Retrieves the current Direct3D state.
 * Returns binary data of fixed size 1180 bytes.
 */
struct GetD3DState : public RDCPProcessedRequest {
  GetD3DState() : RDCPProcessedRequest("getd3dstate") {
    binary_response_size_parser_ = [](uint8_t const* buffer,
                                      uint32_t buffer_size, long& binary_size,
                                      uint32_t& bytes_consumed) {
      (void)buffer;
      (void)buffer_size;
      binary_size = 1180;
      bytes_consumed = 0;
      return true;
    };
  }

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_BINARY_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }
    auto chunk = response->Data();
    data.assign(chunk.begin(), chunk.end());
  }

  std::vector<uint8_t> data;
};

/**
 * Command: getextcontext
 *
 * Retrieves extended context (FPU/SSE state) for a thread.
 */
struct GetExtContext : public RDCPProcessedRequest {
  explicit GetExtContext(uint32_t thread_id);

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_BINARY_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    context.Parse(response->Data());
  }

  ThreadFloatContext context;
};

/**
 * Command: getfile name="..." [offset=... size=...]
 *
 * Reads content from a file on the Xbox.
 */
struct GetFile : public RDCPProcessedRequest {
  explicit GetFile(const std::string& path);
  GetFile(const std::string& path, int32_t offset, int32_t size);

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_BINARY_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    auto chunk = response->Data();
    data.assign(chunk.begin(), chunk.end());
  }

  std::vector<uint8_t> data;
};

/**
 * Command: getfileattributes name="..."
 *
 * Retrieves attributes (size, timestamps) for a file.
 */
struct GetFileAttributes : public RDCPProcessedRequest {
  explicit GetFileAttributes(const std::string& path)
      : RDCPProcessedRequest("getfileattributes") {
    SetData(" name=\"");
    AppendData(path);
    AppendData("\"");
  }

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_MULTILINE_RESPONSE ||
           status == StatusCode::ERR_FILE_NOT_FOUND ||
           status == StatusCode::ERR_ACCESS_DENIED;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    exists = status == StatusCode::OK_MULTILINE_RESPONSE;
    if (exists) {
      auto parsed = RDCPMapResponse(response->Data());
      filesize = parsed.GetQWORD("sizelo", "sizehi");
      create_timestamp = parsed.GetQWORD("createlo", "createhi");
      change_timestamp = parsed.GetQWORD("changelo", "changehi");
      flags = parsed.valueless_keys;
    }
  }

  bool exists;
  int64_t filesize{0};
  uint64_t create_timestamp{0};
  uint64_t change_timestamp{0};
  std::set<std::string> flags;
};

/**
 * Command: getgamma
 *
 * Retrieves the video gamma table.
 */
struct GetGamma : public RDCPProcessedRequest {
  GetGamma();

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_BINARY_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    auto chunk = response->Data();
    data.assign(chunk.begin(), chunk.end());
  }

  std::vector<uint8_t> data;
};

/**
 * Command: getmem addr=... length=...
 *
 * Reads a block of memory and returns it as a HEX string.
 */
struct GetMem : public RDCPProcessedRequest {
  GetMem(uint32_t addr, uint32_t length) : RDCPProcessedRequest("getmem") {
    SetData(" addr=");
    AppendHexString(addr);
    AppendData(" length=");
    AppendHexString(length);
  }
};

/**
 * Command: getmem2 addr=... length=...
 *
 * Reads a block of memory and returns it as binary data.
 */
struct GetMemBinary : public RDCPProcessedRequest {
  GetMemBinary(uint32_t addr, uint32_t length);

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_BINARY_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    auto chunk = response->Data();
    data.assign(chunk.begin(), chunk.end());
  }

  uint32_t length;
  std::vector<uint8_t> data;
};

/**
 * Command: getpalette
 *
 * Retrieves the palette for a specific stage.
 */
struct GetPalette : public RDCPProcessedRequest {
  explicit GetPalette(int stage) : RDCPProcessedRequest("getpalette") {
    SetData(" STAGE=");
    AppendHexString(stage);
  }
};

/**
 * Command: getpid
 *
 * Retrieves the process ID of the running title.
 */
struct GetProcessID : public RDCPProcessedRequest {
  GetProcessID() : RDCPProcessedRequest("getpid") {}

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    auto parsed = RDCPMapResponse(response->Data());
    process_id = parsed.GetDWORD("pid");
  }

  int process_id{0};
};

/**
 * Command: getsum addr=... length=...
 *
 * Calculates a checksum (or hash) for a memory range.
 */
struct GetChecksum : public RDCPProcessedRequest {
  GetChecksum(uint32_t addr, uint32_t len, uint32_t blocksize);

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_BINARY_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    auto value = reinterpret_cast<const uint32_t*>(response->Data().data());
    for (int i = 0; i < length / 4; ++i) {
      checksums.push_back(*value++);
    }
  }

  std::vector<uint32_t> checksums;
  uint32_t length;
};

/**
 * Command: getsurf id=...
 *
 * Retrieves a surface (image data) by its ID.
 */
struct GetSurface : public RDCPProcessedRequest {
  explicit GetSurface(int surface_id) : RDCPProcessedRequest("getsurf") {
    SetData(" id=");
    AppendHexString(surface_id);
  }
};

/**
 * Command: getuserpriv [name="..."]
 *
 * Retrieves the privileges for a user (or current user if name is omitted).
 */
struct GetUserPrivileges : public RDCPProcessedRequest {
  GetUserPrivileges() : RDCPProcessedRequest("getuserpriv") {}
  explicit GetUserPrivileges(const std::string& username)
      : RDCPProcessedRequest("getuserpriv") {
    SetData(" name=\"");
    AppendData(username);
    AppendData("\"");
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }
    auto parsed = RDCPMapResponse(response->Data());
    flags = parsed.valueless_keys;
  }

  std::set<std::string> flags;
};

/**
 * Command: getutildrvinfo
 *
 * Retrieves information about the utility drive partitions.
 */
struct GetUtilityDriveInfo : public RDCPProcessedRequest {
  GetUtilityDriveInfo() : RDCPProcessedRequest("getutildrvinfo") {}
  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }
    auto parsed = RDCPMapResponse(response->Data());
    for (auto& it : parsed.map) {
      partitions[it.first] = ParseInt32(it.second);
    }
  }

  std::map<std::string, uint32_t> partitions;
};

/**
 * Command: go
 *
 * Resumes execution of the title.
 */
struct Go : public RDCPProcessedRequest {
  Go() : RDCPProcessedRequest("go") {}
};

/**
 * Command: gpucount [enable|disable]
 *
 * Enables or disables GPU performance counters.
 */
struct EnableGPUCounter : public RDCPProcessedRequest {
  explicit EnableGPUCounter(bool enable = true)
      : RDCPProcessedRequest("gpucount") {
    if (enable) {
      SetData(" enable");
    } else {
      SetData(" disable");
    }
  }
};

/**
 * Command: halt [thread=...]
 *
 * Halts the execution of a specific thread or all threads.
 */
struct Halt : public RDCPProcessedRequest {
  Halt() : RDCPProcessedRequest("halt") {}
  explicit Halt(int thread_id) : RDCPProcessedRequest("halt") {
    SetData(" thread=");
    AppendHexString(thread_id);
  }
};

/**
 * Command: isbreak addr=...
 *
 * Checks if there is a breakpoint at the specified address.
 */
struct IsBreak : public RDCPProcessedRequest {
  enum Type {
    NONE = 0,
    WRITE,
    READ_OR_WRITE,
    EXECUTE,
    ADDRESS,
  };

  explicit IsBreak(uint32_t addr) : RDCPProcessedRequest("isbreak") {
    SetData(" addr=");
    AppendHexString(addr);
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }
    auto parsed = RDCPMapResponse(response->Data());
    type = static_cast<Type>(parsed.GetDWORD("type"));
  }

  Type type{NONE};
};

/**
 * Command: isdebugger
 *
 * Checks if a debugger is currently connected.
 */
struct IsDebugger : public RDCPProcessedRequest {
  IsDebugger() : RDCPProcessedRequest("isdebugger") {}

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_BINARY_RESPONSE ||
           status == StatusCode::ERR_EXISTS;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    attached = status == StatusCode::ERR_EXISTS;
  }

  bool attached{false};
};

/**
 * Command: isstopped thread=...
 *
 * Checks if a specific thread is stopped.
 */
struct IsStopped : public RDCPProcessedRequest {
  explicit IsStopped(uint32_t thread_id) : RDCPProcessedRequest("isstopped") {
    SetData(" thread=");
    AppendHexString(thread_id);
  }

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK || status == StatusCode::ERR_NOT_STOPPED;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    stopped = status == StatusCode::OK;
    if (!stopped) {
      return;
    }

    const auto& data = response->Data();
    auto delimiter = std::find(data.begin(), data.end(), ' ');

    std::string reason(data.begin(), delimiter);
    RDCPMapResponse parsed(delimiter, data.end());

    if (reason == "stopped") {
      stop_reason = std::make_shared<StopReasonUnknown>();
      return;
    }

    if (reason == "debugstr") {
      stop_reason = std::make_shared<StopReasonDebugstr>(parsed);
      return;
    }

    if (reason == "assert") {
      stop_reason = std::make_shared<StopReasonAssertion>(parsed);
      return;
    }

    if (reason == "break") {
      stop_reason = std::make_shared<StopReasonBreakpoint>(parsed);
      return;
    }

    if (reason == "singlestep") {
      stop_reason = std::make_shared<StopReasonSingleStep>(parsed);
      return;
    }

    if (reason == "data") {
      stop_reason = std::make_shared<StopReasonDataBreakpoint>(parsed);
      return;
    }

    if (reason == "execution") {
      stop_reason = std::make_shared<StopReasonExecutionStateChange>(parsed);
      return;
    }

    if (reason == "exception") {
      stop_reason = std::make_shared<StopReasonException>(parsed);
      return;
    }

    if (reason == "create") {
      stop_reason = std::make_shared<StopReasonCreateThread>(parsed);
      return;
    }

    if (reason == "terminate") {
      stop_reason = std::make_shared<StopReasonTerminateThread>(parsed);
      return;
    }

    if (reason == "modload") {
      stop_reason = std::make_shared<StopReasonModuleLoad>(parsed);
      return;
    }

    if (reason == "sectload") {
      stop_reason = std::make_shared<StopReasonSectionLoad>(parsed);
      return;
    }

    if (reason == "sectunload") {
      stop_reason = std::make_shared<StopReasonSectionUnload>(parsed);
      return;
    }

    if (reason == "rip") {
      stop_reason = std::make_shared<StopReasonRIP>(parsed);
      return;
    }

    if (reason == "ripstop") {
      stop_reason = std::make_shared<StopReasonRIPStop>(parsed);
      return;
    }
  }

  bool stopped{false};
  std::shared_ptr<StopReasonBase_> stop_reason;
};

/**
 * Command: irtsweep
 *
 * TBD
 */
struct IRTSweep : public RDCPProcessedRequest {
  IRTSweep() : RDCPProcessedRequest("irtsweep") {}
};

/**
 * Command: kd [enable|disable|except|exceptif]
 *
 * Configures Kernel Debugger behavior.
 */
struct KernelDebug : public RDCPProcessedRequest {
  enum Mode {
    ENABLE,
    DISABLE,
    EXCEPT,
    EXCEPT_IF,
  };

  KernelDebug(Mode mode) : RDCPProcessedRequest("kd") {
    switch (mode) {
      case ENABLE:
        SetData(" enable");
        break;
      case DISABLE:
        SetData(" disable");
        break;
      case EXCEPT:
        SetData(" except");
        break;
      case EXCEPT_IF:
        SetData(" exceptif");
        break;
    }
  }
};

/**
 * Command: keyxchg
 *
 * Performs a key exchange operation for authentication/encryption.
 */
struct KeyExchange : public RDCPProcessedRequest {
  explicit KeyExchange(std::vector<uint8_t> key_data)
      : RDCPProcessedRequest("keyxchg"), key_data_(std::move(key_data)) {}

  const std::vector<uint8_t>* BinaryPayload() override { return &key_data_; }

  std::vector<uint8_t> key_data_;
};

/**
 * Command: lockmode
 *
 * Sets the system lock mode.
 */
struct LockMode : public RDCPProcessedRequest {
  // Lock command: lockmode boxid=... [encrypt]
  explicit LockMode(uint64_t box_id, bool encrypt = false)
      : RDCPProcessedRequest("lockmode") {
    SetData(" boxid=");
    AppendHexString(box_id);
    if (encrypt) {
      AppendData(" encrypt");
    }
  }
};

/**
 * Command: lockmode unlock
 *
 * Unlocks the system.
 */
struct Unlock : public RDCPProcessedRequest {
  Unlock() : RDCPProcessedRequest("lockmode") { SetData(" unlock"); }
};

/**
 * Command: lop
 *
 * Controls the LOP profiler.
 */
struct LOP : public RDCPProcessedRequest {
  enum Command {
    START_EVENT,
    START_COUNTER,
    STOP,
    INFO,
  };

  explicit LOP(Command command, int data = 0) : RDCPProcessedRequest("lop") {
    switch (command) {
      case START_EVENT:
        SetData(" cmd=start event=");
        AppendHexString(data);
        break;

      case START_COUNTER:
        SetData(" cmd=start counter=");
        AppendHexString(data);
        break;

      case STOP:
        SetData(" cmd=stop");
        break;

      case INFO:
        SetData(" cmd=info");
        break;
    }
  }
};

/**
 * Command: magicboot title="..." [debug] [cold]
 *
 * Reboots the console into a specific title.
 * Can perform a cold boot and enable debug monitor on the new title.
 */
struct MagicBoot : public RDCPProcessedRequest {
  explicit MagicBoot(const std::string& title,
                     bool enable_xbdm_after_reboot = false,
                     bool enable_cold = false)
      : RDCPProcessedRequest("magicboot") {
    SetData(" title=\"");
    AppendData(title);
    AppendData("\"");
    if (enable_xbdm_after_reboot) {
      AppendData(" debug");
    }
    if (enable_cold) {
      AppendData(" cold");
    }
  }
};

/**
 * Command: memtrack
 *
 * Usage: memtrack cmd=[enable|disable|save|...]
 *
 * Controls memory tracking / leak detection functionality.
 */
struct MemTrack : public RDCPProcessedRequest {
  enum Command {
    ENABLE,
    ENABLE_ONCE,
    DISABLE,
    SAVE,
    QUERY_STACK_DEPTH,
    QUERY_TYPE,
    QUERY_FLAGS,
  };

  explicit MemTrack(Command command) : RDCPProcessedRequest("memtrack") {
    switch (command) {
      case DISABLE:
        SetData(" cmd=disable");
        break;

      case QUERY_STACK_DEPTH:
        SetData(" cmd=querystackdepth");
        break;

      case QUERY_FLAGS:
        SetData(" cmd=queryflags");
        break;

      default:
        assert(false && "Invalid command.");
    }
  }

  MemTrack(Command command, uint32_t stack_depth, uint32_t flags)
      : RDCPProcessedRequest("memtrack") {
    switch (command) {
      case ENABLE:
        SetData(" cmd=enable");
        break;

      case ENABLE_ONCE:
        SetData(" cmd=enableonce");
        break;

      default:
        assert(false && "Invalid command.");
    }

    AppendData(" stackdepth=");
    AppendHexString(stack_depth);
    AppendData(" flags=");
    AppendHexString(flags);
  }

  MemTrack(Command command, const std::string& filename)
      : RDCPProcessedRequest("memtrack") {
    switch (command) {
      case SAVE:
        SetData(" cmd=save filename=\"");
        AppendData(filename);
        AppendData("\"");
        break;

      default:
        assert(false && "Invalid command.");
    }
  }

  MemTrack(Command command, uint32_t type) : RDCPProcessedRequest("memtrack") {
    switch (command) {
      case QUERY_TYPE:
        SetData(" cmd=querytype type=");
        AppendHexString(type);
        break;

      default:
        assert(false && "Invalid command.");
    }
  }
};

/**
 * Command: mmglobal
 *
 * Retrieves global memory manager statistics (available pages, pool info, etc).
 */
struct MemoryMapGlobal : public RDCPProcessedRequest {
  MemoryMapGlobal() : RDCPProcessedRequest("mmglobal") {}

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_MULTILINE_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    auto parsed = RDCPMapResponse(response->Data());
    mm_highest_physical_page = parsed.GetDWORD("MmHighestPhysicalPage");
    retail_pfn_region = parsed.GetDWORD("RetailPfnRegion");
    system_pte_range = parsed.GetDWORD("SystemPteRange");
    available_pages = parsed.GetDWORD("AvailablePages");
    allocated_pages_by_usage = parsed.GetDWORD("AllocatedPagesByUsage");
    pfn_database = parsed.GetDWORD("PfnDatabase");
    address_space_lock = parsed.GetDWORD("AddressSpaceLock");
    vad_root = parsed.GetDWORD("VadRoot");
    vad_hint = parsed.GetDWORD("VadHint");
    vad_free_hint = parsed.GetDWORD("VadFreeHint");
    mm_number_of_physical_pages = parsed.GetDWORD("MmNumberOfPhysicalPages");
    mm_available_pages = parsed.GetDWORD("MmAvailablePages");
  }

  uint32_t mm_highest_physical_page{0};
  uint32_t retail_pfn_region{0};
  uint32_t system_pte_range{0};
  uint32_t available_pages{0};
  uint32_t allocated_pages_by_usage{0};
  uint32_t pfn_database{0};
  uint32_t address_space_lock{0};
  uint32_t vad_root{0};
  uint32_t vad_hint{0};
  uint32_t vad_free_hint{0};
  uint32_t mm_number_of_physical_pages{0};
  uint32_t mm_available_pages{0};
};

/**
 * Command: mkdir name="..."
 *
 * Creates a new directory.
 */
struct Mkdir : public RDCPProcessedRequest {
  explicit Mkdir(const std::string& name) : RDCPProcessedRequest("mkdir") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\"");
  }
};

/**
 * Command: modlong name="..."
 *
 * Retrieves the full long path/name for a loaded module/file.
 */
struct ModLongName : public RDCPProcessedRequest {
  explicit ModLongName(const std::string& name)
      : RDCPProcessedRequest("modlong") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\"");
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }
    path = response->Message();
  }

  std::string path;
};

/**
 * Command: modsections name="..."
 *
 * Lists the sections (segments) of a loaded module.
 */
struct ModSections : public RDCPProcessedRequest {
  struct SectionInfo {
    std::string name;
    uint32_t base;
    uint32_t size;
    uint32_t index;
    uint32_t flags;
    std::set<std::string> additional_flags;

    friend std::ostream& operator<<(std::ostream& os, const SectionInfo& i) {
      os << "name: " << i.name << " base: " << i.base << " size: " << i.size
         << " index: " << i.index << " flags: " << i.flags;
      for (auto& flag : i.additional_flags) {
        os << " " << flag;
      }
      return os;
    }
  };

  explicit ModSections(const std::string& name)
      : RDCPProcessedRequest("modsections") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\"");
  }

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_MULTILINE_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    auto parsed = RDCPMultiMapResponse(response->Data());
    for (auto& it : parsed.maps) {
      SectionInfo info{
          .name = it.GetString("name"),
          .base = static_cast<uint32_t>(it.GetDWORD("base")),
          .size = static_cast<uint32_t>(it.GetDWORD("size")),
          .index = static_cast<uint32_t>(it.GetDWORD("index")),
          .flags = static_cast<uint32_t>(it.GetDWORD("flags")),
          .additional_flags = it.valueless_keys,
      };
      sections.emplace_back(info);
    }
  }

  std::vector<SectionInfo> sections;
};

/**
 * Command: modules
 *
 * Lists all loaded modules.
 */
struct Modules : public RDCPProcessedRequest {
  Modules() : RDCPProcessedRequest("modules") {}

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_MULTILINE_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    auto parsed = RDCPMultiMapResponse(response->Data());
    for (auto& it : parsed.maps) {
      Module module(it.GetString("name"),
                    static_cast<uint32_t>(it.GetDWORD("base")),
                    static_cast<uint32_t>(it.GetDWORD("size")),
                    static_cast<uint32_t>(it.GetDWORD("check")),
                    static_cast<uint32_t>(it.GetDWORD("timestamp")),
                    it.HasKey("tls"), it.HasKey("xbe"));
      modules.emplace_back(module);
    }
  }

  std::vector<Module> modules;
};

/**
 * Command: stopon [all] [createthread] [fce] [debugstr] [stacktrace]
 *          nostopon [all] [...]
 *
 * Configures the "Stop On" events (events that cause the debug monitor to halt
 * execution).
 */
struct StopOnBase_ : public RDCPProcessedRequest {
  static constexpr uint32_t kAll = 0xFFFFFFFF;
  static constexpr uint32_t kCreateThread = 0x01;
  static constexpr uint32_t kFirstChanceException = 0x02;
  static constexpr uint32_t kDebugStr = 0x04;
  static constexpr uint32_t kStacktrace = 0x08;

  StopOnBase_(std::string command, uint32_t events)
      : RDCPProcessedRequest(std::move(command)) {
    if (events == kAll) {
      SetData(" all");
      return;
    }

    if (events & kCreateThread) {
      AppendData(" createthread");
    }
    if (events & kFirstChanceException) {
      AppendData(" fce");
    }
    if (events & kDebugStr) {
      AppendData(" debugstr");
    }
    if (events & kStacktrace) {
      AppendData(" stacktrace");
    }
  }
};

/**
 * Command: nostopon ...
 *
 * Disables stopping on specified events.
 */
struct NoStopOn : public StopOnBase_ {
  explicit NoStopOn(uint32_t events = kAll) : StopOnBase_("nostopon", events) {}
};

/**
 * Command: stopon ...
 *
 * Enables stopping on specified events.
 */
struct StopOn : public StopOnBase_ {
  explicit StopOn(uint32_t events = kAll) : StopOnBase_("stopon", events) {}
};

/**
 * Command: notify
 *
 * Sets up a notification channel.
 */
struct Notify : public RDCPProcessedRequest {
  Notify() : RDCPProcessedRequest("notify") {}

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_CONNECTION_DEDICATED;
  }
};

/**
 * Command: notifyat port=... [drop] [debug]
 *
 * Sets up a notification channel at a specific port.
 */
struct NotifyAt : public RDCPProcessedRequest {
  explicit NotifyAt(uint16_t port, bool drop_flag = false,
                    bool debug_flag = false)
      : RDCPProcessedRequest("notifyat"), port(port) {
    SetData(" Port=");
    AppendDecimalString(port);
    if (drop_flag) {
      AppendData(" drop");
    }
    if (debug_flag) {
      AppendData(" debug");
    }
  }

  NotifyAt(uint16_t port, std::string address, bool drop_flag = false,
           bool debug_flag = false)
      : RDCPProcessedRequest("notifyat"),
        port(port),
        address(std::move(address)) {
    SetData(" Port=");
    AppendHexString(port);
    AppendData(" addr=");
    AppendData(address);
    if (drop_flag) {
      AppendData(" drop");
    }
    if (debug_flag) {
      AppendData(" debug");
    }
  }

  std::string address;
  uint16_t port;
};

/**
 * Command: pbsnap
 *
 * Captures a D3D snapshot.
 */
struct PBSnap : public RDCPProcessedRequest {
  PBSnap() : RDCPProcessedRequest("pbsnap") {}
};

/**
 * Command: pclist
 *
 * Lists all available performance counters.
 */
struct PerformanceCounterList : public RDCPProcessedRequest {
  struct Counter {
    std::string name;
    int32_t type;
  };

  PerformanceCounterList() : RDCPProcessedRequest("pclist") {}

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_MULTILINE_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    auto parsed = RDCPMultiMapResponse(response->Data());
    for (auto& it : parsed.maps) {
      Counter counter{
          .name = it.GetString("name"),
          .type = it.GetDWORD("type"),
      };
      counters.emplace_back(counter);
    }
  }

  std::vector<Counter> counters;
};

/**
 * Command: pdbinfo addr=...
 *
 * Retrieves PDB (Program Database) information for a module at the specified
 * address.
 */
struct PDBInfo : public RDCPProcessedRequest {
  explicit PDBInfo(uint32_t address) : RDCPProcessedRequest("pdbinfo") {
    SetData(" addr=");
    AppendHexString(address);
  }

  // TODO: Parse response.
};

/**
 * Command: pssnap x=... y=... [flags=...] [marker=...]
 *
 * D3D snapshot.
 */
struct PSSnap : public RDCPProcessedRequest {
  PSSnap(int x, int y, int flags = 0, int marker = 0)
      : RDCPProcessedRequest("pssnap") {
    SetData(" x=");
    AppendHexString(x);
    AppendData(" y=");
    AppendHexString(y);

    if (flags) {
      AppendData(" flags=");
      AppendHexString(flags);
    }
    if (marker) {
      AppendData(" marker=");
      AppendHexString(marker);
    }
  }

  // TODO: Parse response.
};

/**
 * Command: querypc name="..." [type=...]
 *
 * Queries a performance counter.
 */
struct QueryPerformanceCounter : public RDCPProcessedRequest {
  explicit QueryPerformanceCounter(const std::string& name)
      : RDCPProcessedRequest("querypc") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\"");
  }

  QueryPerformanceCounter(const std::string& name, int counter_type)
      : RDCPProcessedRequest("querypc") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\" type=");
    AppendHexString(counter_type);
  }
};

/**
 * Command: reboot [wait] [warm] [nodebug] [stop]
 *
 * Reboots the console.
 */
struct Reboot : public RDCPProcessedRequest {
  static constexpr uint32_t kWait = 0x01;
  static constexpr uint32_t kWarm = 0x02;
  static constexpr uint32_t kNoDebug = 0x04;
  static constexpr uint32_t kStop = 0x08;

  explicit Reboot(uint32_t flags = 0) : RDCPProcessedRequest("reboot") {
    if (flags & kWait) {
      AppendData(" wait");
    }
    if (flags & kWarm) {
      AppendData(" warm");
    }
    if (flags & kNoDebug) {
      AppendData(" nodebug");
    }
    if (flags & kStop) {
      AppendData(" stop");
    }
  }
};

/**
 * Command: rename name="..." newname="..."
 *
 * Renames a file or directory.
 */
struct Rename : public RDCPProcessedRequest {
  Rename(const std::string& name, const std::string& new_name)
      : RDCPProcessedRequest("rename") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\" newname=\"");
    AppendData(new_name);
    AppendData("\"");
  }
};

/**
 * Command: resume thread=...
 *
 * Resumes a suspended thread.
 */
struct Resume : public RDCPProcessedRequest {
  explicit Resume(int thread_id) : RDCPProcessedRequest("resume") {
    SetData(" thread=");
    AppendHexString(thread_id);
  }
};

/**
 * Command: screenshot
 *
 * Captures a screenshot of the current frame buffer.
 */
struct Screenshot : public RDCPProcessedRequest {
  Screenshot();

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_BINARY_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    auto chunk = response->Data();
    data.assign(chunk.begin(), chunk.end());
  }

  uint32_t width{0};
  uint32_t height{0};
  uint32_t format{0};
  uint32_t pitch{0};

  std::vector<uint8_t> data;

 private:
  bool ParseSize(uint8_t const* buffer, uint32_t buffer_size, long& binary_size,
                 uint32_t& bytes_consumed);
};

/**
 * Command: sendfile name="..." length=...
 *
 * Sends a file to the Xbox.
 */
struct SendFile : public RDCPProcessedRequest {
  SendFile(const std::string& name, std::vector<uint8_t> buffer)
      : RDCPProcessedRequest("sendfile"), binary_payload(std::move(buffer)) {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\" length=");
    AppendHexString(static_cast<uint32_t>(binary_payload.size()));
  }

  [[nodiscard]] const std::vector<uint8_t>* BinaryPayload() override {
    return &binary_payload;
  }

  std::vector<uint8_t> binary_payload;
};

/**
 * Command: setconfig index=... value=...
 *
 * Sets specific NVRAM configuration values.
 */
struct SetNVRAMConfig : public RDCPProcessedRequest {
  SetNVRAMConfig(int32_t index, int32_t value)
      : RDCPProcessedRequest("setconfig") {
    SetData(" index=");
    AppendHexString(index);
    AppendData(" value=");
    AppendHexString(value);
  }
};

/**
 * Command: setcontext thread=... [ext=...] [context data...]
 *
 * Sets the CPU context (registers) for a thread.
 * Can set standard integer/control registers and extended floating point state.
 */
struct SetContext : public RDCPProcessedRequest {
  SetContext(int thread_id, const ThreadContext& context)
      : RDCPProcessedRequest("setcontext") {
    SetData(" thread=");
    AppendHexString(thread_id);
    AppendData(" ");
    AppendData(context.Serialize());
  }

  SetContext(int thread_id, const ThreadFloatContext& context)
      : RDCPProcessedRequest("setcontext") {
    SetData(" thread=");
    AppendHexString(thread_id);

    binary_payload = context.Serialize();
    AppendData(" ext=");
    AppendHexString(static_cast<uint32_t>(binary_payload.size()));
  }

  SetContext(int thread_id, const ThreadContext& context,
             const ThreadFloatContext& ext)
      : RDCPProcessedRequest("setcontext") {
    SetData(" thread=");
    AppendHexString(thread_id);
    AppendData(" ");
    AppendData(context.Serialize());
    binary_payload = ext.Serialize();
    AppendData(" ext=");
    AppendHexString(static_cast<uint32_t>(binary_payload.size()));
  }

  [[nodiscard]] const std::vector<uint8_t>* BinaryPayload() override {
    return &binary_payload;
  }

  std::vector<uint8_t> binary_payload;
};

/**
 * Command: setfileattributes name="..." [readonly=...] [hidden=...]
 * [create...=...] [change...=...]
 *
 * Sets attributes (readonly, hidden, timestamps) for a file.
 */
struct SetFileAttributes : public RDCPProcessedRequest {
  explicit SetFileAttributes(
      const std::string& name, std::optional<bool> readonly = std::nullopt,
      std::optional<bool> hidden = std::nullopt,
      std::optional<uint64_t> create_timestamp = std::nullopt,
      std::optional<uint64_t> change_timestamp = std::nullopt)
      : RDCPProcessedRequest("setfileattributes") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\"");

    if (readonly.has_value()) {
      AppendData(" readonly=");
      AppendData(*readonly ? "1" : "0");
    }

    if (hidden.has_value()) {
      AppendData(" hidden=");
      AppendData(*hidden ? "1" : "0");
    }

    if (create_timestamp.has_value()) {
      AppendData(" createhi=");
      AppendHexString(static_cast<uint32_t>(*create_timestamp >> 32));
      AppendData(" createlo=");
      AppendHexString(static_cast<uint32_t>(*create_timestamp & 0xFFFFFFFF));
    }

    if (change_timestamp.has_value()) {
      AppendData(" changehi=");
      AppendHexString(static_cast<uint32_t>(*change_timestamp >> 32));
      AppendData(" changelo=");
      AppendHexString(static_cast<uint32_t>(*change_timestamp & 0xFFFFFFFF));
    }
  }
};

/**
 * Command: setmem addr=... data=...
 *
 * Writes data to memory. Data can be binary or a hex string.
 */
struct SetMem : public RDCPProcessedRequest {
  static constexpr uint32_t kMaximumDataSize = (MAXIMUM_SEND_LENGTH - 32) / 4;
  SetMem(uint32_t address, const std::vector<uint8_t>& data)
      : RDCPProcessedRequest("setmem") {
    assert(data.size() <= kMaximumDataSize);
    SetData(" addr=");
    AppendHexString(address);
    AppendData(" data=");
    AppendHexBuffer(data);
  }

  SetMem(uint32_t address, const std::string& hex_data)
      : RDCPProcessedRequest("setmem") {
    assert(hex_data.size() <= (MAXIMUM_SEND_LENGTH - 32));
    SetData(" addr=");
    AppendHexString(address);
    AppendData(" data=");
    AppendData(hex_data);
  }
};

/**
 * Command: setsystime clocklo=... clockhi=... [tz=...]
 *
 * Sets the system time.
 */
struct SetSystemTime : public RDCPProcessedRequest {
  explicit SetSystemTime(uint64_t nt_timestamp)
      : RDCPProcessedRequest("setsystime") {
    SetData(" clocklo=");
    AppendHexString(static_cast<uint32_t>(nt_timestamp & 0xFFFFFFFF));
    AppendData(" clockhi=");
    AppendHexString(static_cast<uint32_t>((nt_timestamp >> 32) & 0xFFFFFFFF));
  }

  SetSystemTime(uint64_t nt_timestamp, int32_t tz)
      : RDCPProcessedRequest("setsystime") {
    SetData(" clocklo=");
    AppendHexString(static_cast<uint32_t>(nt_timestamp & 0xFFFFFFFF));
    AppendData(" clockhi=");
    AppendHexString(static_cast<uint32_t>((nt_timestamp >> 32) & 0xFFFFFFFF));
    AppendData(" tz=");
    AppendHexString(tz);
  }
};

/**
 * Command: signcontent
 *
 * Signs the content? Details TBD.
 */
struct SignContent : public RDCPProcessedRequest {
  explicit SignContent(const std::string& path, uint32_t title_id = 0)
      : RDCPProcessedRequest("signcontent") {
    SetData(" name=\"");
    AppendData(path);
    AppendData("\"");
    if (title_id != 0) {
      AppendData(" titleid=");
      AppendHexString(title_id);
    }
  }
};

/**
 * Command: stop
 *
 * Stops execution of the title.
 */
struct Stop : public RDCPProcessedRequest {
  Stop() : RDCPProcessedRequest("stop") {}

  [[nodiscard]] bool IsOK() const override {
    // Treat "already stopped" responses as successful.
    return status == StatusCode::OK || status == StatusCode::ERR_UNEXPECTED;
  }
};

/**
 * Command: suspend thread=...
 *
 * Suspends execution of a specific thread.
 */
struct Suspend : public RDCPProcessedRequest {
  explicit Suspend(int thread_id) : RDCPProcessedRequest("suspend") {
    SetData(" thread=");
    AppendHexString(thread_id);
  }
};

/**
 * Command: sysfileupd
 *
 * Updates a system file? Details TBD.
 */
struct SysFileUpd : public RDCPProcessedRequest {
  // Mode 1: Update from local source file on Xbox
  SysFileUpd(const std::string& path, const std::string& local_src,
             uint64_t filetime)
      : RDCPProcessedRequest("sysfileupd") {
    SetData(" name=\"");
    AppendData(path);
    AppendData("\" localsrc=\"");
    AppendData(local_src);
    AppendData("\"");
    AppendFTime(filetime);
  }

  // Mode 2: Update from stream (upload)
  SysFileUpd(const std::string& path, std::vector<uint8_t> data, uint32_t crc,
             uint64_t filetime)
      : RDCPProcessedRequest("sysfileupd"), binary_payload(std::move(data)) {
    SetData(" name=\"");
    AppendData(path);
    AppendData("\" size=");
    AppendHexString(static_cast<uint32_t>(binary_payload.size()));
    AppendData(" crc=");
    AppendHexString(crc);
    AppendFTime(filetime);
  }

  [[nodiscard]] const std::vector<uint8_t>* BinaryPayload() override {
    return binary_payload.empty() ? nullptr : &binary_payload;
  }

 private:
  void AppendFTime(uint64_t filetime) {
    AppendData(" ftimelo=");
    AppendHexString(static_cast<uint32_t>(filetime & 0xFFFFFFFF));
    AppendData(" ftimehi=");
    AppendHexString(static_cast<uint32_t>((filetime >> 32) & 0xFFFFFFFF));
  }

  std::vector<uint8_t> binary_payload;
};

/**
 * Command: systime
 *
 * Retrieves the current system time.
 */
struct SystemTime : public RDCPProcessedRequest {
  SystemTime() : RDCPProcessedRequest("systime") {}

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }
    auto parsed = RDCPMapResponse(response->Data());
    system_time = parsed.GetQWORD("low", "high");
  }

  uint64_t system_time{0};
};

/**
 * Command: threadinfo thread=...
 *
 * Retrieves information about a specific thread (suspend count, priority, stack
 * limits, etc.).
 */
struct ThreadInfo : public RDCPProcessedRequest {
  explicit ThreadInfo(int thread_id) : RDCPProcessedRequest("threadinfo") {
    SetData(" thread=");
    AppendHexString(thread_id);
  }

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_MULTILINE_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    auto parsed = RDCPMapResponse(response->Data());
    suspend_count = parsed.GetDWORD("suspend");
    priority = parsed.GetDWORD("priority");
    tls_base = parsed.GetDWORD("tlsbase");
    start = parsed.GetDWORD("start");
    base = parsed.GetDWORD("base");
    limit = parsed.GetDWORD("limit");
    create_timestamp = parsed.GetQWORD("createlo", "createhi");
  }

  int32_t suspend_count{0};
  int32_t priority{0};
  uint32_t tls_base{0};
  uint32_t start{0};
  uint32_t base{0};
  uint32_t limit{0};
  uint64_t create_timestamp{0};
};

/**
 * Command: threads
 *
 * Lists all active thread IDs.
 */
struct Threads : public RDCPProcessedRequest {
  Threads() : RDCPProcessedRequest("threads") {}

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_MULTILINE_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }
    auto parsed = RDCPMultilineResponse(response->Data());
    for (const auto& item : parsed.lines) {
      threads.push_back(ParseInt32(item));
    }
  }

  std::vector<int> threads;
};

/**
 * Command: title [name="..." [dir="..."] [cmdline="..."] [persist]]
 *
 * Configures the title to be loaded on the next boot.
 */
struct LoadOnBootTitle : public RDCPProcessedRequest {
  LoadOnBootTitle() : RDCPProcessedRequest("title") { SetData(" none"); }

  explicit LoadOnBootTitle(
      const std::string& name,
      const std::optional<std::string>& directory = std::nullopt,
      const std::string& command_line = "", bool persist = false)
      : RDCPProcessedRequest("title") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\"");

    if (directory.has_value()) {
      AppendData(" dir=\"");
      AppendData(*directory);
      AppendData("\"");
    }

    if (!command_line.empty()) {
      AppendData(" cmdline=\"");
      AppendData(command_line);
      AppendData("\"");
    }

    if (persist) {
      AppendData(" persist");
    }
  }
};

/**
 * Command: title nopersist
 *
 * Clears the persistent title setting.
 */
struct LoadOnBootTitleUnpersist : public RDCPProcessedRequest {
  LoadOnBootTitleUnpersist() : RDCPProcessedRequest("title") {
    SetData(" nopersist");
  }
};

/**
 * Command: user
 *
 * Adds a user? Details TBD.
 */
struct User : public RDCPProcessedRequest {
  enum AccessRights {
    READ = 1,
    WRITE = 2,
    CONTROL = 4,
    CONFIG = 8,
    MANAGE = 16
  };

  // Add/Update user
  User(const std::string& name, int access_flags, uint64_t password = 0)
      : RDCPProcessedRequest("user") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\"");
    if (password != 0) {
      AppendData(" passwd=");
      AppendHexString(password);
    }
    if (access_flags & READ) {
      AppendData(" read");
    }
    if (access_flags & WRITE) {
      AppendData(" write");
    }
    if (access_flags & CONTROL) {
      AppendData(" control");
    }
    if (access_flags & CONFIG) {
      AppendData(" config");
    }
    if (access_flags & MANAGE) {
      AppendData(" manage");
    }
  }
};

/**
 * Command: user name="..." remove
 *
 * Removes a user.
 */
struct RemoveUser : public RDCPProcessedRequest {
  explicit RemoveUser(const std::string& name) : RDCPProcessedRequest("user") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\" remove");
  }
};

/**
 * Command: userlist
 *
 * Lists users on the system.
 */
struct UserList : public RDCPProcessedRequest {
  UserList() : RDCPProcessedRequest("userlist") {}
  // TODO: Parse response.
};

/**
 * Command: vssnap first=... last=... [flags=...] [marker=...]
 *
 * Captures a vertex shader snapshot
 */
struct VSSnap : public RDCPProcessedRequest {
  VSSnap(uint32_t first, uint32_t last, uint32_t flags = 0, uint32_t marker = 0)
      : RDCPProcessedRequest("vssnap") {
    SetData(" first=");
    AppendHexString(first);
    AppendData(" last=");
    AppendHexString(last);
    if (flags) {
      AppendData(" flags=");
      AppendHexString(flags);
    }
    if (marker) {
      AppendData(" marker=");
      AppendHexString(marker);
    }
  }
};

/**
 * Command: walkmem
 *
 * Walks the committed memory regions.
 */
struct WalkMem : public RDCPProcessedRequest {
  WalkMem() : RDCPProcessedRequest("walkmem") {}

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_MULTILINE_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }

    auto parsed = RDCPMultiMapResponse(response->Data());
    for (auto& it : parsed.maps) {
      regions.emplace_back(it);
    }
  }

  std::vector<MemoryRegion> regions;
};

/**
 * Command: writefile name="..." [trunc] [create]
 *
 * Writes data to a file.
 */
struct WriteFile : public RDCPProcessedRequest {
  WriteFile(const std::string& name, std::vector<uint8_t> buffer)
      : RDCPProcessedRequest("writefile"), binary_payload(std::move(buffer)) {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\"");
    AppendData(" length=");
    AppendHexString(static_cast<uint32_t>(binary_payload.size()));
  }

  [[nodiscard]] const std::vector<uint8_t>* BinaryPayload() override {
    return &binary_payload;
  }

  std::vector<uint8_t> binary_payload;
};

/**
 * Command: xbeinfo name="..." [dir="..."]
 *
 * Retrieves information about an XBE file.
 */
struct XBEInfo : public RDCPProcessedRequest {
  XBEInfo() : RDCPProcessedRequest("xbeinfo") { SetData(" running"); }

  explicit XBEInfo(const std::string& name, bool on_disk_only = false)
      : RDCPProcessedRequest("xbeinfo") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\"");

    if (on_disk_only) {
      AppendData(" ondiskonly");
    }
  }

  XBEInfo(const std::string& name, const std::string& dir)
      : RDCPProcessedRequest("xbeinfo") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\"");

    if (!dir.empty()) {
      AppendData(" dir=\"");
      AppendData(dir);
      AppendData("\"");
    }
  }

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_MULTILINE_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }
    auto parsed = RDCPMapResponse(response->Data());
    name = parsed.GetString("name");
    timestamp = parsed.GetDWORD("timestamp");
    checksum = parsed.GetDWORD("checksum");
  }

  std::string name;
  uint32_t timestamp{0};
  uint32_t checksum{0};
};

/**
 * Command: xtlinfo
 *
 * Retrieves information about the last XAPI error.
 */
struct XTLInfo : public RDCPProcessedRequest {
  XTLInfo() : RDCPProcessedRequest("xtlinfo") {}

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_MULTILINE_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }
    auto parsed = RDCPMapResponse(response->Data());
    last_err = parsed.GetDWORD("lasterr");
  }

  int last_err{0};
};

#endif  // XBDM_GDB_BRIDGE_XBDM_REQUESTS_H
