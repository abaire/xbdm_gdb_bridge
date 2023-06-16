// Types that are shared between the host and the tracer running on the XBOX.
#ifndef XBDM_GDB_BRIDGE_SRC_TRACER_TRACER_XBOX_SHARED_H_
#define XBDM_GDB_BRIDGE_SRC_TRACER_TRACER_XBOX_SHARED_H_

#include <cstdint>

namespace NTRCTracer {

//! Provides details about a PGRAPH command.
//!
//! Keep in sync with pushbuffer_command.h
typedef struct PushBufferCommand {
  //! Whether the data contained in this struct is valid or not. Command structs
  //! may be invalid due to an error or because the command was a jump.
  uint32_t valid;

  //! Whether processing this command should automatically increment the target
  //! address.
  uint32_t non_increasing;

  //! The ID of the method. E.g., `NV097_FLIP_STALL`.
  uint32_t method;

  //! The subchannel of the method.
  uint32_t subchannel;

  //! The number of parameters to the method.
  uint32_t parameter_count;
} __attribute((packed)) PushBufferCommand;

//! Encapsulates information about a single PGRAPH command.
//!
//! Keep in sync with pushbuffer_command.h
typedef struct PushBufferCommandTraceInfo {
  //! Whether the data contained in this struct is valid or not.
  uint32_t valid;

  //! The arbitrary packet index, used to match the packet with associated
  //! captures (e.g., framebuffer dumps).
  uint32_t packet_index;

  //! The actual command.
  PushBufferCommand command;

  //! The address from which this packet was read.
  uint32_t address;

  //! The PGRAPH graphics class for this packet (e.g., 0x97 for 3D).
  uint32_t graphics_class;

  // The data param is only valid on the XBOX itself so this field is repurposed
  // to link into the data map.
  uint32_t data_id;
} __attribute((packed)) PushBufferCommandTraceInfo;

}  // namespace NTRCTracer

#endif  // XBDM_GDB_BRIDGE_SRC_TRACER_TRACER_XBOX_SHARED_H_
