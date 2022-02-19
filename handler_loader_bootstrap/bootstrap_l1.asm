; Trivial loader that simply allocates a fixed size block of memory, stores the address of the block in the last 4 bytes
; of the program, and returns a SUCCESS HRESULT.

bits 32

%include "macros.asm.inc"

%define BOOTSTRAP_STAGE_2_POOL '2lsB'
%define BOOTSTRAP_STAGE_2_SIZE 0x400

org 0x0000
section .text

; HRESULT bootstrap(void *DmAllocatePoolWithTagAddress)
bootstrap:

    push ebp
    mov ebp, esp

    ; The address of DmAllocatePoolWithTag
    mov edx, dword [ebp+8]

    ; DmAllocatePoolWithTag(BOOTSTRAP_STAGE_2_SIZE, BOOTSTRAP_STAGE_2_POOL);
    push BOOTSTRAP_STAGE_2_POOL
    push BOOTSTRAP_STAGE_2_SIZE
    call edx

    ; Disable memory protection to allow writing within this code segment.
    mov edx, cr0
    push edx
    and edx, 0xFFFEFFFF
    mov cr0, edx

    relocate_address ecx, result
    mov [ecx], eax

    ; Reenable memory protection
    pop edx
    mov cr0, edx

    ; Return a successful HRESULT
    mov eax, 0x02DB0000

    mov esp, ebp
    pop ebp
    ret 4

; Reserve space for the address of the allocated pool.
section .data
align 4
result DD 0
