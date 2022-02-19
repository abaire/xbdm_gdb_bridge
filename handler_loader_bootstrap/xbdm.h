#ifndef XBDM_GDB_BRIDGE_XBDM_H
#define XBDM_GDB_BRIDGE_XBDM_H

#include "windefs.h"

struct CommandContext;

// Main processor procedure.
typedef HRESULT_API_PTR(ProcessorProc)(const char *command, char *response,
                                       DWORD response_len,
                                       struct CommandContext *ctx);

// Function handler.
typedef HRESULT_API_PTR(CommandHandlerFunc)(struct CommandContext *ctx,
                                            char *response, DWORD response_len);

typedef struct CommandContext {
  CommandHandlerFunc handler;
  DWORD data_size;
  void *buffer;
  DWORD buffer_size;
  void *user_data;
  DWORD bytes_remaining;
} CommandContext;

// Register a new processor for commands with the given prefix.
typedef HRESULT_API_PTR(DmRegisterCommandProcessor)(const char *prefix,
                                                    ProcessorProc proc);

// Allocate a new block of memory with the given tag.
typedef PVOID_API_PTR(DmAllocatePoolWithTag)(DWORD size, DWORD tag);

// Free the given block, which was previously allocated via
// DmAllocatePoolWithTag.
typedef VOID_API_PTR(DmFreePool)(void *block);

#endif  // XBDM_GDB_BRIDGE_XBDM_H
