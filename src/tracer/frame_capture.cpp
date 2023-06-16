#include "frame_capture.h"

#include "dyndxt_loader/dyndxt_requests.h"
#include "ntrc_dyndxt.h"
#include "util/logging.h"
#include "xbox/xbox_interface.h"

namespace NTRCTracer {

constexpr const char kLoggingTagTracer[] = "TRC_FC";
#define LOG_CAP(lvl) LOG_TAGGED(lvl, kLoggingTagTracer)

bool FrameCapture::FetchPGRAPHTraceData(XBOXInterface &interface) {
  auto request =
      std::make_shared<DynDXTLoader::InvokeReceiveSizePrefixedBinary>(
          NTRC_HANDLER_NAME "!read_pgraph=0x4000");
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    // A notification of data availability may have triggered this fetch while a
    // read operation retrieved the data, so it is not considered an error for
    // data to be unavailable.
    if (request->status == ERR_DATA_NOT_AVAILABLE) {
      return true;
    }
    LOG_CAP(error) << *request << std::endl;
    return false;
  }

  auto &data = request->response_data;
  if (data.empty()) {
    return true;
  }

  trace_buffer.insert(std::end(trace_buffer), std::begin(data), std::end(data));
  ProcessBuffer();

  return true;
}

void FrameCapture::Reset() {
  pgraph_parameter_map.clear();
  pgraph_commands.clear();
}

void FrameCapture::ProcessBuffer() {
  const auto packet_size = sizeof(PushBufferCommandTraceInfo);
  while (trace_buffer.size() >= packet_size) {
    PushBufferCommandTraceInfo packet;
    memcpy(&packet, trace_buffer.data(), packet_size);
    auto packet_end = trace_buffer.begin() + packet_size;

    // data_id is currently set to the XBOX-side address of the parameter data
    // buffer, unless the parameters were discarded, in which case no parameter
    // data will be available to be read.
    if (packet.command.valid && packet.data_id &&
        packet.command.parameter_count) {
      auto additional_data_size = 4 * packet.command.parameter_count;
      if (trace_buffer.size() < packet_size + additional_data_size) {
        return;
      }

      // TODO: Allow slots to be recycled to avoid overflow.
      //       In practice the buffer should fit more frames than a typical
      //       capture would ever need.
      assert(next_free_id < 0xFFFFFFFF);

      auto data_id = next_free_id++;
      const auto *params =
          reinterpret_cast<const uint32_t *>(trace_buffer.data() + packet_size);
      pgraph_parameter_map.emplace(
          data_id, std::vector<uint32_t>(
                       params, params + packet.command.parameter_count));

      trace_buffer.erase(trace_buffer.begin(),
                         packet_end + additional_data_size);
      packet.data_id = data_id;
    } else {
      trace_buffer.erase(trace_buffer.begin(), packet_end);
    }

    pgraph_commands.emplace_back(packet);
  }
}

}  // namespace NTRCTracer
