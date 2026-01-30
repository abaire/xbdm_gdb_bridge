#include <boost/test/unit_test.hpp>
#include <filesystem>
#include <fstream>
#include <vector>

#include "tracer/frame_capture.h"
#include "tracer/tracer_xbox_shared.h"

namespace NTRCTracer {

class FrameCaptureTestFixture {
 public:
  FrameCaptureTestFixture() {
    char temp_dir_template[] = "/tmp/frame_capture_test_XXXXXX";
    char* temp_dir_path = mkdtemp(temp_dir_template);
    artifact_path = temp_dir_path;
    capture.Setup(artifact_path);
  }

  ~FrameCaptureTestFixture() {
    capture.Close();
    std::filesystem::remove_all(artifact_path);
  }

  void AddPGRAPHPacket(const PushBufferCommandTraceInfo& packet,
                       const std::vector<uint32_t>& params = {}) {
    auto& buffer = GetPGRAPHBuffer();
    const uint8_t* start = reinterpret_cast<const uint8_t*>(&packet);
    buffer.insert(buffer.end(), start, start + sizeof(packet));

    if (!params.empty()) {
      const uint8_t* pstart = reinterpret_cast<const uint8_t*>(params.data());
      buffer.insert(buffer.end(), pstart,
                    pstart + params.size() * sizeof(uint32_t));
    }
  }

  void AddAuxPacket(const AuxDataHeader& header,
                    const std::vector<uint8_t>& data) {
    auto& buffer = GetAuxBuffer();
    const uint8_t* start = reinterpret_cast<const uint8_t*>(&header);
    buffer.insert(buffer.end(), start, start + sizeof(header));
    buffer.insert(buffer.end(), data.begin(), data.end());
  }

  void ProcessPGRAPH() {
    // We need to call the private method. For now we will rely on friend access
    // once we add it to the header.
    CallProcessPGRAPHBuffer(capture);
  }

  void ProcessAux() { CallProcessAuxBuffer(capture); }

  std::vector<uint8_t>& GetPGRAPHBuffer() {
    return capture.pgraph_trace_buffer_;
  }

  std::vector<uint8_t>& GetAuxBuffer() { return capture.aux_trace_buffer_; }

  // Helper functions to call private methods and access private members.
  // These will be implemented in a way that requires friendship.
  static void CallProcessPGRAPHBuffer(FrameCapture& fc);
  static void CallProcessAuxBuffer(FrameCapture& fc);

  std::filesystem::path artifact_path;
  FrameCapture capture;
};

// Implementation of helper functions that will be friended.
void FrameCaptureTestFixture::CallProcessPGRAPHBuffer(FrameCapture& fc) {
  fc.ProcessPGRAPHBuffer();
}

void FrameCaptureTestFixture::CallProcessAuxBuffer(FrameCapture& fc) {
  fc.ProcessAuxBuffer();
}

BOOST_FIXTURE_TEST_SUITE(frame_capture_suite, FrameCaptureTestFixture)

BOOST_AUTO_TEST_CASE(test_process_pgraph_single_packet) {
  PushBufferCommandTraceInfo packet = {0};
  packet.valid = 1;
  packet.packet_index = 123;
  packet.command.parameter_count = 0;

  AddPGRAPHPacket(packet);
  ProcessPGRAPH();

  BOOST_TEST(capture.pgraph_commands.size() == 1);
  auto packet_index = capture.pgraph_commands.front().packet_index;
  BOOST_TEST(packet_index == 123);
  BOOST_TEST(GetPGRAPHBuffer().empty());
}

BOOST_AUTO_TEST_CASE(test_process_pgraph_multiple_packets) {
  for (int i = 0; i < 5; ++i) {
    PushBufferCommandTraceInfo packet = {0};
    packet.valid = 1;
    packet.packet_index = i;
    packet.command.parameter_count = 0;
    AddPGRAPHPacket(packet);
  }

  ProcessPGRAPH();

  BOOST_TEST(capture.pgraph_commands.size() == 5);
  BOOST_TEST(GetPGRAPHBuffer().empty());

  uint32_t i = 0;
  for (auto const& cmd : capture.pgraph_commands) {
    BOOST_TEST(cmd.packet_index == i++);
  }
}

BOOST_AUTO_TEST_CASE(test_process_pgraph_partial_packet) {
  PushBufferCommandTraceInfo packet = {0};
  packet.valid = 1;
  packet.packet_index = 456;
  packet.command.parameter_count = 0;

  // Add only half a packet
  auto& buffer = GetPGRAPHBuffer();
  const uint8_t* start = reinterpret_cast<const uint8_t*>(&packet);
  buffer.insert(buffer.end(), start, start + sizeof(packet) / 2);

  ProcessPGRAPH();

  BOOST_TEST(capture.pgraph_commands.empty());
  BOOST_TEST(buffer.size() == sizeof(packet) / 2);

  // Add the rest
  buffer.insert(buffer.end(), start + sizeof(packet) / 2,
                start + sizeof(packet));
  ProcessPGRAPH();

  BOOST_TEST(capture.pgraph_commands.size() == 1);
  auto packet_index = capture.pgraph_commands.front().packet_index;
  BOOST_TEST(packet_index == 456);
  BOOST_TEST(buffer.empty());
}

BOOST_AUTO_TEST_CASE(test_process_pgraph_with_params) {
  PushBufferCommandTraceInfo packet = {0};
  packet.valid = 1;
  packet.packet_index = 789;
  packet.command.parameter_count = 2;
  packet.command.valid = 1;
  packet.data.data_state = PBCPDS_HEAP_BUFFER;

  std::vector<uint32_t> params = {0xDEADBEEF, 0xCAFEBABE};
  AddPGRAPHPacket(packet, params);

  ProcessPGRAPH();

  BOOST_TEST(capture.pgraph_commands.size() == 1);
  BOOST_TEST(GetPGRAPHBuffer().empty());

  auto data_id = capture.pgraph_commands.front().data.data.data_id;
  BOOST_TEST(capture.pgraph_parameter_map.count(data_id) == 1);
  BOOST_TEST(capture.pgraph_parameter_map[data_id] == params);
}

BOOST_AUTO_TEST_CASE(test_process_aux_single_packet) {
  AuxDataHeader header = {0};
  header.packet_index = 111;
  header.data_type = ADT_PGRAPH_DUMP;
  header.len = 4;

  std::vector<uint8_t> data = {1, 2, 3, 4};
  AddAuxPacket(header, data);

  ProcessAux();

  BOOST_TEST(GetAuxBuffer().empty());
  // We can't easily check the output file without knowing the name,
  // but we verified the buffer is consumed.
}

BOOST_AUTO_TEST_CASE(test_process_aux_partial_packet) {
  AuxDataHeader header = {0};
  header.packet_index = 222;
  header.data_type = ADT_PGRAPH_DUMP;
  header.len = 10;

  std::vector<uint8_t> data(10, 0xAA);

  // Add header but only some data
  auto& buffer = GetAuxBuffer();
  const uint8_t* hstart = reinterpret_cast<const uint8_t*>(&header);
  buffer.insert(buffer.end(), hstart, hstart + sizeof(header));
  buffer.insert(buffer.end(), data.begin(), data.begin() + 5);

  ProcessAux();

  BOOST_TEST(buffer.size() == sizeof(header) + 5);

  // Add the rest
  buffer.insert(buffer.end(), data.begin() + 5, data.end());
  ProcessAux();

  BOOST_TEST(buffer.empty());
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace NTRCTracer
