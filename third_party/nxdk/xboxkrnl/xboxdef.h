#ifndef __XBOXDEF_H__
#define __XBOXDEF_H__

#include <cstdint>

extern "C" {

typedef const void *LPCVOID;
typedef void VOID, *PVOID, *LPVOID;
typedef PVOID HANDLE, *PHANDLE;

typedef unsigned char BOOLEAN, *PBOOLEAN;

typedef signed char SCHAR, *PSCHAR;

typedef int8_t CHAR, *PCHAR, CCHAR, *LPCH, *PCH, OCHAR, *POCHAR;
typedef int16_t SHORT, *PSHORT;
typedef int32_t INT, *PINT, *LPINT;
typedef int32_t LONG, *PLONG, *LPLONG;
typedef int64_t LONGLONG, *PLONGLONG;

typedef uint8_t BYTE;
typedef uint8_t UCHAR, *PUCHAR;
typedef uint16_t USHORT, *PUSHORT, CSHORT;
typedef uint16_t WORD, WCHAR, *PWSTR;
typedef uint32_t UINT, *PUINT, *LPUINT;
typedef uint32_t DWORD, *PDWORD, *LPDWORD;
typedef uint32_t ULONG, *PULONG;
typedef uint64_t ULONGLONG;

typedef LONG NTSTATUS;
typedef NTSTATUS *PNTSTATUS;

#define MAXDWORD 0xFFFFFFFFUL

typedef uint32_t SIZE_T, *PSIZE_T;

typedef int32_t BOOL, *PBOOL;
typedef const char *PCSZ, *PCSTR, *LPCSTR;

typedef ULONG ULONG_PTR;
typedef LONG LONG_PTR;

typedef ULONG_PTR DWORD_PTR;

typedef struct _FLOATING_SAVE_AREA {
  WORD ControlWord;
  WORD StatusWord;
  WORD TagWord;
  WORD ErrorOpcode;
  DWORD ErrorOffset;
  DWORD ErrorSelector;
  DWORD DataOffset;
  DWORD DataSelector;
  DWORD MXCsr;
  DWORD Reserved2;
  BYTE RegisterArea[128];
  BYTE XmmRegisterArea[128];
  BYTE Reserved4[224];
  DWORD Cr0NpxState;
} __attribute__((packed)) FLOATING_SAVE_AREA, *PFLOATING_SAVE_AREA;

typedef struct _CONTEXT {
  DWORD ContextFlags;
  FLOATING_SAVE_AREA FloatSave;
  DWORD Edi;
  DWORD Esi;
  DWORD Ebx;
  DWORD Edx;
  DWORD Ecx;
  DWORD Eax;
  DWORD Ebp;
  DWORD Eip;
  DWORD SegCs;
  DWORD EFlags;
  DWORD Esp;
  DWORD SegSs;
} CONTEXT, *PCONTEXT;

#define EXCEPTION_NONCONTINUABLE 0x01
#define EXCEPTION_UNWINDING 0x02
#define EXCEPTION_EXIT_UNWIND 0x04
#define EXCEPTION_STACK_INVALID 0x08
#define EXCEPTION_NESTED_CALL 0x10
#define EXCEPTION_TARGET_UNWIND 0x20
#define EXCEPTION_COLLIDED_UNWIND 0x40
#define EXCEPTION_UNWIND                                                   \
  (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND | EXCEPTION_TARGET_UNWIND | \
   EXCEPTION_COLLIDED_UNWIND)
#define EXCEPTION_MAXIMUM_PARAMETERS 15

typedef struct _EXCEPTION_RECORD {
  NTSTATUS ExceptionCode;
  ULONG ExceptionFlags;
  struct _EXCEPTION_RECORD *ExceptionRecord;
  PVOID ExceptionAddress;
  ULONG NumberParameters;
  ULONG_PTR ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
} EXCEPTION_RECORD, *PEXCEPTION_RECORD;

typedef struct _STRING {
  USHORT Length;
  USHORT MaximumLength;
  PCHAR Buffer;
} STRING, *PSTRING;

typedef STRING ANSI_STRING, *PANSI_STRING;

/**
 * MS's way to represent a 64-bit signed int on platforms that may not support
 * them directly.
 */
typedef union _LARGE_INTEGER {
  struct {
    ULONG LowPart; /**< The low-order 32 bits. */
    LONG HighPart; /**< The high-order 32 bits. */
  };
  struct {
    ULONG LowPart; /**< The low-order 32 bits. */
    LONG HighPart; /**< The high-order 32 bits. */
  } u;
  LONGLONG QuadPart; /**< A signed 64-bit integer. */
} LARGE_INTEGER, *PLARGE_INTEGER;

/**
 * MS's way to represent a 64-bit unsigned int on platforms that may not support
 * them directly.
 */
typedef union _ULARGE_INTEGER {
  struct {
    ULONG LowPart; /**< The low-order 32 bits. */
    ULONG HighPart;
    /**< The high-order 32 bits. */ /**< The high-order 32 bits. */
  };
  struct {
    ULONG LowPart;  /**< The low-order 32 bits. */
    ULONG HighPart; /**< The high-order 32 bits. */
  } u;
  ULONGLONG QuadPart; /**< An unsigned 64-bit integer. */
} ULARGE_INTEGER, *PULARGE_INTEGER;

/**
 * Header or descriptor for an entry in a doubly linked list.
 * Initialized by InitializeListHead, members shouldn't be updated manually.
 */
typedef struct _LIST_ENTRY {
  struct _LIST_ENTRY *Flink; /**< Points to the next entry of the list or the
                                header if there is no next entry */
  struct _LIST_ENTRY *Blink; /**< Points to the previous entry of the list or
                                the header if there is no previous entry */
} LIST_ENTRY, *PLIST_ENTRY;

/**
 * Struct for modelling critical sections in the XBOX-kernel
 */
typedef struct _RTL_CRITICAL_SECTION {
  union {
    struct {
      UCHAR Type;
      UCHAR Absolute;
      UCHAR Size;
      UCHAR Inserted;
      LONG SignalState;
      LIST_ENTRY WaitListHead;
    } Event;
    ULONG RawEvent[4];
  } Synchronization;

  LONG LockCount;
  LONG RecursionCount;
  PVOID OwningThread;
} RTL_CRITICAL_SECTION, *PRTL_CRITICAL_SECTION;

/* values for FileAttributes */
#define FILE_ATTRIBUTE_READONLY 0x00000001
#define FILE_ATTRIBUTE_HIDDEN 0x00000002
#define FILE_ATTRIBUTE_SYSTEM 0x00000004
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_ARCHIVE 0x00000020
#define FILE_ATTRIBUTE_DEVICE 0x00000040
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define FILE_ATTRIBUTE_TEMPORARY 0x00000100
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFF

typedef struct _XBE_SECTION_HEADER {
  DWORD Flags;
  DWORD VirtualAddress;
  DWORD VirtualSize;
  DWORD FileAddress;
  DWORD FileSize;
  PCSZ SectionName;
  LONG SectionReferenceCount;
  WORD *HeadReferenceCount;
  WORD *TailReferenceCount;
  BYTE CheckSum[20];
} XBE_SECTION_HEADER, *PXBE_SECTION_HEADER;

}  // extern "C"

#endif
