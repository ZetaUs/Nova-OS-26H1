bits 64

global kernel_entry
extern kernel_main

section .text
kernel_entry:
    mov rsp, 0x90008
    sub rsp, 40
    mov dword [rsp + 32], 32
    call kernel_main
.halt:
    cli
    hlt
    jmp .halt
