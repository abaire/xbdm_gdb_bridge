#ifndef XBDM_GDB_BRIDGE_BOOTSTRAP_L2_IMPORTS_H
#define XBDM_GDB_BRIDGE_BOOTSTRAP_L2_IMPORTS_H

// Keep in sync with values in bootstrap_l2.asm
#define BOOTSTRAP_L2_IMPORT_TABLE_SIZE 16
#define BOOTSTRAP_L2_ENTRYPOINT_START 0x00000000
#define BOOTSTRAP_L2_IMPORT_TABLE_START 0x00000100
#define BOOTSTRAP_L2_CODE_START 0x00000180

// Indices of the import functions.
enum BootstrapL2ImportOffset {
  DmAllocatePoolWithTagOffset = 0,
  DmFreePoolOffset,
  DmRegisterCommandProcessorOffset,
  XeLoadSectionOffset,
  XeUnloadSectionOffset,
  MmDbgAllocateMemoryOffset,
  MmDbgFreeMemoryOffset,
};

#endif  // XBDM_GDB_BRIDGE_BOOTSTRAP_L2_IMPORTS_H
