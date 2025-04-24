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

//! Enumerates the possible states of a PushBufferCommandParameters struct.
//!
//! Keep in sync with pushbuffer_command.h
typedef enum PBCPDataState {
  PBCPDS_INVALID = 0,
  PBCPDS_SMALL_BUFFER = 1,
  PBCPDS_HEAP_BUFFER = 2,
} PBCPDataState;

//! Holds the parameter data for a PushBufferCommand.
//!
//! Keep in sync with pushbuffer_command.h
typedef struct PushBufferCommandParameters {
  //! A value from PBCPDataState indicating what data, if any, is valid in this
  //! struct.
  uint32_t data_state;
  union {
    //! Contains the parameters inline.
    uint32_t buffer[4];

    // The `heap_buffer` value is only valid on the XBOX itself so this field is
    // repurposed to link into the local data map.
    uint32_t data_id;
  } data;
} __attribute((packed)) PushBufferCommandParameters;

//! Encapsulates information about a single PGRAPH command.
//!
//! Keep in sync with pushbuffer_command.h
typedef struct PushBufferCommandTraceInfo {
  //! Whether the data contained in this struct is valid or not.
  uint32_t valid;

  //! The arbitrary packet index, used to match the packet with associated
  //! captures (e.g., framebuffer dumps).
  uint32_t packet_index;

  //! The number of BEGIN_END(end) calls since the trace began.
  uint32_t draw_index;

  //! The actual command.
  PushBufferCommand command;

  //! The address from which this packet was read.
  uint32_t address;

  //! The PGRAPH graphics class for this packet (e.g., 0x97 for 3D).
  uint32_t graphics_class;

  // Parameters passed to the command, if any.
  // If populated, this will always be exactly (command.parameter_count * 4)
  // bytes.
  PushBufferCommandParameters data;

  //! Address to return to in response to a DMA return command.
  //! This value must be initialized to zero to detect (unsupported) nested
  //! subroutines.
  uint32_t subroutine_return_address;
} __attribute((packed)) PushBufferCommandTraceInfo;

//! Describes some auxiliary buffer data type.
//!
//! Keep in sync with pgraph_command_callbacks.h
typedef enum AuxDataType {
  //! A raw dump of the PGRAPH region.
  ADT_PGRAPH_DUMP,
  //! A raw dump of the PFB region.
  ADT_PFB_DUMP,
  //! A raw dump of the RDI data.
  ADT_RDI_DUMP,
  //! A surface buffer of some sort.
  ADT_SURFACE,
  //! A texture.
  ADT_TEXTURE,
} AuxDataType;

//! Header describing an entry in the auxiliary data stream.
//!
//! Keep in sync with pgraph_command_callbacks.h
typedef struct AuxDataHeader {
  //! The index of the PushBufferCommandTraceInfo packet with which this data is
  //! associated.
  uint32_t packet_index;

  //! The draw count of the PushBufferCommandTraceInfo packet with which this
  //! data is associated.
  uint32_t draw_index;

  //! A value from AuxDataType indicating the type of data.
  uint32_t data_type;

  //! The length of the data, which starts immediately following this header.
  uint32_t len;
} __attribute((packed)) AuxDataHeader;

//! Header describing RDI data.
//!
//! Keep in sync with pgraph_command_callbacks.h
typedef struct RDIHeader {
  //! The offset from which the following RDI values were read.
  uint32_t offset;
  //! The number of 32-bit values that follow this struct.
  uint32_t count;
} __attribute((packed)) RDIHeader;

//! Describes the application of a surface.
//!
//! Keep in sync with pgraph_command_callbacks.h
typedef enum SurfaceType {
  ST_COLOR,
  ST_DEPTH,
} SurfaceType;

//! Header describing surface data.
//!
//! Keep in sync with pgraph_command_callbacks.h
typedef struct SurfaceHeader {
  //! The intended use of this surface.
  uint32_t type;

  //! The format of this surface (e.g., A8R8G8B8).
  uint32_t format;

  //! The number of ASCII characters immediately following this header
  //! containing a description of the content.
  uint32_t description_len;
  //! The number of image bytes immediately following the description
  //! characters.
  uint32_t len;

  uint32_t width;
  uint32_t height;
  uint32_t pitch;

  uint32_t clip_x;
  uint32_t clip_y;
  uint32_t clip_width;
  uint32_t clip_height;

  //! Whether this surface is swizzled or not.
  uint32_t swizzle;
  uint32_t swizzle_param;
} __attribute((packed)) SurfaceHeader;

//! Header describing texture data.
//!
//! Keep in sync with pgraph_command_callbacks.h
typedef struct TextureHeader {
  //! The texture unit/stage that this texture is associated with.
  uint32_t stage;

  //! The layer index of this texture.
  uint32_t layer;

  //! The number of image bytes immediately following this header.
  uint32_t len;

  uint32_t format;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t pitch;
  //! The value of the control0 register.
  uint32_t control0;
  //! The value of the control1 register.
  uint32_t control1;

  //! Packed image width ((x >> 16) & 0x1FFF) | height (x & 0x1FFF).
  uint32_t image_rect;
} __attribute((packed)) TextureHeader;

}  // namespace NTRCTracer

#endif  // XBDM_GDB_BRIDGE_SRC_TRACER_TRACER_XBOX_SHARED_H_
