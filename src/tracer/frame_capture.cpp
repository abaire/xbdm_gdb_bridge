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
          NTRC_HANDLER_NAME "!read_pgraph maxsize=0x100000");
  interface.SendCommandSync(request, NTRC_HANDLER_NAME);
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

FrameCapture::FetchResult FrameCapture::FetchAuxTraceData(
    XBOXInterface &interface) {
  // The aux buffer is read in its entirety, since a single read may not be
  // sufficient to retrieve the data that spawned the triggering notification.
  bool has_fetched_data = false;
  while (true) {
    auto request =
        std::make_shared<DynDXTLoader::InvokeReceiveSizePrefixedBinary>(
            NTRC_HANDLER_NAME "!read_aux maxsize=0x1000000");
    interface.SendCommandSync(request, NTRC_HANDLER_NAME);
    if (!request->IsOK()) {
      if (request->status == ERR_DATA_NOT_AVAILABLE) {
        break;
      }
      LOG_CAP(error) << *request << std::endl;
      return FetchResult::ERROR;
    }

    auto &data = request->response_data;
    if (data.empty()) {
      break;
    }

    has_fetched_data = true;
    aux_trace_buffer_.insert(std::end(aux_trace_buffer_), std::begin(data),
                             std::end(data));
    ProcessAuxBuffer();
  }

  return has_fetched_data ? FetchResult::DATA_FETCHED
                          : FetchResult::NO_DATA_AVAILABLE;
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
  auto log = [this, &packet](uint32_t method, const uint32_t *param) {
    nv2a_log_ << "nv2a_pgraph_method " << std::dec << packet.command.subchannel
              << ": 0x" << std::hex << packet.graphics_class << " -> 0x"
              << method;

    if (!param) {
      nv2a_log_ << " <NO_DATA>";
    } else {
      nv2a_log_ << " 0x" << std::hex << *param;
    }
    nv2a_log_ << std::endl;
  };

  const uint32_t *data = nullptr;
  switch (packet.data.data_state) {
    case PBCPDS_INVALID:
      break;

    case PBCPDS_SMALL_BUFFER:
      data = packet.data.data.buffer;
      break;

    case PBCPDS_HEAP_BUFFER:
      data = pgraph_parameter_map[packet.data.data.data_id].data();
      break;
  }

  uint32_t method = packet.command.method;
  for (auto i = 0; i < packet.command.parameter_count; ++i) {
    const uint32_t *param = nullptr;
    if (data) {
      param = data + i;
    }
    log(method, param);
    if (!packet.command.non_increasing) {
      method += 4;
    }
  }

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

void FrameCapture::ProcessAuxBuffer() {
  const auto header_size = sizeof(AuxDataHeader);
  while (aux_trace_buffer_.size() >= header_size) {
    AuxDataHeader packet;
    memcpy(&packet, aux_trace_buffer_.data(), header_size);
    auto packet_end = aux_trace_buffer_.begin() + header_size;

    // Check to see if the packet's data is fully retrieved yet.
    if (aux_trace_buffer_.size() < (header_size + packet.len)) {
      return;
    }

    switch (packet.data_type) {
      case ADT_PGRAPH_DUMP:
        LogPGRAPH(packet, packet.len - header_size, packet_end);
        break;

      case ADT_PFB_DUMP:
        LogPFB(packet, packet.len - header_size, packet_end);
        LOG_CAP(error) << "TODO: Save PFB";
        break;

      case ADT_RDI_DUMP:
        LogRDI(packet, packet.len - header_size, packet_end);
        break;

      case ADT_SURFACE:
        LogSurface(packet, packet.len - header_size, packet_end);
        break;

      case ADT_TEXTURE:
        LogTexture(packet, packet.len - header_size, packet_end);
        break;

      default:
        LOG_CAP(error) << "Skipping unsupported auxiliary packet of type "
                       << packet.data_type;
        break;
    }

    packet_end += packet.len;
    aux_trace_buffer_.erase(aux_trace_buffer_.begin(), packet_end);
  }
}

void FrameCapture::LogPGRAPH(const AuxDataHeader &packet, uint32_t data_len,
                             std::vector<uint8_t>::const_iterator data) const {
  char filename[64];
  snprintf(filename, sizeof(filename), "%010u_PGRAPH.bin", packet.packet_index);

  auto os = std::ofstream(artifact_path_ / filename, std::ios_base::out |
                                                         std::ios_base::trunc |
                                                         std::ios_base::binary);

  const char *d = reinterpret_cast<const char *>(&data[0]);
  os.write(d, data_len);
  os.close();
}

void FrameCapture::LogPFB(const NTRCTracer::AuxDataHeader &packet,
                          uint32_t data_len,
                          std::vector<uint8_t>::const_iterator data) const {
  char filename[64];
  snprintf(filename, sizeof(filename), "%010u_PFB.bin", packet.packet_index);

  auto os = std::ofstream(artifact_path_ / filename, std::ios_base::out |
                                                         std::ios_base::trunc |
                                                         std::ios_base::binary);

  const char *d = reinterpret_cast<const char *>(&data[0]);
  os.write(d, data_len);
  os.close();
}

void FrameCapture::LogRDI(const NTRCTracer::AuxDataHeader &packet,
                          uint32_t data_len,
                          std::vector<uint8_t>::const_iterator data) const {
  const char *d = reinterpret_cast<const char *>(&data[0]);
  auto header = reinterpret_cast<const RDIHeader *>(d);

  std::string region;
  switch (header->offset) {
    case 0x100000:
      region = "Shader";
      break;

    case 0x170000:
      region = "Constants_0";
      break;

    case 0xCC0000:
      region = "Constants_1";
      break;

    default: {
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "UNKNOWN_0x%08X", header->offset);
      region = buffer;
    } break;
  }

  char filename[64];
  snprintf(filename, sizeof(filename), "%010u_RDI_%s.bin", packet.packet_index,
           region.c_str());

  auto os = std::ofstream(artifact_path_ / filename, std::ios_base::out |
                                                         std::ios_base::trunc |
                                                         std::ios_base::binary);

  d += sizeof(*header);
  auto len_without_header =
      static_cast<std::streamsize>(data_len - sizeof(*header));
  os.write(d, len_without_header);
  os.close();
}

void FrameCapture::LogSurface(const NTRCTracer::AuxDataHeader &packet,
                              uint32_t data_len,
                              std::vector<uint8_t>::const_iterator data) const {
  const char *d = reinterpret_cast<const char *>(&data[0]);
  auto header = reinterpret_cast<const SurfaceHeader *>(d);

  std::string surface_type;
  switch (header->type) {
    case ST_COLOR:
      surface_type = "Color";
      break;
    case ST_DEPTH:
      surface_type = "Depth";
      break;
    default: {
      LOG_CAP(error) << "Unknown surface type " << header->type;
      char buf[32];
      snprintf(buf, sizeof(buf), "UNKNOWN_%d", header->type);
      surface_type = buf;
    }
  }

  d += sizeof(*header);
  data_len -= sizeof(*header);

  if (header->description_len) {
    char filename[64];
    snprintf(filename, sizeof(filename), "%010u_Surface_%s.txt",
             packet.packet_index, surface_type.c_str());

    auto os = std::ofstream(artifact_path_ / filename,
                            std::ios_base::out | std::ios_base::trunc);
    auto description = std::string(d, d + header->description_len);
    os << "[\"surface\": {" << std::endl;
    os << R"("description": ")" << description << "\"," << std::endl;
    os << R"("type": ")" << surface_type << "\"," << std::endl;
    os << R"("width": )" << header->width << "," << std::endl;
    os << R"("height": )" << header->height << "," << std::endl;
    os << R"("pitch": )" << header->pitch << std::endl;
    os << "}" << std::endl;
    os.close();
  }
  d += header->description_len;
  data_len -= header->description_len;

  {
    char filename[64];
    snprintf(filename, sizeof(filename), "%010u_Surface_%s.bin",
             packet.packet_index, surface_type.c_str());
    auto os = std::ofstream(
        artifact_path_ / filename,
        std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);

    os.write(d, data_len);
    os.close();
  }
}

void FrameCapture::LogTexture(const NTRCTracer::AuxDataHeader &packet,
                              uint32_t data_len,
                              std::vector<uint8_t>::const_iterator data) const {
  const char *d = reinterpret_cast<const char *>(&data[0]);
  auto header = reinterpret_cast<const TextureHeader *>(d);

  d += sizeof(*header);
  data_len -= sizeof(*header);

  {
    char filename[64];
    snprintf(filename, sizeof(filename), "%010u_Texture_%d_%d.txt",
             packet.packet_index, header->stage, header->layer);

    auto os = std::ofstream(artifact_path_ / filename,
                            std::ios_base::out | std::ios_base::trunc);
    os << "[\"texture\": {" << std::endl;
    os << R"("stage": ")" << header->stage << "\"," << std::endl;
    os << R"("layer": ")" << header->layer << "\"," << std::endl;
    os << R"("width": )" << header->width << "," << std::endl;
    os << R"("height": )" << header->height << "," << std::endl;
    os << R"("depth": )" << header->depth << "," << std::endl;
    os << R"("pitch": )" << header->pitch << "," << std::endl;
    os << R"("format": )" << header->format << std::endl;
    os << R"("format_hex": "0x)" << std::hex << std::setw(8)
       << std::setfill('0') << header->format << "\"," << std::endl;

    os << R"("control0": )" << header->control0 << std::endl;
    os << R"("control0_hex": "0x)" << std::hex << std::setw(8)
       << std::setfill('0') << header->control0 << "\"," << std::endl;

    os << R"("control1": )" << header->control1 << std::endl;
    os << R"("control1_hex": "0x)" << std::hex << std::setw(8)
       << std::setfill('0') << header->control1 << "\"," << std::endl;

    os << "}" << std::endl;
    os.close();
  }

  {
    char filename[64];
    snprintf(filename, sizeof(filename), "%010u_Texture_%d_%d.bin",
             packet.packet_index, header->stage, header->layer);
    auto os = std::ofstream(
        artifact_path_ / filename,
        std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
    os.write(d, data_len);
    os.close();
  }
}

}  // namespace NTRCTracer
