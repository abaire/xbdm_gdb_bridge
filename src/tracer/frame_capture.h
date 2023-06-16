#ifndef XBDM_GDB_BRIDGE_SRC_TRACER_FRAMECAPTURE_H_
#define XBDM_GDB_BRIDGE_SRC_TRACER_FRAMECAPTURE_H_

#include <list>
#include <map>
#include <vector>

#include "tracer_xbox_shared.h"

class XBOXInterface;

namespace NTRCTracer {

class FrameCapture {
 public:
  void Reset();

  bool FetchPGRAPHTraceData(XBOXInterface &interface);

 private:
  //! Reads as many PushBufferCommandTraceInfo instances from trace_buffer as
  //! possible, erasing consumed bytes.
  void ProcessBuffer();

 public:
  uint32_t next_free_id = 0;

  //! Map of arbitrary ID to a vector of parameters for some PGRAPH command.
  std::map<uint32_t, std::vector<uint32_t>> pgraph_parameter_map;

  //! List of captured PGRAPH commands.
  std::list<PushBufferCommandTraceInfo> pgraph_commands;

 private:
  //! Stores bytes that were not consumed as part of the last trace fetch.
  std::vector<uint8_t> trace_buffer;
};

}  // namespace NTRCTracer

#endif  // XBDM_GDB_BRIDGE_SRC_TRACER_FRAMECAPTURE_H_
