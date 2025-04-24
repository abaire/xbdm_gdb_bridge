#include "frame_capture.h"

#include "dyndxt_loader/dyndxt_requests.h"
#include "lodepng.h"
#include "ntrc_dyndxt.h"
extern "C" {
#include "swizzle.h"
}
#include "image_util.h"
#include "util/logging.h"
#include "xbox/xbox_interface.h"

namespace NTRCTracer {

constexpr const char kLoggingTagTracer[] = "TRC_FC";
#define LOG_CAP(lvl) LOG_TAGGED(lvl, kLoggingTagTracer)

static constexpr uint32_t SURFACE_FORMAT_Y8 = 0x01;
static constexpr uint32_t SURFACE_FORMAT_X1R5G5B5_Z1R5G5B5 = 0x02;
static constexpr uint32_t SURFACE_FORMAT_X1R5G5B5_O1R5G5B5 = 0x03;
static constexpr uint32_t SURFACE_FORMAT_A1R5G5B5 = 0x04;
static constexpr uint32_t SURFACE_FORMAT_R5G6B5 = 0x05;
static constexpr uint32_t SURFACE_FORMAT_Y16 = 0x06;
static constexpr uint32_t SURFACE_FORMAT_X8R8G8B8_Z8R8G8B8 = 0x07;
static constexpr uint32_t SURFACE_FORMAT_X8R8G8B8_O1Z7R8G8B8 = 0x08;
static constexpr uint32_t SURFACE_FORMAT_X1A7R8G8B8_Z1A7R8G8B8 = 0x09;
static constexpr uint32_t SURFACE_FORMAT_X1A7R8G8B8_O1A7R8G8B8 = 0x0A;
static constexpr uint32_t SURFACE_FORMAT_X8R8G8B8_O8R8G8B8 = 0x0B;
static constexpr uint32_t SURFACE_FORMAT_A8R8G8B8 = 0x0C;

struct SurfaceFormatDefinition {
  uint32_t bytes_per_pixel;
  std::shared_ptr<uint8_t[]> (*converter)(const void *src, uint32_t src_size);
  bool has_alpha;
};

static const std::map<uint32_t, SurfaceFormatDefinition> kSurfaceFormats = {
    {SURFACE_FORMAT_Y8, {1, nullptr, false}},
    {SURFACE_FORMAT_X1R5G5B5_Z1R5G5B5, {2, A1R5G5B5ToRGBA888, true}},
    {SURFACE_FORMAT_X1R5G5B5_O1R5G5B5, {2, A1R5G5B5ToRGBA888, true}},
    {SURFACE_FORMAT_A1R5G5B5, {2, A1R5G5B5ToRGBA888, true}},
    {SURFACE_FORMAT_R5G6B5, {2, RGB565ToRGB88, false}},
    {SURFACE_FORMAT_Y16, {2, nullptr, false}},
    {SURFACE_FORMAT_X8R8G8B8_Z8R8G8B8, {4, BGRAToRGBA, true}},
    {SURFACE_FORMAT_X8R8G8B8_O1Z7R8G8B8, {4, BGRAToRGBA, true}},
    {SURFACE_FORMAT_X1A7R8G8B8_Z1A7R8G8B8, {4, BGRAToRGBA, true}},
    {SURFACE_FORMAT_X1A7R8G8B8_O1A7R8G8B8, {4, BGRAToRGBA, true}},
    {SURFACE_FORMAT_X8R8G8B8_O8R8G8B8, {4, BGRAToRGBA, true}},
    {SURFACE_FORMAT_A8R8G8B8, {4, BGRAToRGBA, true}},
};

static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_Y8 = 0x00;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_AY8 = 0x01;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A1R5G5B5 = 0x02;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X1R5G5B5 = 0x03;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A4R4G4B4 = 0x04;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R5G6B5 = 0x05;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8R8G8B8 = 0x06;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X8R8G8B8 = 0x07;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_I8_A8R8G8B8 = 0x0B;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT1_A1R5G5B5 = 0x0C;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT23_A8R8G8B8 =
    0x0E;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT45_A8R8G8B8 =
    0x0F;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A1R5G5B5 =
    0x10;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R5G6B5 = 0x11;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8R8G8B8 =
    0x12;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_Y8 = 0x13;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R8B8 = 0x16;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_G8B8 = 0x17;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8 = 0x19;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8Y8 = 0x1A;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_AY8 = 0x1B;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X1R5G5B5 =
    0x1C;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A4R4G4B4 =
    0x1D;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X8R8G8B8 =
    0x1E;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8 = 0x1F;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8Y8 = 0x20;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_CR8YB8CB8YA8 =
    0x24;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_YB8CR8YA8CB8 =
    0x25;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R6G5B5 = 0x27;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_G8B8 = 0x28;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R8B8 = 0x29;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_DEPTH_Y16_FIXED =
    0x2C;
static constexpr uint32_t
    NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_X8_Y24_FIXED = 0x2E;
static constexpr uint32_t
    NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_Y16_FIXED = 0x30;
static constexpr uint32_t
    NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_Y16_FLOAT = 0x31;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_Y16 = 0x35;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8B8G8R8 = 0x3A;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_B8G8R8A8 = 0x3B;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R8G8B8A8 = 0x3C;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8B8G8R8 =
    0x3F;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_B8G8R8A8 =
    0x40;
static constexpr uint32_t NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R8G8B8A8 =
    0x41;

struct TextureFormatDefinition {
  bool swizzled;
  uint32_t bytes_per_pixel;
  bool compressed;
  std::shared_ptr<uint8_t[]> (*converter)(const void *src, uint32_t src_size);
  bool has_alpha;
};
static const std::map<uint32_t, TextureFormatDefinition> kTextureFormats = {
    {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_Y8, {true, 1, false, nullptr, false}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A1R5G5B5,
     {true, 2, false, A1R5G5B5ToRGBA888, true}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X1R5G5B5,
     {true, 2, false, AXR5G5B5ToRGB888, false}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A4R4G4B4,
     {true, 2, false, A4R4G4B4ToRGBA888, true}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R5G6B5,
     {true, 2, false, RGB565ToRGB88, false}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A1R5G5B5,
     {false, 2, false, A1R5G5B5ToRGBA888, true}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R5G6B5,
     {false, 2, false, RGB565ToRGB88, false}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X1R5G5B5,
     {false, 2, false, AXR5G5B5ToRGB888, false}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A4R4G4B4,
     {false, 2, false, A4R4G4B4ToRGBA888, true}},

    {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_I8_A8R8G8B8,
     {true, 1, false, nullptr, true}},

    {NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT1_A1R5G5B5,
     {false, 2, true, nullptr, true}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT23_A8R8G8B8,
     {false, 4, true, nullptr, true}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT45_A8R8G8B8,
     {false, 4, true, nullptr, true}},

    {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8R8G8B8,
     {true, 4, false, BGRAToRGBA, true}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8R8G8B8,
     {false, 4, false, BGRAToRGBA, true}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X8R8G8B8,
     {true, 4, false, BGRAToRGBA, false}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X8R8G8B8,
     {false, 4, false, BGRAToRGBA, false}},

    {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8B8G8R8,
     {true, 4, false, nullptr, true}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8B8G8R8,
     {false, 4, false, nullptr, true}},

    {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_B8G8R8A8,
     {false, 4, false, ARGBToRGBA, true}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_B8G8R8A8,
     {true, 4, false, ARGBToRGBA, true}},

    {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R8G8B8A8,
     {true, 4, false, ABGRToRGBA, true}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R8G8B8A8,
     {false, 4, false, ABGRToRGBA, true}},

    {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_Y8,
     {false, 1, false, nullptr, false}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_Y16,
     {false, 2, false, nullptr, false}},

    // {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R8B8,
    //  {false, 2, false, nullptr, false}},
    // {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_G8B8,
    //  {false, 2, false, nullptr, false}},
    //
    {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8, {true, 1, false, nullptr, false}},
    // {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8Y8, {true, 2, false, nullptr,
    // true}}, {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_AY8,
    //  {false, 2, false, nullptr, true}},
    // {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8,
    //  {false, 1, false, nullptr, false}},
    // {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8Y8,
    //  {false, 2, false, nullptr, true}},

    {NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_CR8YB8CB8YA8,
     {false, 4, false, nullptr, true}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_YB8CR8YA8CB8,
     {false, 4, false, nullptr, true}},

    {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R6G5B5,
     {true, 2, false, nullptr, false}},
    // {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_G8B8, {true, 2, false, nullptr,
    // false}},
    // {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R8B8, {true, 2, false,
    // nullptr, false}},
    // {NV097_SET_TEXTURE_FORMAT_COLOR_SZ_DEPTH_Y16_FIXED,
    //  {true, 2, false, nullptr, false}},
    // {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_Y16_FIXED,
    //  {false, 2, false, nullptr, false}},
    // {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_Y16_FLOAT,
    //  {false, 2, false, nullptr, false}},
    {NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_X8_Y24_FIXED,
     {false, 4, false, nullptr, true}},
};  // namespace NTRCTracer

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
        LogPGRAPH(packet, packet.len, packet_end);
        break;

      case ADT_PFB_DUMP:
        LogPFB(packet, packet.len, packet_end);
        LOG_CAP(error) << "TODO: Save PFB" << std::endl;
        break;

      case ADT_RDI_DUMP:
        LogRDI(packet, packet.len, packet_end);
        break;

      case ADT_SURFACE:
        LogSurface(packet, packet.len, packet_end);
        break;

      case ADT_TEXTURE:
        LogTexture(packet, packet.len, packet_end);
        break;

      default:
        LOG_CAP(error) << "Skipping unsupported auxiliary packet of type "
                       << packet.data_type << std::endl;
        break;
    }

    packet_end += packet.len;
    aux_trace_buffer_.erase(aux_trace_buffer_.begin(), packet_end);
  }
}

void FrameCapture::LogPGRAPH(const AuxDataHeader &packet, uint32_t data_len,
                             std::vector<uint8_t>::const_iterator data) const {
  char filename[64];
  snprintf(filename, sizeof(filename), "%010u_%u_PGRAPH.bin",
           packet.packet_index, packet.draw_index);

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
  snprintf(filename, sizeof(filename), "%010u_%u_PFB.bin", packet.packet_index,
           packet.draw_index);

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
  snprintf(filename, sizeof(filename), "%010u_%u_RDI_%s.bin",
           packet.packet_index, packet.draw_index, region.c_str());

  auto os = std::ofstream(artifact_path_ / filename, std::ios_base::out |
                                                         std::ios_base::trunc |
                                                         std::ios_base::binary);

  d += sizeof(*header);
  auto len_without_header =
      static_cast<std::streamsize>(data_len - sizeof(*header));
  os.write(d, len_without_header);
  os.close();
}

static void SaveSurfaceImage(const void *raw, uint32_t data_len,
                             std::ofstream &os, uint32_t surface_type,
                             uint32_t width, uint32_t height, uint32_t pitch,
                             bool swizzle) {
  auto surface_format_entry = kSurfaceFormats.find(surface_type);
  assert(surface_format_entry != kSurfaceFormats.end());
  const auto &surface_format = surface_format_entry->second;

  std::unique_ptr<uint8_t[]> buffer;

  if (swizzle) {
    buffer = std::unique_ptr<uint8_t[]>(new uint8_t[data_len]);
    unswizzle_rect(static_cast<const uint8_t *>(raw), width, height,
                   buffer.get(), pitch, surface_format.bytes_per_pixel);
  }

  const uint8_t *input =
      buffer ? buffer.get() : static_cast<const uint8_t *>(raw);
  std::shared_ptr<uint8_t[]> converted_buffer;
  if (surface_format.converter) {
    converted_buffer = surface_format.converter(input, data_len);
    input = converted_buffer.get();
  }

  std::vector<unsigned char> png_data;
  unsigned error = 0;

  if (surface_type == SURFACE_FORMAT_Y8 || surface_type == SURFACE_FORMAT_Y16) {
    error = lodepng::encode(png_data, input, width, height, LCT_GREY,
                            surface_format.bytes_per_pixel * 8);
  } else if (surface_format.has_alpha) {
    error = lodepng::encode(png_data, input, width, height, LCT_RGBA, 8);
  } else {
    error = lodepng::encode(png_data, input, width, height, LCT_RGB, 8);
  }

  if (error) {
    auto error_message = lodepng_error_text(error);
    LOG_CAP(error) << " PNG encoding failed " << error_message << std::endl;
  } else {
    os.write(reinterpret_cast<const char *>(png_data.data()), png_data.size());
  }
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
      LOG_CAP(error) << "Unknown surface type " << header->type << std::endl;
      char buf[32];
      snprintf(buf, sizeof(buf), "UNKNOWN_%d", header->type);
      surface_type = buf;
    }
  }

  d += sizeof(*header);
  data_len -= sizeof(*header);
  uint32_t surface_format = 0;

  if (header->description_len) {
    char filename[64];
    snprintf(filename, sizeof(filename), "%010u_%u_Surface_%s.txt",
             packet.packet_index, packet.draw_index, surface_type.c_str());

    auto os = std::ofstream(artifact_path_ / filename,
                            std::ios_base::out | std::ios_base::trunc);
    auto description = std::string(d, d + header->description_len);
    os << "{" << std::endl;

    os << "  \"surface\": {" << std::endl;
    os << R"(    "description": ")" << description << "\"," << std::endl;
    os << R"(    "draw": )" << packet.draw_index << "," << std::endl;
    os << R"(    "type": ")" << surface_type << "\"," << std::endl;
    os << R"(    "format_hex": "0x)" << std::hex << std::setw(8)
       << std::setfill('0') << header->format << std::dec << "\"," << std::endl;
    os << R"(    "swizzle": )" << header->swizzle << "," << std::endl;
    os << R"(    "format_hex": "0x)" << std::hex << std::setw(8)
       << std::setfill('0') << header->swizzle_param << std::dec << "\","
       << std::endl;
    os << R"(    "clip_x": )" << header->clip_x << "," << std::endl;
    os << R"(    "clip_y": )" << header->clip_y << "," << std::endl;
    os << R"(    "clip_width": )" << header->clip_width << "," << std::endl;
    os << R"(    "clip_height": )" << header->clip_height << "," << std::endl;
    os << R"(    "width": )" << header->width << "," << std::endl;
    os << R"(    "height": )" << header->height << "," << std::endl;
    os << R"(    "pitch": )" << header->pitch << std::endl;

    os << "  }" << std::endl;
    os << "}" << std::endl;
    os.close();

    auto format_start = description.find("format ");
    if (format_start != std::string::npos) {
      auto format_substring = description.substr(format_start + 7);
      auto format_end = format_substring.find(',');
      if (format_end != std::string::npos) {
        format_substring = format_substring.substr(0, format_end);
        surface_format = std::stoul(format_substring, nullptr, 16);
      }
    }
  }
  d += header->description_len;
  data_len -= header->description_len;

  {
    char filename[64];
    snprintf(filename, sizeof(filename), "%010u_%u_Surface_%s.bin",
             packet.packet_index, packet.draw_index, surface_type.c_str());
    auto os = std::ofstream(
        artifact_path_ / filename,
        std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);

    os.write(d, data_len);
    os.close();

    if (surface_format) {
      snprintf(filename, sizeof(filename), "%010u_%u_Surface_%s.png",
               packet.packet_index, packet.draw_index, surface_type.c_str());
      os = std::ofstream(artifact_path_ / filename, std::ios_base::out |
                                                        std::ios_base::trunc |
                                                        std::ios_base::binary);
      SaveSurfaceImage(d, data_len, os, surface_format, header->width,
                       header->height, header->pitch, header->swizzle);
      os.close();
    }
  }
}

static void SaveTextureImage(const void *raw, uint32_t data_len,
                             std::ofstream &os, uint32_t texture_type,
                             const TextureFormatDefinition &texture_format,
                             uint32_t mipmap_count, uint32_t width,
                             uint32_t height, uint32_t depth, uint32_t pitch) {
  std::unique_ptr<uint8_t[]> buffer;

  if (texture_format.swizzled) {
    buffer = std::unique_ptr<uint8_t[]>(new uint8_t[data_len]);
    unswizzle_rect(static_cast<const uint8_t *>(raw), width, height,
                   buffer.get(), pitch, texture_format.bytes_per_pixel);
  }

  const uint8_t *input =
      buffer ? buffer.get() : static_cast<const uint8_t *>(raw);

  std::shared_ptr<uint8_t[]> converted_buffer;
  if (texture_format.converter) {
    converted_buffer = texture_format.converter(input, data_len);
    input = converted_buffer.get();
  }

  std::vector<uint8_t> png_data;
  uint32_t error = 0;

  if (texture_format.compressed) {
    auto compression = DXTCompression::INVALID;
    if (texture_type == NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT1_A1R5G5B5) {
      compression = DXTCompression::DXT1;
    } else if (texture_type ==
               NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT23_A8R8G8B8) {
      compression = DXTCompression::DXT3;
    } else if (texture_type ==
               NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT45_A8R8G8B8) {
      compression = DXTCompression::DXT5;
    }
    error = EncodeDDS(png_data, input, data_len, width, height, compression);
  } else if (texture_type == NV097_SET_TEXTURE_FORMAT_COLOR_SZ_Y8 ||
             texture_type == NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_Y16 ||
             texture_type == NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8) {
    error = lodepng::encode(png_data, input, width, height, LCT_GREY,
                            texture_format.bytes_per_pixel * 8);
  } else if (texture_format.has_alpha) {
    error = lodepng::encode(png_data, input, width, height, LCT_RGBA, 8);
  } else {
    error = lodepng::encode(png_data, input, width, height, LCT_RGB, 8);
  }

  if (error) {
    auto error_message = lodepng_error_text(error);
    LOG_CAP(error) << " PNG encoding failed " << error_message << std::endl;
  } else {
    os.write(reinterpret_cast<const char *>(png_data.data()), png_data.size());
  }
}

void FrameCapture::LogTexture(const NTRCTracer::AuxDataHeader &packet,
                              uint32_t data_len,
                              std::vector<uint8_t>::const_iterator data) const {
  const char *d = reinterpret_cast<const char *>(&data[0]);
  auto header = reinterpret_cast<const TextureHeader *>(d);

  d += sizeof(*header);
  data_len -= sizeof(*header);

  uint32_t texture_type = (header->format >> 8) & 0x7F;
  uint32_t mipmap_levels = (header->format >> 16) & 0x0F;

  {
    char filename[64];
    snprintf(filename, sizeof(filename), "%010u_%u_Texture_%d_%d.txt",
             packet.packet_index, packet.draw_index, header->stage,
             header->layer);

    auto os = std::ofstream(artifact_path_ / filename,
                            std::ios_base::out | std::ios_base::trunc);
    os << "{" << std::endl;
    os << "  \"texture\": {" << std::endl;
    os << R"(    "stage": ")" << header->stage << "\"," << std::endl;
    os << R"(    "layer": ")" << header->layer << "\"," << std::endl;
    os << R"(    "draw": )" << packet.draw_index << "," << std::endl;
    os << R"(    "width": )" << header->width << "," << std::endl;
    os << R"(    "height": )" << header->height << "," << std::endl;
    os << R"(    "depth": )" << header->depth << "," << std::endl;
    os << R"(    "pitch": )" << header->pitch << "," << std::endl;
    os << R"(    "mipmap_levels": )" << mipmap_levels << "," << std::endl;
    os << R"(    "type_hex": "0x)" << std::hex << std::setw(8)
       << std::setfill('0') << texture_type << std::dec << "\"," << std::endl;
    os << R"(    "format": )" << header->format << "," << std::endl;
    os << R"(    "format_hex": "0x)" << std::hex << std::setw(8)
       << std::setfill('0') << header->format << std::dec << "\"," << std::endl;
    os << R"(    "imagerect_hex": "0x)" << std::hex << std::setw(8)
       << std::setfill('0') << header->image_rect << std::dec << "\","
       << std::endl;
    os << R"(    "control0": )" << header->control0 << "," << std::endl;
    os << R"(    "control0_hex": "0x)" << std::hex << std::setw(8)
       << std::setfill('0') << header->control0 << std::dec << "\","
       << std::endl;
    os << R"(    "control1": )" << header->control1 << "," << std::endl;
    os << R"(    "control1_hex": "0x)" << std::hex << std::setw(8)
       << std::setfill('0') << header->control1 << std::dec << "\""
       << std::endl;

    os << "  }" << std::endl;
    os << "}" << std::endl;
    os.close();
  }

  {
    char filename[64];
    snprintf(filename, sizeof(filename), "%010u_%u_Texture_%d_%d.bin",
             packet.packet_index, packet.draw_index, header->stage,
             header->layer);
    auto os = std::ofstream(
        artifact_path_ / filename,
        std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
    os.write(d, data_len);
    os.close();

    auto texture_format_entry = kTextureFormats.find(texture_type);
    assert(texture_format_entry != kTextureFormats.end());
    const auto &texture_format = texture_format_entry->second;

    snprintf(filename, sizeof(filename), "%010u_%u_Texture_%d_%d.%s",
             packet.packet_index, packet.draw_index, header->stage,
             header->layer, texture_format.compressed ? "dds" : "png");
    os = std::ofstream(artifact_path_ / filename, std::ios_base::out |
                                                      std::ios_base::trunc |
                                                      std::ios_base::binary);
    SaveTextureImage(d, data_len, os, texture_type, texture_format,
                     mipmap_levels, header->width, header->height,
                     header->depth, header->pitch);
    os.close();
  }
}

}  // namespace NTRCTracer
