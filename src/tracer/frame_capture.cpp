#include "frame_capture.h"

#include "dyndxt_loader/dyndxt_requests.h"
#include "ntrc_dyndxt.h"
#include "util/logging.h"
#include "xbox/xbox_interface.h"

namespace NTRCTracer {

constexpr const char kLoggingTagTracer[] = "TRC_FC";
#define LOG_CAP(lvl) LOG_TAGGED(lvl, kLoggingTagTracer)

void FrameCapture::Setup(const std::filesystem::path &artifact_path,
                         bool verbose) {
  artifact_path_ = artifact_path;
  verbose_logging_ = verbose;
  nv2a_log_ = std::ofstream(artifact_path_ / "nv2a_log.txt",
                            std::ios_base::out | std::ios_base::trunc);
  nv2a_log_ << "pgraph method log from nvtrc" << std::endl;

  pgraph_parameter_map.clear();
  pgraph_commands.clear();
}

void FrameCapture::Close() { nv2a_log_.close(); }

FrameCapture::FetchResult FrameCapture::FetchPGRAPHTraceData(
    XBOXInterface &interface) {
  auto request =
      std::make_shared<DynDXTLoader::InvokeReceiveSizePrefixedBinary>(
          NTRC_HANDLER_NAME "!read_pgraph=0x4000");
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    // A notification of data availability may have triggered this fetch while a
    // read operation retrieved the data, so it is not considered an error for
    // data to be unavailable.
    if (request->status == ERR_DATA_NOT_AVAILABLE) {
      return FetchResult::NO_DATA_AVAILABLE;
    }
    LOG_CAP(error) << *request << std::endl;
    return FetchResult::ERROR;
  }

  auto &data = request->response_data;
  if (data.empty()) {
    return FetchResult::NO_DATA_AVAILABLE;
  }

  pgraph_trace_buffer_.insert(std::end(pgraph_trace_buffer_), std::begin(data),
                              std::end(data));
  ProcessPGRAPHBuffer();

  return FetchResult::DATA_FETCHED;
}

FrameCapture::FetchResult FrameCapture::FetchGraphicsTraceData(
    XBOXInterface &interface) {
  auto request =
      std::make_shared<DynDXTLoader::InvokeReceiveSizePrefixedBinary>(
          NTRC_HANDLER_NAME "!read_graphics=0x1000000");
  interface.SendCommandSync(request);
  if (!request->IsOK()) {
    // A notification of data availability may have triggered this fetch while a
    // read operation retrieved the data, so it is not considered an error for
    // data to be unavailable.
    if (request->status == ERR_DATA_NOT_AVAILABLE) {
      return FetchResult::NO_DATA_AVAILABLE;
    }
    LOG_CAP(error) << *request << std::endl;
    return FetchResult::ERROR;
  }

  auto &data = request->response_data;
  if (data.empty()) {
    return FetchResult::NO_DATA_AVAILABLE;
  }

  // TODO: Process retrieved graphics buffer data.

  return FetchResult::DATA_FETCHED;
}

void FrameCapture::ProcessPGRAPHBuffer() {
  const auto packet_size = sizeof(PushBufferCommandTraceInfo);
  while (pgraph_trace_buffer_.size() >= packet_size) {
    PushBufferCommandTraceInfo packet;
    memcpy(&packet, pgraph_trace_buffer_.data(), packet_size);
    auto packet_end = pgraph_trace_buffer_.begin() + packet_size;

    // data_id is currently set to the XBOX-side address of the parameter data
    // buffer, unless the parameters were discarded, in which case no parameter
    // data will be available to be read.
    if (packet.command.valid && packet.data.data_state == PBCPDS_HEAP_BUFFER &&
        packet.command.parameter_count) {
      auto additional_data_size = 4 * packet.command.parameter_count;
      if (pgraph_trace_buffer_.size() < packet_size + additional_data_size) {
        return;
      }

      // TODO: Allow slots to be recycled to avoid overflow.
      //       In practice the buffer should fit more frames than a typical
      //       capture would ever need.
      assert(next_free_id < 0xFFFFFFFF);

      auto data_id = next_free_id++;
      const auto *params = reinterpret_cast<const uint32_t *>(
          pgraph_trace_buffer_.data() + packet_size);
      pgraph_parameter_map.emplace(
          data_id, std::vector<uint32_t>(
                       params, params + packet.command.parameter_count));

      packet_end += additional_data_size;
      packet.data.data.data_id = data_id;
    }

    pgraph_trace_buffer_.erase(pgraph_trace_buffer_.begin(), packet_end);
    pgraph_commands.emplace_back(packet);

    LogPacket(packet);
  }
}

void FrameCapture::LogPacket(const PushBufferCommandTraceInfo &packet) {
  nv2a_log_ << "nv2a_pgraph_method " << packet.command.subchannel << ": 0x"
            << std::hex << packet.graphics_class << " -> 0x"
            << packet.command.method;

  const uint32_t *data = nullptr;
  switch (packet.data.data_state) {
    case PBCPDS_INVALID:
      nv2a_log_ << " <NO_DATA>";
      break;

    case PBCPDS_SMALL_BUFFER:
      data = packet.data.data.buffer;
      break;

    case PBCPDS_HEAP_BUFFER:
      data = pgraph_parameter_map[packet.data.data.data_id].data();
      break;
  }

  if (data) {
    for (auto i = 0; i < packet.command.parameter_count; ++i) {
      nv2a_log_ << " 0x" << std::hex << data[i];
    }
  }
  nv2a_log_ << std::endl;

  if (verbose_logging_) {
    nv2a_log_ << "  Detailed info:" << std::endl;
    nv2a_log_ << "    Address: 0x" << std::hex << packet.address << std::endl;
    nv2a_log_ << "    Method: 0x" << std::hex << packet.command.method
              << std::endl;
    nv2a_log_ << "    Non increasing: "
              << (packet.command.non_increasing ? "TRUE" : "FALSE")
              << std::endl;
    nv2a_log_ << "    Subchannel: 0x" << std::hex << packet.command.subchannel
              << std::endl;
    if (data) {
      for (auto i = 0; i < packet.command.parameter_count; ++i) {
        nv2a_log_ << "    Param[" << (i + 1) << "]: 0x" << std::hex << data[i]
                  << std::endl;
      }
    }
    nv2a_log_ << std::endl;
  }
}

}  // namespace NTRCTracer
