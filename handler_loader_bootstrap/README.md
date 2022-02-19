# Handler loader

The handler loader is responsible for dynamically installing arbitrary XBDM command
processors.


The handler is injected through a multi stage bootloading process, leveraging XBDM's
`setmem` method to overwrite a host function that may then be invoked via the normal
XBDM command process.

The host function chosen is `DmResumeThread`, which is invoked more or less verbatim by the
`resume` XBDM command. This method takes a single DWORD parameter and returns an HRESULT.

## Bootstrap process

The bootstrapping is accomplished in two stages:

### L1 Loader

The L1 bootstrap simply allocates an XBDM memory block via `DmAllocatePoolWithTag`,
storing the memory address within the boot loader for retrieval via `getmem`.

### L2 Loader

The L2 loader is injected into the block allocated via the L1 loader and an accompanying
stub is injected to allow invocation.
