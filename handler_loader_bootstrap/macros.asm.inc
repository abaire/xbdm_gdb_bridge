%ifndef HANDLER_LOADER_BOOTSTRAP_MACROS_ASM_INC_
%define HANDLER_LOADER_BOOTSTRAP_MACROS_ASM_INC_

; HRESULT facility field for XBOX results
%define FACILITY_XBOX 0x2db

%define S_OK (FACILITY_XBOX << 16)

; relocate_address(register, label)
; Set the given register to the absolute address of the given label.
%macro relocate_address 2
            call %%geteip
%%geteip:   pop %1
            add %1, %2 - %%geteip
%endmacro

%endif  ; HANDLER_LOADER_BOOTSTRAP_MACROS_ASM_INC_
