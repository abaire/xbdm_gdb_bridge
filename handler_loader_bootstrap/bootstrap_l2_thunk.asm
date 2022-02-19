; Calls into the L2 bootstrap, passing the address of the bootstrap as a parameter.

bits 32

org 0x0000
section .text

; HRESULT bootstrap(void *l2_bootloader_entrypoint)
bootstrap:

            ; The address of the L2 bootstrap entrypoint
            mov edx, dword [esp+4]

            ; HRESULT entrypoint(void *bootstrap_base_address);
            call edx

            ret 4
