; Installs the command processor for bl2 commands.

bits 32

%include "macros.asm.inc"

%define PROCESSOR_NAME "bl2"

; Keep in sync with BOOTSTRAP_L2_IMPORT_TABLE_SIZE in bootstrap_l2_imports.h
%define IMPORT_TABLE_SIZE 16
%define BOOTSTRAP_L2_ENTRYPOINT_START 0x00000000
%define BOOTSTRAP_L2_IMPORT_TABLE_START 0x00000100
%define BOOTSTRAP_L2_CODE_START 0x00000180


org 0x0000
section .entry_point start=BOOTSTRAP_L2_ENTRYPOINT_START
; HRESULT entrypoint(void)
entrypoint:
            push ebp
            mov ebp, esp

            ; HRESULT DmRegisterCommandProcessor(const char *prefix, ProcessorProc proc);
            relocate_address ecx, processor
            push ecx
            relocate_address ecx, processor_prefix
            push ecx
            relocate_address ecx, DmRegisterCommandProcessorOffset
            call [ecx]

            mov esp, ebp
            pop ebp
            ret

; Because this bootstrap is generated as a flat binary, the section declarations have no actual effect. Insert nop's
; to make sure the import_table starts at the expected offset.
; The section declarations are still useful, as nasm will emit a message if sections (including manual padding) overlap.
            ;TIMES (BOOTSTRAP_L2_IMPORT_TABLE_START - $) DB 0x90
            TIMES (BOOTSTRAP_L2_IMPORT_TABLE_START - ($ - entrypoint)) DB 0x90




section .import_table_data start=BOOTSTRAP_L2_IMPORT_TABLE_START

; Keep order in sync with BootstrapL2ImportOffset in bootstrap_l2_imports.h.
import_table:
            DmAllocatePoolWithTagOffset         DD 0
            DmFreePoolOffset                    DD 0
            DmRegisterCommandProcessorOffset    DD 0
            TIMES (IMPORT_TABLE_SIZE - (($ - import_table) / 4)) DD 0



section .code start=BOOTSTRAP_L2_CODE_START

; HRESULT processor(
;       const char *command,
;       char *response,
;       DWORD response_len,
;       CommandContext *ctx)
processor:

            push ebp
            mov ebp, esp

            mov eax, 0x02DB0000

            mov esp, ebp
            pop ebp
            ret 16

            ; Set ecx to just past the point of the call.
            get_eip:
            mov ecx, [esp]
            ret



section .data
align 4
            processor_prefix  DD  PROCESSOR_NAME, 0
