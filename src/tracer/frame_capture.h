#ifndef XBDM_GDB_BRIDGE_SRC_TRACER_FRAMECAPTURE_H_
#define XBDM_GDB_BRIDGE_SRC_TRACER_FRAMECAPTURE_H_

#include <filesystem>
#include <fstream>
#include <list>
#include <map>
#include <vector>

#include "tracer_xbox_shared.h"

class XBOXInterface;

namespace NTRCTracer {

class FrameCapture {
 public:
  //! Prepares this FrameCapture for use, creating artifacts within the given
  //! path.
  void Setup(const std::filesystem::path& artifact_path, bool verbose = false);

  //! Closes this capture and flushes any pending writes.
  void Close();

  enum class FetchResult {
    NO_DATA_AVAILABLE,
    ERROR,
    DATA_FETCHED,
  };
  //! Retrieves and consumes PGRAPH trace data from the given XBOX.
  FetchResult FetchPGRAPHTraceData(XBOXInterface& interface);

  //! Retrieves and consumes graphics trace information from the given XBOX.
  FetchResult FetchAuxTraceData(XBOXInterface& interface);

 private:
  //! Reads as many PushBufferCommandTraceInfo instances from
  //! pgraph_trace_buffer_ as possible, erasing consumed bytes.
  void ProcessPGRAPHBuffer();

  //! Writes information about the given packet to the nv2a_log.
  void LogPacket(const PushBufferCommandTraceInfo& packet);

  //! Reads as many aux data structures from aux_trace_buffer_ as possible,
  //! erasing consumed bytes.
  void ProcessAuxBuffer();

  void LogPGRAPH(const AuxDataHeader& packet, uint32_t data_len,
                 std::vector<uint8_t>::const_iterator data) const;

  void LogPFB(const AuxDataHeader& packet, uint32_t data_len,
              std::vector<uint8_t>::const_iterator data) const;

  void LogRDI(const AuxDataHeader& packet, uint32_t data_len,
              std::vector<uint8_t>::const_iterator data) const;

  void LogSurface(const AuxDataHeader& packet, uint32_t data_len,
                  std::vector<uint8_t>::const_iterator data) const;

  void LogTexture(const AuxDataHeader& packet, uint32_t data_len,
                  std::vector<uint8_t>::const_iterator data) const;

 public:
  //! Map of arbitrary ID to a vector of parameters for some PGRAPH command.
  std::map<uint32_t, std::vector<uint32_t>> pgraph_parameter_map;

  //! List of captured PGRAPH commands.
  std::list<PushBufferCommandTraceInfo> pgraph_commands;

 private:
  uint32_t next_free_id = 0;

  //! The path at which artifacts will be created.
  std::filesystem::path artifact_path_;

  //! Output stream for the nv2a_log.
  std::ofstream nv2a_log_;

  //! Whether or not to write verbose nv2a logs.
  bool verbose_logging_;

  //! Stores bytes that were not consumed as part of the last trace fetch.
  std::vector<uint8_t> pgraph_trace_buffer_;

  //! Stores bytes that were not consumed as part of the last aux fetch.
  std::vector<uint8_t> aux_trace_buffer_;
};

}  // namespace NTRCTracer

#endif  // XBDM_GDB_BRIDGE_SRC_TRACER_FRAMECAPTURE_H_
