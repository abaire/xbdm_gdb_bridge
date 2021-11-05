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
#include "rdcp/types/module.h"
#include "rdcp/types/thread_context.h"
#include "rdcp/xbdm_stop_reasons.h"
#include "util/parsing.h"

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

struct BreakBase_ : public RDCPProcessedRequest {
  BreakBase_() : RDCPProcessedRequest("break") {}
};

struct BreakAtStart : public BreakBase_ {
  BreakAtStart() : BreakBase_() { SetData(" start"); }
};

struct BreakClearAll : public BreakBase_ {
  BreakClearAll() : BreakBase_() { SetData(" clearall"); }
};

struct BreakAddress : public BreakBase_ {
  explicit BreakAddress(uint32_t address, bool clear = false) : BreakBase_() {
    if (clear) {
      SetData(" clear");
    }
    AppendData(" addr=");
    AppendHexString(address);
  }
};

struct BreakRange_ : public BreakBase_ {
  BreakRange_(const std::string& type, uint32_t address, uint32_t size = 0,
              bool clear = false)
      : BreakBase_() {
    if (clear) {
      SetData(" clear");
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

struct BreakOnRead : public BreakRange_ {
  explicit BreakOnRead(uint32_t address, int size = 0, bool clear = false)
      : BreakRange_("read", address, size, clear) {}
};

struct BreakOnWrite : public BreakRange_ {
  explicit BreakOnWrite(uint32_t address, int size = 0, bool clear = false)
      : BreakRange_("write", address, size, clear) {}
};

struct BreakOnExecute : public BreakRange_ {
  explicit BreakOnExecute(uint32_t address, int size = 0, bool clear = false)
      : BreakRange_("execute", address, size, clear) {}
};

struct Bye : public RDCPProcessedRequest {
  Bye() : RDCPProcessedRequest("bye") {}
};

struct ProfilerCaptureControl : public RDCPProcessedRequest {
  explicit ProfilerCaptureControl(bool start = true)
      : RDCPProcessedRequest("capctrl") {
    if (start) {
      SetData(" start");
    }
  }
};

struct Continue : public RDCPProcessedRequest {
  explicit Continue(int thread_id, bool exception = false)
      : RDCPProcessedRequest("continue") {
    SetData(" thread=");
    AppendHexString(thread_id);
    if (exception) {
      AppendData(" exception");
    }
  }
};

/*

# struct Crashdump(_ProcessedCommand):
#     """???."""
#
#     struct Response(_ProcessedResponse):
#         pass
#
#     construct():
#         RDCPProcessedRequest("crashdump") {
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

struct SetDevkitName : public RDCPProcessedRequest {
  explicit SetDevkitName(const std::string& new_name)
      : RDCPProcessedRequest("dbgname") {
    SetData(" name=");
    AppendData(new_name);
  }
};

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

struct DebugMode : public RDCPProcessedRequest {
  DebugMode() : RDCPProcessedRequest("debugmode") {}
  // TODO: Implement me.
};

/*
struct Dedicate : public RDCPProcessedRequest {
    """Sets connection as dedicated."""

    struct Response(_ProcessedRawBodyResponse):
        pass

    construct(global_enable=None, handler_name=None):
        RDCPProcessedRequest("dedicate") {
        if global_enable:
            self.body = b" global"
        elif handler_name:
            self.body = bytes(f' handler="{handler_name}"', "utf-8")
*/

/*
struct DefTitle : public RDCPProcessedRequest {
    """???"""

    struct Response(_ProcessedRawBodyResponse):
        pass

    construct(
        self,
        launcher: bool = false,
        name: Optional[str] = None,
        directory: Optional[str] = None,
        ,
    ):
        RDCPProcessedRequest("deftitle") {

        if not name:
            self.body = b" none"
            return

        if launcher:
            self.body = b" launcher"
            return

        if not name:
            raise ValueError("Missing required 'name' parameter.")

        if not directory:
            raise ValueError("Missing required 'directory' parameter.")

        body = f' name="{name}" dir="{directory}"'
        self.body = bytes(body, "utf-8")
*/

struct Delete : public RDCPProcessedRequest {
  explicit Delete(const std::string& path, bool is_directory = false)
      : RDCPProcessedRequest("delete") {
    SetData("name=\"");
    AppendData(path);
    AppendData("\"");
    if (is_directory) {
      AppendData(" dir");
    }
  }
};

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

struct DriveFreeSpace : public RDCPProcessedRequest {
  explicit DriveFreeSpace(const std::string& drive_letter)
      : RDCPProcessedRequest("drivefreespace") {
    SetData(" name=");
    AppendData(drive_letter);
    if (drive_letter.size() == 1) {
      AppendData(") {\\");
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
    free_to_caller = parsed.GetQWORD("freetocallerlo", "freetocallerhi");
    total_bytes = parsed.GetQWORD("totalbyteslo", "totalbyteshi");
    free_bytes = parsed.GetQWORD("totalfreebyteslo", "totalfreebyteshi");
  }

  int64_t free_to_caller{0};
  int64_t total_bytes{0};
  int64_t free_bytes{0};
};

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

struct FuncCall : public RDCPProcessedRequest {
  explicit FuncCall(int thread_id) : RDCPProcessedRequest("funccall") {
    SetData(" thread=");
    AppendHexString(thread_id);
  }
};

struct GetContext : public RDCPProcessedRequest {
  explicit GetContext(int thread_id, bool enable_control = false,
                      bool enable_integer = false, bool enable_float = false)
      : RDCPProcessedRequest("getcontext") {
    SetData(" thread=");
    AppendHexString(thread_id);

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

/*
struct GetD3DState : public RDCPProcessedRequest {
    """Retrieves the current D3D state."""

    struct Response(_ProcessedResponse):
        construct(response: rdcp_response.RDCPResponse):
            RDCPProcessedRequest(response)

            self.printable_data = ""
            self.data = bytes()

            if not self.ok:
                return

            self.data = response.data
            # TODO: Parse the response data and drop this.
            self.printable_data = binascii.hexlify(self.data)

        @property
        def ok(self):
            return self.status ==
rdcp_response.RDCPResponse.STATUS_BINARY_RESPONSE

        @property
        def _body_str(self) -> str:
            return f"{self.printable_data}"

    construct(
        self,
        ,
    ):
        RDCPProcessedRequest("getd3dstate") {
        self._binary_response_length = 1180
*/

struct GetExtContext : public RDCPProcessedRequest {
  explicit GetExtContext(int thread_id)
      : RDCPProcessedRequest("getextcontext") {
    SetData(" thread=");
    AppendHexString(thread_id);
  }

  [[nodiscard]] long ExpectedBinaryResponseSize() const override {
    return RDCPResponse::kBinaryInt32SizePrefix;
  }
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

struct GetFile : public RDCPProcessedRequest {
  explicit GetFile(const std::string& path) : RDCPProcessedRequest("getfile") {
    SetData(" name=\"");
    AppendData(path);
    AppendData("\"");
  }

  GetFile(const std::string& path, int32_t offset, int32_t size)
      : RDCPProcessedRequest("getfile") {
    SetData(" name=\"");
    AppendData(path);
    AppendData("\" offset=");
    AppendHexString(offset);
    AppendData(" size=");
    AppendHexString(size);
  }

  [[nodiscard]] long ExpectedBinaryResponseSize() const override {
    return RDCPResponse::kBinaryInt32SizePrefix;
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

struct GetFileAttributes : public RDCPProcessedRequest {
  explicit GetFileAttributes(const std::string& path)
      : RDCPProcessedRequest("getfileattributes") {
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

    auto parsed = RDCPMapResponse(response->Data());
    filesize = parsed.GetQWORD("sizelo", "sizehi");
    create_timestamp = parsed.GetQWORD("createlo", "createhi");
    change_timestamp = parsed.GetQWORD("changelo", "changehi");
    flags = parsed.valueless_keys;
  }

  int64_t filesize{0};
  int64_t create_timestamp{0};
  int64_t change_timestamp{0};
  std::set<std::string> flags;
};

struct GetGamma : public RDCPProcessedRequest {
  GetGamma() : RDCPProcessedRequest("getgamma") {}

  [[nodiscard]] long ExpectedBinaryResponseSize() const override { return 768; }
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

/*
struct GetMem : public RDCPProcessedRequest {
    """Gets the contents of a block of memory."""

    struct Response(_ProcessedResponse):
        construct(response: rdcp_response.RDCPResponse):
            RDCPProcessedRequest(response)

            self.printable_data = ""
            self.data = bytes()

            if not self.ok:
                return

            self.printable_data, self.data = response.parse_hex_data()

        @property
        def ok(self):
            return self.status ==
rdcp_response.RDCPResponse.STATUS_MULTILINE_RESPONSE

        @property
        def _body_str(self) -> str:
            return f"{self.printable_data}"

    construct(addr, length):
        RDCPProcessedRequest("getmem") {
        addr = "0x%X" % addr
        length = "0x%X" % length
        self.body = bytes(f" addr={addr} length={length}", "utf-8")
*/

struct GetMemBinary : public RDCPProcessedRequest {
  GetMemBinary(uint32_t addr, uint32_t length)
      : RDCPProcessedRequest("getmem2"), length(length) {
    SetData(" ADDR=");
    AppendHexString(addr);
    AppendData(" LENGTH=");
    AppendHexString(length);
  }

  [[nodiscard]] long ExpectedBinaryResponseSize() const override {
    return length;
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

  uint32_t length;
  std::vector<uint8_t> data;
};

/*
struct GetPalette : public RDCPProcessedRequest {
    """Retrieves palette information (D3DINT_GET_PALETTE)."""

    struct Response(_ProcessedRawBodyResponse):
        # TODO: Implement. Calling on the dashboard gives an error.
        pass

    construct(stage: int):
        RDCPProcessedRequest("getpalette") {
        self.body = bytes(" STAGE=0x%X" % stage, "utf-8")
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

struct GetChecksum : public RDCPProcessedRequest {
  GetChecksum(uint32_t addr, uint32_t len, uint32_t blocksize)
      : RDCPProcessedRequest("getsum") {
    assert((addr & 0x07) == 0);
    assert((len & 0x07) == 0);
    assert((blocksize & 0x07) == 0);

    SetData(" ADDR=");
    AppendHexString(addr);
    AppendData(" LENGTH=");
    AppendHexString(len);
    AppendData(" BLOCKSIZE=");
    AppendHexString(blocksize);

    length = len / blocksize * 4;
  }

  [[nodiscard]] long ExpectedBinaryResponseSize() const override {
    return length;
  }
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

/*
struct GetSurface : public RDCPProcessedRequest {
    """???"""

    struct Response(_ProcessedRawBodyResponse):
        pass

    construct(surface_id: int):
        RDCPProcessedRequest("getsurf") {
        self.body = bytes(f" id=0x%X" % surface_id, "utf-8")
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

struct Go : public RDCPProcessedRequest {
  Go() : RDCPProcessedRequest("go") {}
};

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

struct Halt : public RDCPProcessedRequest {
  Halt() : RDCPProcessedRequest("halt") {}
  explicit Halt(int thread_id) : RDCPProcessedRequest("halt") {
    SetData(" thread=");
    AppendHexString(thread_id);
  }
};

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

struct IsStopped : public RDCPProcessedRequest {
  explicit IsStopped(int thread_id) : RDCPProcessedRequest("isstopped") {
    SetData(" thread=");
    AppendHexString(thread_id);
  }

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_BINARY_RESPONSE ||
           status == StatusCode::ERR_NOT_STOPPED;
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
    assert(delimiter != data.end());

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

/*
struct IRTSweep : public RDCPProcessedRequest {
   """???"""

   struct Response(_ProcessedRawBodyResponse):
       pass

   construct():
       RDCPProcessedRequest("irtsweep") {
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

/*
# struct KeyExchange(_ProcessedCommand):
#     """???"""
#
#     struct Response(_ProcessedRawBodyResponse):
#         pass
#
#     construct(keydata: bytes):
#         RDCPProcessedRequest("keyxchg") {
#         self._binary_payload = keydata
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

struct Mkdir : public RDCPProcessedRequest {
  explicit Mkdir(const std::string& name) : RDCPProcessedRequest("mkdir") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\"");
  }
};

struct ModLongName : public RDCPProcessedRequest {
  explicit ModLongName(const std::string& name)
      : RDCPProcessedRequest("modlong") {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\"");
  }

  // TODO: Parse response.
};

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

  explicit ModSections(const std::string& path)
      : RDCPProcessedRequest("modsections") {
    SetData(" path=\"");
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

struct NoStopOn : public StopOnBase_ {
  explicit NoStopOn(uint32_t events = kAll) : StopOnBase_("nostopon", events) {}
};

struct StopOn : public StopOnBase_ {
  explicit StopOn(uint32_t events = kAll) : StopOnBase_("stopon", events) {}
};

/*
struct Notify : public RDCPProcessedRequest {
    """Registers connection as a notification channel."""

    struct Response(_ProcessedResponse):
        pass

    construct():
        RDCPProcessedRequest("notify") {
        self._dedicate_notification_mode = true
*/

struct NotifyAt : public RDCPProcessedRequest {
  explicit NotifyAt(uint16_t port, bool drop_flag = false,
                    bool debug_flag = false)
      : RDCPProcessedRequest("notifyat"), port(port) {
    SetData(" Port=");
    AppendHexString(port);
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

/*
struct PBSnap : public RDCPProcessedRequest {
    """Takes a D3D snapshot (binary must be compiled as debug or profile)."""

    struct Response(_ProcessedRawBodyResponse):
        pass

    construct():
        RDCPProcessedRequest("pbsnap") {
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

struct PDBInfo : public RDCPProcessedRequest {
  explicit PDBInfo(uint32_t address) : RDCPProcessedRequest("pdbinfo") {
    SetData(" addr=");
    AppendHexString(address);
  }

  // TODO: Parse response.
};

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

struct Resume : public RDCPProcessedRequest {
  explicit Resume(int thread_id) : RDCPProcessedRequest("resume") {
    SetData(" thread=");
    AppendHexString(thread_id);
  }
};

/*
struct Screenshot : public RDCPProcessedRequest {
    """Captures a screenshot from the device."""

    struct Response(_ProcessedResponse):
        construct(response: rdcp_response.RDCPResponse):
            RDCPProcessedRequest(response)

            self.printable_data = ""
            self.data = bytes()

            if not self.ok:
                return

            self.printable_data, self.data = response.parse_hex_data()

        @property
        def ok(self):
            return self.status ==
rdcp_response.RDCPResponse.STATUS_MULTILINE_RESPONSE

        @property
        def _body_str(self) -> str:
            return f"{self.printable_data}"

    construct():
        RDCPProcessedRequest("screenshot") {
*/

struct SendFile : public RDCPProcessedRequest {
  SendFile(const std::string& name, std::vector<uint8_t> buffer)
      : RDCPProcessedRequest("sendfile"), binary_payload(std::move(buffer)) {
    SetData(" name=\"");
    AppendData(name);
    AppendData("\" length=");
    AppendHexString(static_cast<uint32_t>(buffer.size()));
  }

  [[nodiscard]] const std::vector<uint8_t>* BinaryPaylod() override {
    return &binary_payload;
  }

  std::vector<uint8_t> binary_payload;
};

/*
# struct ServiceName(_ProcessedCommand):
#     """???"""
#
#     struct Response(_ProcessedRawBodyResponse):
#         pass
#
#     construct():
#         RDCPProcessedRequest("servname") {
#         # id(int) name(string)
#         # name must begin with one of (prod|part|test)
#         # There's a second mode that looks like it can take a command string
that matches some internal state var
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

  [[nodiscard]] const std::vector<uint8_t>* BinaryPaylod() override {
    return &binary_payload;
  }

  std::vector<uint8_t> binary_payload;
};

struct SetFileAttributes : public RDCPProcessedRequest {
  explicit SetFileAttributes(
      const std::string& path, std::optional<bool> readonly = std::nullopt,
      std::optional<bool> hidden = std::nullopt,
      std::optional<uint64_t> create_timestamp = std::nullopt,
      std::optional<uint64_t> change_timestamp = std::nullopt)
      : RDCPProcessedRequest("setfileattributes") {
    SetData(" name=\"");
    AppendData(path);
    AppendData("\"");

    if (readonly.has_value()) {
      AppendData(" readonly=");
      if (*readonly) {
        AppendData("1");
      } else {
        AppendData("0");
      }
    }

    if (hidden.has_value()) {
      AppendData(" hidden=");
      if (*hidden) {
        AppendData("1");
      } else {
        AppendData("0");
      }
    }

    if (create_timestamp.has_value()) {
      AppendData(" createlo=");
      AppendHexString(static_cast<uint32_t>(*create_timestamp & 0xFFFFFFFF));
      AppendData(" createhi=");
      AppendHexString(
          static_cast<uint32_t>((*create_timestamp >> 32) & 0xFFFFFFFF));
    }

    if (change_timestamp.has_value()) {
      AppendData(" changelo=");
      AppendHexString(static_cast<uint32_t>(*change_timestamp & 0xFFFFFFFF));
      AppendData(" changehi=");
      AppendHexString(
          static_cast<uint32_t>((*change_timestamp >> 32) & 0xFFFFFFFF));
    }
  }
};

struct SetMem : public RDCPProcessedRequest {
  SetMem(uint32_t address, const std::vector<uint8_t>& data)
      : RDCPProcessedRequest("setmem") {
    SetData(" addr=");
    AppendHexString(address);
    AppendData(" data=");
    AppendHexBuffer(data);
  }

  SetMem(uint32_t address, const std::string& hex_data)
      : RDCPProcessedRequest("setmem") {
    SetData(" addr=");
    AppendHexString(address);
    AppendData(" data=");
    AppendData(hex_data);
  }
};

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

struct Stop : public RDCPProcessedRequest {
  Stop() : RDCPProcessedRequest("stop") {}
};

struct Suspend : public RDCPProcessedRequest {
  explicit Suspend(int thread_id) : RDCPProcessedRequest("suspend") {
    SetData(" thread=");
    AppendHexString(thread_id);
  }
};

/*
# sysfileupd - Looks like this may be invoking a system update?
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

struct LoadOnBootTitleUnpersist : public RDCPProcessedRequest {
  LoadOnBootTitleUnpersist() : RDCPProcessedRequest("title") {
    SetData(" nopersist");
  }
};

struct UserList : public RDCPProcessedRequest {
  UserList() : RDCPProcessedRequest("userlist") {}
  // TODO: Parse response.
};

/*
struct VSSnap : public RDCPProcessedRequest {
    """Takes a D3D snapshot (binary must be compiled as debug)."""

    struct Response(_ProcessedRawBodyResponse):
        pass

    construct(
        first: int, last: int, flags: int = 0, marker: int = 0
    ):
        RDCPProcessedRequest("vssnap") {
        self.body = bytes(" first=0x%X last=0x%X" % (first, last), "utf-8")
        if flags:
            self.body += b" flags=0x%X" % flags
        if marker:
            self.body += b" marker=0x%X" % marker
*/

struct WalkMem : public RDCPProcessedRequest {
  struct Region {
    uint32_t base;
    uint32_t size;
    uint32_t protect;
    std::set<std::string> flags;
  };

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
      Region r{
          .base = it.GetUInt32("base"),
          .size = it.GetUInt32("size"),
          .protect = it.GetUInt32("protect"),
          .flags = it.valueless_keys,
      };
      regions.emplace_back(r);
    }
  }

  std::vector<Region> regions;
};

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

  [[nodiscard]] bool IsOK() const override {
    return status == StatusCode::OK_MULTILINE_RESPONSE;
  }

  void ProcessResponse(const std::shared_ptr<RDCPResponse>& response) override {
    if (!IsOK()) {
      return;
    }
    auto parsed = RDCPMapResponse(response->Data());
    name = parsed.GetString("name");
    timestamp = parsed.GetUInt32("timestamp");
    checksum = parsed.GetUInt32("checksum");
  }

  std::string name;
  uint32_t timestamp{0};
  uint32_t checksum{0};
};

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
