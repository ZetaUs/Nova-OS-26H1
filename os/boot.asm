bits 16
org 0x7C00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl
    mov si, stage2_dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_error
    jmp 0x0000:0x7E00

disk_error:
    mov si, error_message
    mov ah, 0x0E
.print:
    lodsb
    test al, al
    jz .halt
    int 0x10
    jmp .print
.halt:
    cli
    hlt
    jmp .halt

stage2_dap:
    db 0x10, 0
    dw 8
    dw 0x7E00
    dw 0
    dd 1
    dd 0

boot_drive: db 0
error_message: db 'Nova OS: boot disk error', 0

times 510-($-$$) db 0
dw 0xAA55
