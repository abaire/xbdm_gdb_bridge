; Trivial loader that simply allocates a fixed size block of memory, stores the address of the block in the last 4 bytes
; of the program, and returns a SUCCESS HRESULT.

bits 32

%define BOOTSTRAP_STAGE_2_POOL '2lsB'
%define BOOTSTRAP_STAGE_2_SIZE 0x400

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

; Get the relocated address of the result DWORD.
call get_eip
base_address: add ecx, result - base_address
mov [ecx], eax

; Reenable memory protection
pop edx
mov cr0, edx

; Return a successful HRESULT
mov eax, 0x02DB0000

mov esp, ebp
pop ebp
ret 4

; Set ecx to just past the point of the call.
get_eip:
mov ecx, [esp]
ret

; Reserve space for the address of the allocated pool.
align 4
result DD 0
